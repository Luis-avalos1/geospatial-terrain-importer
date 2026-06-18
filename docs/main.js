import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

// ── Terrains (each has terrain/<name>/meta.json) ────────────────────────────────
const TERRAINS = [
  { name: 'everest',     label: 'Mt. Everest' },
  { name: 'fuji',        label: 'Mt. Fuji' },
  { name: 'grandcanyon', label: 'Grand Canyon' },
  { name: 'sf',          label: 'San Francisco' },
];

const WORLD_SPAN = 4.0;          // DEM horizontal extent in world units
const SEA_THRESHOLD_M = 5;       // show a water plane only for genuinely sea-level (coastal) terrain

// ── Renderer / scene ────────────────────────────────────────────────────────────
const canvas = document.getElementById('view');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;

const scene = new THREE.Scene();
scene.background = makeSkyTexture();
scene.fog = new THREE.Fog(0x9fb6cf, 9, 18);

const camera = new THREE.PerspectiveCamera(50, 1, 0.01, 100);
camera.position.set(5.5, 4.2, 5.5);

const controls = new OrbitControls(camera, canvas);
controls.enableDamping = true;
controls.dampingFactor = 0.08;
controls.minDistance = 1.4;
controls.maxDistance = 13;
controls.maxPolarAngle = Math.PI * 0.495;
controls.autoRotate = true;
controls.autoRotateSpeed = 0.35;
controls.enabled = false;          // enabled after the intro fly-in
controls.target.set(0, -0.1, 0);

// ── Lighting ────────────────────────────────────────────────────────────────────
const sun = new THREE.DirectionalLight(0xfff2e0, 2.2);
sun.castShadow = true;
sun.shadow.mapSize.set(2048, 2048);
sun.shadow.camera.near = 0.1;
sun.shadow.camera.far = 30;
sun.shadow.camera.left = -3; sun.shadow.camera.right = 3;
sun.shadow.camera.top = 3;   sun.shadow.camera.bottom = -3;
sun.shadow.bias = -0.0004;
sun.shadow.normalBias = 0.03;
scene.add(sun);
scene.add(sun.target);

const hemi = new THREE.HemisphereLight(0xbcd6ff, 0x33302a, 0.65);
scene.add(hemi);
const ambient = new THREE.AmbientLight(0xffffff, 0.12);
scene.add(ambient);

const texLoader = new THREE.TextureLoader();

// ── State ─────────────────────────────────────────────────────────────────────
let mesh = null;
let water = null;
let meta = null;
let heightsByLod = [];
let textures = {};
let curS = 1, curZmid = 0;       // metres->world scale and midpoint of current terrain
let state = { lod: 0, mode: 'hillshade', exag: 3, sun: 0.68 };

// ── Sky gradient as a background texture ─────────────────────────────────────────
function makeSkyTexture() {
  const c = document.createElement('canvas');
  c.width = 2; c.height = 256;
  const g = c.getContext('2d').createLinearGradient(0, 0, 0, 256);
  g.addColorStop(0.0, '#2b4d7a');
  g.addColorStop(0.45, '#5b7ea8');
  g.addColorStop(0.75, '#9fb6cf');
  g.addColorStop(1.0, '#c8d4e0');
  const ctx = c.getContext('2d');
  ctx.fillStyle = g; ctx.fillRect(0, 0, 2, 256);
  const tex = new THREE.CanvasTexture(c);
  tex.colorSpace = THREE.SRGBColorSpace;
  return tex;
}

// ── Sun position from time-of-day (0=dawn .. 0.5=noon .. 1=dusk) ────────────────
function updateSun() {
  const t = state.sun;
  const angle = (t - 0.5) * Math.PI * 1.35;          // east -> west sweep
  const alt = Math.max(0.08, Math.sin(t * Math.PI)) * (Math.PI / 2) * 0.9;
  const d = 10;
  sun.position.set(
    d * Math.cos(alt) * Math.sin(angle),
    d * Math.sin(alt),
    d * Math.cos(alt) * Math.cos(angle)
  );
  // Warmer + dimmer near the horizon.
  const high = Math.sin(t * Math.PI);                 // 0 at dawn/dusk, 1 at noon
  sun.intensity = 1.2 + high * 1.4;
  sun.color.setHSL(0.09 + high * 0.04, 0.6 - high * 0.35, 0.5 + high * 0.12);
  hemi.intensity = 0.35 + high * 0.4;
}

// ── Geometry from a heightfield ──────────────────────────────────────────────────
function buildGeometry(heights, size, spanM, zmin, zmax, exag) {
  const geo = new THREE.BufferGeometry();
  const pos = new Float32Array(size * size * 3);
  const uv = new Float32Array(size * size * 2);
  const zmid = (zmin + zmax) / 2;
  const s = WORLD_SPAN / spanM;
  curS = s; curZmid = zmid;

  for (let j = 0; j < size; j++) {
    for (let i = 0; i < size; i++) {
      const idx = j * size + i;
      pos[idx * 3 + 0] = (i / (size - 1) - 0.5) * WORLD_SPAN;
      pos[idx * 3 + 1] = (heights[idx] - zmid) * s * exag;
      pos[idx * 3 + 2] = (j / (size - 1) - 0.5) * WORLD_SPAN;
      uv[idx * 2 + 0] = i / (size - 1);
      uv[idx * 2 + 1] = 1 - j / (size - 1);
    }
  }

  const idxArr = (size * size > 65535)
    ? new Uint32Array((size - 1) * (size - 1) * 6)
    : new Uint16Array((size - 1) * (size - 1) * 6);
  let k = 0;
  for (let j = 0; j < size - 1; j++) {
    for (let i = 0; i < size - 1; i++) {
      const tl = j * size + i, tr = tl + 1, bl = tl + size, br = bl + 1;
      idxArr[k++] = tl; idxArr[k++] = bl; idxArr[k++] = tr;   // upward normals
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
    m.wireframe = true; m.map = null; m.color.set(0x76aee6);
  } else {
    m.wireframe = false; m.map = textures[state.mode]; m.color.set(0xffffff);
  }
  m.needsUpdate = true;
}

function rebuildMesh() {
  if (!meta) return;
  const size = meta.lods[state.lod].size;
  const spanM = meta.span_km * 1000;
  const geo = buildGeometry(heightsByLod[state.lod], size, spanM,
    meta.elevation_min_m, meta.elevation_max_m, state.exag);

  if (mesh) {
    mesh.geometry.dispose();
    mesh.geometry = geo;
  } else {
    const mat = new THREE.MeshStandardMaterial({ roughness: 0.95, metalness: 0.0 });
    mesh = new THREE.Mesh(geo, mat);
    mesh.castShadow = true;
    mesh.receiveShadow = true;
    scene.add(mesh);
  }
  applyMaterial();
  updateWater();
  updateStats();
}

function updateWater() {
  const seaY = (0 - curZmid) * curS * state.exag;
  const coastal = meta.elevation_min_m < SEA_THRESHOLD_M;
  if (!coastal) {
    if (water) water.visible = false;
    return;
  }
  if (!water) {
    const g = new THREE.PlaneGeometry(WORLD_SPAN * 3, WORLD_SPAN * 3);
    g.rotateX(-Math.PI / 2);
    const m = new THREE.MeshStandardMaterial({
      color: 0x274b6d, transparent: true, opacity: 0.72,
      roughness: 0.15, metalness: 0.1,
    });
    water = new THREE.Mesh(g, m);
    water.receiveShadow = true;
    scene.add(water);
  }
  water.visible = true;
  water.position.y = seaY;
}

// ── Load a terrain ───────────────────────────────────────────────────────────────
async function loadTerrain(name) {
  showLoading(true);
  const base = `terrain/${name}/`;
  meta = await (await fetch(base + 'meta.json')).json();

  heightsByLod = await Promise.all(meta.lods.map(async (l) =>
    new Float32Array(await (await fetch(base + l.file)).arrayBuffer())));

  textures = {
    hillshade: await texLoader.loadAsync(base + meta.textures.hillshade),
    colormap: await texLoader.loadAsync(base + meta.textures.colormap),
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
  document.getElementById('source').textContent =
    `${meta.source}. ${meta.location.lat.toFixed(3)}, ${meta.location.lon.toFixed(3)}.`;
  showLoading(false);
}

// ── UI ───────────────────────────────────────────────────────────────────────────
function showLoading(on) { document.getElementById('loading').classList.toggle('hidden', !on); }

function buildLocationButtons() {
  const host = document.getElementById('locations');
  TERRAINS.forEach((t, i) => {
    const b = document.createElement('button');
    b.textContent = t.label; b.dataset.name = t.name;
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

function updateStats() {
  const l = meta.lods[state.lod];
  document.getElementById('stats').innerHTML = `
    <dt>Grid</dt><dd>${l.size} &times; ${l.size}</dd>
    <dt>Triangles</dt><dd>${l.triangles.toLocaleString()}</dd>
    <dt>Ground span</dt><dd>${meta.span_km} km</dd>
    <dt>Resolution</dt><dd>${meta.metres_per_pixel} m/px</dd>
    <dt>Elevation</dt><dd>${meta.elevation_min_m}&ndash;${meta.elevation_max_m} m</dd>
    <dt>Relief</dt><dd>${meta.relief_m} m</dd>`;
}

document.getElementById('surface').addEventListener('click', (e) => {
  const b = e.target.closest('button'); if (!b) return;
  [...e.currentTarget.children].forEach((c) => c.classList.remove('active'));
  b.classList.add('active'); state.mode = b.dataset.mode; applyMaterial();
});
document.getElementById('exag').addEventListener('input', (e) => {
  state.exag = parseFloat(e.target.value);
  document.getElementById('exagval').textContent = state.exag;
  rebuildMesh();
});
document.getElementById('sun').addEventListener('input', (e) => {
  state.sun = parseFloat(e.target.value) / 100; updateSun();
});

// ── About modal ───────────────────────────────────────────────────────────────
const about = document.getElementById('about');
const openAbout = () => about.classList.remove('hidden');
const closeAbout = () => about.classList.add('hidden');
document.getElementById('intro-about').addEventListener('click', openAbout);
document.getElementById('open-about').addEventListener('click', openAbout);
document.getElementById('about-close').addEventListener('click', closeAbout);
about.addEventListener('click', (e) => { if (e.target === about) closeAbout(); });

// ── Idle auto-rotate (pause while the user interacts) ───────────────────────────
let idleTimer = null;
controls.addEventListener('start', () => {
  controls.autoRotate = false;
  if (idleTimer) clearTimeout(idleTimer);
});
controls.addEventListener('end', () => {
  if (idleTimer) clearTimeout(idleTimer);
  idleTimer = setTimeout(() => { controls.autoRotate = true; }, 6000);
});

// ── Intro -> fly-in ──────────────────────────────────────────────────────────────
let fly = null;
function enterScene() {
  document.getElementById('intro').classList.add('hidden');
  document.getElementById('titlebar').classList.remove('hidden');
  document.getElementById('panel').classList.remove('hidden');
  fly = { t: 0, from: camera.position.clone(), to: new THREE.Vector3(3.4, 2.5, 3.4) };
}
document.getElementById('enter').addEventListener('click', enterScene);

// ── Render loop ───────────────────────────────────────────────────────────────
function resize() {
  const w = window.innerWidth, h = window.innerHeight;
  renderer.setSize(w, h, false);
  camera.aspect = w / h; camera.updateProjectionMatrix();
}
window.addEventListener('resize', resize);

function animate() {
  requestAnimationFrame(animate);
  if (fly) {
    fly.t = Math.min(1, fly.t + 0.012);
    const e = 1 - Math.pow(1 - fly.t, 3);           // ease-out cubic
    camera.position.lerpVectors(fly.from, fly.to, e);
    if (fly.t >= 1) { fly = null; controls.enabled = true; }
  }
  controls.update();
  renderer.render(scene, camera);
}

// ── Boot ──────────────────────────────────────────────────────────────────────
buildLocationButtons();
updateSun();
resize();
animate();
loadTerrain(TERRAINS[0].name)
  .then(() => {
    const btn = document.getElementById('enter');
    btn.disabled = false; btn.textContent = 'Explore terrain';
  })
  .catch((err) => {
    document.getElementById('enter').textContent = 'Load failed: ' + err.message;
  });
