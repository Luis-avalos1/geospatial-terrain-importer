import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

// ── Catalogue of available terrains (each has a meta.json under terrain/<name>/) ──
const TERRAINS = [
  { name: 'fuji', label: 'Mount Fuji' },
  { name: 'sf',   label: 'San Francisco' },
];

const WORLD_SPAN = 4.0;   // the DEM's larger horizontal extent maps to this many world units

// ── Scene setup ───────────────────────────────────────────────────────────────
const canvas = document.getElementById('view');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.outputColorSpace = THREE.SRGBColorSpace;

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x0e1116);
scene.fog = new THREE.Fog(0x0e1116, 8, 16);

const camera = new THREE.PerspectiveCamera(50, 1, 0.01, 100);
camera.position.set(3.2, 2.4, 3.2);

const controls = new OrbitControls(camera, canvas);
controls.enableDamping = true;
controls.dampingFactor = 0.08;
controls.minDistance = 1.2;
controls.maxDistance = 12;
controls.maxPolarAngle = Math.PI * 0.495;   // stay above the ground plane
controls.target.set(0, 0, 0);

// Lighting roughly matches the hillshade (NW, 45 deg above horizon).
const sun = new THREE.DirectionalLight(0xfff4e6, 2.1);
sun.position.set(-3, 4, 3);
scene.add(sun);
scene.add(new THREE.HemisphereLight(0xbcd3ff, 0x202830, 0.7));
scene.add(new THREE.AmbientLight(0xffffff, 0.15));

const texLoader = new THREE.TextureLoader();

// ── Current state ───────────────────────────────────────────────────────────────
let mesh = null;
let meta = null;
let heightsByLod = [];      // Float32Array per LOD
let textures = {};          // { hillshade, colormap }
let state = { lod: 0, mode: 'hillshade', exag: 3 };

// ── Geometry from a heightfield ─────────────────────────────────────────────────
function buildGeometry(heights, size, spanM, zmin, zmax, exag) {
  const geo = new THREE.BufferGeometry();
  const pos = new Float32Array(size * size * 3);
  const uv = new Float32Array(size * size * 2);
  const zmid = (zmin + zmax) / 2;
  const s = WORLD_SPAN / spanM;           // world units per metre (horizontal)
  const span = WORLD_SPAN;                // square DEM

  for (let j = 0; j < size; j++) {
    for (let i = 0; i < size; i++) {
      const idx = j * size + i;
      const h = heights[idx];
      pos[idx * 3 + 0] = (i / (size - 1) - 0.5) * span;             // east  (x)
      pos[idx * 3 + 1] = (h - zmid) * s * exag;                     // up    (y)
      pos[idx * 3 + 2] = (j / (size - 1) - 0.5) * span;             // south (z)
      uv[idx * 2 + 0] = i / (size - 1);
      uv[idx * 2 + 1] = 1 - j / (size - 1);
    }
  }

  // Two triangles per cell, wound so normals face up (+Y).
  const tris = (size - 1) * (size - 1) * 6;
  const idxArr = (size * size > 65535) ? new Uint32Array(tris) : new Uint16Array(tris);
  let k = 0;
  for (let j = 0; j < size - 1; j++) {
    for (let i = 0; i < size - 1; i++) {
      const tl = j * size + i, tr = tl + 1, bl = tl + size, br = bl + 1;
      idxArr[k++] = tl; idxArr[k++] = bl; idxArr[k++] = tr;
      idxArr[k++] = tr; idxArr[k++] = bl; idxArr[k++] = br;
    }
  }

  geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  geo.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
  geo.setIndex(new THREE.BufferAttribute(idxArr, 1));
  geo.computeVertexNormals();
  return geo;
}

function applyMaterial() {
  if (!mesh) return;
  const m = mesh.material;
  if (state.mode === 'wire') {
    m.wireframe = true;
    m.map = textures.hillshade;
    m.color.set(0x8fb7e0);
  } else {
    m.wireframe = false;
    m.map = textures[state.mode];
    m.color.set(0xffffff);
  }
  m.needsUpdate = true;
}

function rebuildMesh() {
  if (!meta) return;
  const size = meta.lods[state.lod].size;
  const heights = heightsByLod[state.lod];
  const spanM = meta.span_km * 1000;
  const geo = buildGeometry(heights, size, spanM, meta.elevation_min_m, meta.elevation_max_m, state.exag);

  if (mesh) {
    mesh.geometry.dispose();
    mesh.geometry = geo;
  } else {
    const mat = new THREE.MeshStandardMaterial({ roughness: 0.95, metalness: 0.0 });
    mesh = new THREE.Mesh(geo, mat);
    scene.add(mesh);
  }
  applyMaterial();
  updateStats();
}

// ── Loading a terrain ────────────────────────────────────────────────────────────
async function loadTerrain(name) {
  setLoading(true);
  const baseUrl = `terrain/${name}/`;
  meta = await (await fetch(baseUrl + 'meta.json')).json();

  heightsByLod = await Promise.all(meta.lods.map(async (l) => {
    const buf = await (await fetch(baseUrl + l.file)).arrayBuffer();
    return new Float32Array(buf);
  }));

  textures = {
    hillshade: await texLoader.loadAsync(baseUrl + meta.textures.hillshade),
    colormap:  await texLoader.loadAsync(baseUrl + meta.textures.colormap),
  };
  for (const t of Object.values(textures)) {
    t.colorSpace = THREE.SRGBColorSpace;
    t.anisotropy = renderer.capabilities.getMaxAnisotropy();
  }

  state.exag = meta.default_exaggeration || 3;
  document.getElementById('exag').value = state.exag;
  document.getElementById('exagval').textContent = state.exag;

  buildLodButtons();
  rebuildMesh();
  buildSource();
  setLoading(false);
}

// ── UI wiring ─────────────────────────────────────────────────────────────────
function setLoading(on) {
  document.getElementById('loading').classList.toggle('hidden', !on);
}

function buildLocationButtons() {
  const host = document.getElementById('locations');
  TERRAINS.forEach((t, i) => {
    const b = document.createElement('button');
    b.textContent = t.label;
    b.dataset.name = t.name;
    if (i === 0) b.classList.add('active');
    b.addEventListener('click', () => {
      [...host.children].forEach((c) => c.classList.remove('active'));
      b.classList.add('active');
      state.lod = 0;
      loadTerrain(t.name);
    });
    host.appendChild(b);
  });
}

function buildLodButtons() {
  const host = document.getElementById('lods');
  host.innerHTML = '';
  meta.lods.forEach((l) => {
    const b = document.createElement('button');
    b.innerHTML = `LOD ${l.level}<br><small>${l.size}&sup2;</small>`;
    if (l.level === state.lod) b.classList.add('active');
    b.addEventListener('click', () => {
      state.lod = l.level;
      [...host.children].forEach((c) => c.classList.remove('active'));
      b.classList.add('active');
      rebuildMesh();
    });
    host.appendChild(b);
  });
}

function buildSource() {
  document.getElementById('source').textContent =
    `Data: ${meta.source}. Location ${meta.location.lat.toFixed(3)}, ${meta.location.lon.toFixed(3)}.`;
}

document.getElementById('surface').addEventListener('click', (e) => {
  const b = e.target.closest('button');
  if (!b) return;
  [...e.currentTarget.children].forEach((c) => c.classList.remove('active'));
  b.classList.add('active');
  state.mode = b.dataset.mode;
  applyMaterial();
});

document.getElementById('exag').addEventListener('input', (e) => {
  state.exag = parseFloat(e.target.value);
  document.getElementById('exagval').textContent = state.exag;
  rebuildMesh();
});

function updateStats() {
  const l = meta.lods[state.lod];
  const tris = l.triangles.toLocaleString();
  document.getElementById('stats').innerHTML = `
    <dt>Grid</dt><dd>${l.size} &times; ${l.size}</dd>
    <dt>Triangles</dt><dd>${tris}</dd>
    <dt>Ground span</dt><dd>${meta.span_km} km</dd>
    <dt>Resolution</dt><dd>${meta.metres_per_pixel} m/px</dd>
    <dt>Elevation</dt><dd>${meta.elevation_min_m} - ${meta.elevation_max_m} m</dd>
    <dt>Relief</dt><dd>${meta.relief_m} m</dd>`;
}

// ── Render loop ───────────────────────────────────────────────────────────────
function resize() {
  const w = window.innerWidth, h = window.innerHeight;
  renderer.setSize(w, h, false);
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
}
window.addEventListener('resize', resize);

function animate() {
  requestAnimationFrame(animate);
  controls.update();
  renderer.render(scene, camera);
}

// ── Boot ──────────────────────────────────────────────────────────────────────
buildLocationButtons();
resize();
animate();
loadTerrain(TERRAINS[0].name).catch((err) => {
  document.getElementById('loading').textContent = 'Failed to load terrain: ' + err.message;
});
