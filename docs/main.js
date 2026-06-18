import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

// ── Built-in terrains (each has terrain/<name>/meta.json) ───────────────────────
const TERRAINS = [
  { name: 'everest',     label: 'Mt. Everest' },
  { name: 'fuji',        label: 'Mt. Fuji' },
  { name: 'grandcanyon', label: 'Grand Canyon' },
  { name: 'sf',          label: 'San Francisco' },
];

const WORLD_SPAN = 4.0;
const SEA_THRESHOLD_M = 5;

// ── Renderer / scene ────────────────────────────────────────────────────────────
const canvas = document.getElementById('view');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;

const scene = new THREE.Scene();
scene.background = makeSkyTexture();
scene.fog = new THREE.Fog(0x9fb0c4, 9, 18);

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
controls.enabled = false;
controls.target.set(0, -0.1, 0);

// ── Lighting + shadows ──────────────────────────────────────────────────────────
const sun = new THREE.DirectionalLight(0xfff2e0, 2.2);
sun.castShadow = true;
sun.shadow.mapSize.set(2048, 2048);
sun.shadow.camera.near = 0.1; sun.shadow.camera.far = 30;
sun.shadow.camera.left = -3; sun.shadow.camera.right = 3;
sun.shadow.camera.top = 3;   sun.shadow.camera.bottom = -3;
sun.shadow.bias = -0.0004; sun.shadow.normalBias = 0.03;
scene.add(sun); scene.add(sun.target);

const hemi = new THREE.HemisphereLight(0xbcd6ff, 0x33302a, 0.65);
scene.add(hemi);
scene.add(new THREE.AmbientLight(0xffffff, 0.12));

const texLoader = new THREE.TextureLoader();

// ── State ─────────────────────────────────────────────────────────────────────
let mesh = null, water = null, meta = null;
let heightsByLod = [], textures = {};
let curS = 1, curZmid = 0;
let state = { lod: 0, mode: 'hillshade', exag: 3, sun: 0.68 };
let uploadDataset = null, uploadChip = null;
const datasetCache = new Map();   // name -> dataset (so re-switching is instant)
const inflight = new Map();        // name -> in-flight Promise (dedupe click vs preload)
const labelFor = (name) => (TERRAINS.find((t) => t.name === name) || {}).label || name;

const $ = (id) => document.getElementById(id);
const exagInput = $('exag');

// ── Sky gradient ────────────────────────────────────────────────────────────────
function makeSkyTexture() {
  const c = document.createElement('canvas'); c.width = 2; c.height = 256;
  const ctx = c.getContext('2d');
  const g = ctx.createLinearGradient(0, 0, 0, 256);
  g.addColorStop(0.0, '#243a57'); g.addColorStop(0.45, '#52708f');
  g.addColorStop(0.75, '#9fb0c4'); g.addColorStop(1.0, '#c9d2dc');
  ctx.fillStyle = g; ctx.fillRect(0, 0, 2, 256);
  const tex = new THREE.CanvasTexture(c); tex.colorSpace = THREE.SRGBColorSpace;
  return tex;
}

// ── Sun from time-of-day ─────────────────────────────────────────────────────────
function updateSun() {
  const t = state.sun;
  const angle = (t - 0.5) * Math.PI * 1.35;
  const alt = Math.max(0.08, Math.sin(t * Math.PI)) * (Math.PI / 2) * 0.9;
  const d = 10;
  sun.position.set(d * Math.cos(alt) * Math.sin(angle), d * Math.sin(alt), d * Math.cos(alt) * Math.cos(angle));
  const high = Math.sin(t * Math.PI);
  sun.intensity = 1.2 + high * 1.4;
  sun.color.setHSL(0.09 + high * 0.04, 0.6 - high * 0.35, 0.5 + high * 0.12);
  hemi.intensity = 0.35 + high * 0.4;
}

// ── Mesh ────────────────────────────────────────────────────────────────────────
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
  if (state.mode === 'wire') { m.wireframe = true; m.map = null; m.color.set(0xe0a13a); }
  else { m.wireframe = false; m.map = textures[state.mode]; m.color.set(0xffffff); }
  m.needsUpdate = true;
}

function rebuildMesh() {
  if (!meta) return;
  const size = meta.lods[state.lod].size;
  const geo = buildGeometry(heightsByLod[state.lod], size, meta.span_km * 1000,
    meta.elevation_min_m, meta.elevation_max_m, state.exag);
  if (mesh) { mesh.geometry.dispose(); mesh.geometry = geo; }
  else {
    mesh = new THREE.Mesh(geo, new THREE.MeshStandardMaterial({ roughness: 0.95, metalness: 0 }));
    mesh.castShadow = true; mesh.receiveShadow = true; scene.add(mesh);
  }
  applyMaterial(); updateWater(); updateStats();
}

function updateWater() {
  const seaY = (0 - curZmid) * curS * state.exag;
  if (meta.elevation_min_m >= SEA_THRESHOLD_M) { if (water) water.visible = false; return; }
  if (!water) {
    const g = new THREE.PlaneGeometry(WORLD_SPAN * 3, WORLD_SPAN * 3); g.rotateX(-Math.PI / 2);
    water = new THREE.Mesh(g, new THREE.MeshStandardMaterial({
      color: 0x274b6d, transparent: true, opacity: 0.72, roughness: 0.15, metalness: 0.1 }));
    water.receiveShadow = true; scene.add(water);
  }
  water.visible = true; water.position.y = seaY;
}

// ── Dataset application (shared by built-in fetch and upload) ────────────────────
function applyDataset(ds) {
  meta = ds.meta; heightsByLod = ds.heights; textures = ds.textures;
  for (const t of Object.values(textures)) {
    t.colorSpace = THREE.SRGBColorSpace;
    t.anisotropy = renderer.capabilities.getMaxAnisotropy();
  }
  state.lod = 0;
  state.exag = Math.round((meta.default_exaggeration || 3) * 2) / 2;
  exagInput.value = state.exag; $('exagval').textContent = state.exag;
  buildLodButtons(); rebuildMesh(); updateReadout();
  $('source').textContent = meta.source;
  showLoading(false);
}

// Fetch + decode a built-in terrain. No UI side effects (used for preloading too).
// Heights and both textures download in parallel; failures reject loudly.
async function fetchDataset(name) {
  const base = `terrain/${name}/`;
  const res = await fetch(base + 'meta.json');
  if (!res.ok) throw new Error(`meta.json ${res.status}`);
  const m = await res.json();
  const [heights, hillshade, colormap] = await Promise.all([
    Promise.all(m.lods.map(async (l) => {
      const r = await fetch(base + l.file);
      if (!r.ok) throw new Error(`${l.file} ${r.status}`);
      return new Float32Array(await r.arrayBuffer());
    })),
    texLoader.loadAsync(base + m.textures.hillshade),
    texLoader.loadAsync(base + m.textures.colormap),
  ]);
  return { meta: m, heights, textures: { hillshade, colormap } };
}

// Cache-aware, de-duplicated fetch: a click and a background preload of the
// same terrain share one download instead of racing two.
function getDataset(name) {
  if (datasetCache.has(name)) return Promise.resolve(datasetCache.get(name));
  if (inflight.has(name)) return inflight.get(name);
  const p = fetchDataset(name)
    .then((ds) => { datasetCache.set(name, ds); inflight.delete(name); return ds; })
    .catch((e) => { inflight.delete(name); throw e; });
  inflight.set(name, p);
  return p;
}

async function loadTerrainByName(name) {
  if (datasetCache.has(name)) { applyDataset(datasetCache.get(name)); return; }
  showLoading(true, 'Loading ' + labelFor(name));
  try {
    applyDataset(await getDataset(name));
  } finally {
    showLoading(false);   // always clears - never strands the spinner
  }
}

// Warm the cache for the other terrains, one at a time so a slow link is not
// saturated and the terrain the user actually clicks stays responsive.
async function preloadOthers(except) {
  for (const t of TERRAINS) {
    if (t.name !== except) { try { await getDataset(t.name); } catch { /* ignore */ } }
  }
}

// ── GeoTIFF upload (parsed in-browser) ──────────────────────────────────────────
function terrainColor(t) {
  const s = [[0, 46, 58, 120], [0.15, 0, 132, 200], [0.28, 40, 162, 108],
            [0.5, 232, 224, 150], [0.75, 150, 112, 80], [1, 248, 248, 250]];
  if (t <= 0) return [s[0][1], s[0][2], s[0][3]];
  for (let i = 1; i < s.length; i++) {
    if (t <= s[i][0]) {
      const a = s[i - 1], b = s[i], f = (t - a[0]) / (b[0] - a[0]);
      return [a[1] + (b[1] - a[1]) * f, a[2] + (b[2] - a[2]) * f, a[3] + (b[3] - a[3]) * f];
    }
  }
  return [s[5][1], s[5][2], s[5][3]];
}

function makeTexFromGrid(grid, T, zmin, zmax, cellM, hillshade) {
  const cv = document.createElement('canvas'); cv.width = T; cv.height = T;
  const ctx = cv.getContext('2d'); const img = ctx.createImageData(T, T); const d = img.data;
  const range = Math.max(zmax - zmin, 1e-6);
  const az = 315 * Math.PI / 180, alt = 45 * Math.PI / 180;
  const lx = Math.cos(alt) * Math.sin(az), ly = Math.cos(alt) * Math.cos(az), lz = Math.sin(alt);
  for (let j = 0; j < T; j++) {
    for (let i = 0; i < T; i++) {
      const idx = j * T + i; const t = (grid[idx] - zmin) / range;
      let [r, g, b] = terrainColor(t);
      if (hillshade) {
        const hl = grid[j * T + Math.max(i - 1, 0)], hr = grid[j * T + Math.min(i + 1, T - 1)];
        const hu = grid[Math.max(j - 1, 0) * T + i], hd = grid[Math.min(j + 1, T - 1) * T + i];
        const dzdx = (hr - hl) / (2 * cellM) * 2, dzdy = (hd - hu) / (2 * cellM) * 2;
        let nx = -dzdx, ny = dzdy, nz = 1; const L = Math.hypot(nx, ny, nz);
        let sh = (nx * lx + ny * ly + nz * lz) / L; sh = Math.max(0, sh);
        const f = 0.35 + 0.8 * sh; r *= f; g *= f; b *= f;
      }
      const o = idx * 4;
      d[o] = Math.min(255, r); d[o + 1] = Math.min(255, g); d[o + 2] = Math.min(255, b); d[o + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  const tex = new THREE.CanvasTexture(cv); tex.colorSpace = THREE.SRGBColorSpace; return tex;
}

async function ingestGeoTiff(buf, title) {
  const { fromArrayBuffer } = await import('https://cdn.jsdelivr.net/npm/geotiff@2.1.3/+esm');
  const tiff = await fromArrayBuffer(buf);
  const image = await tiff.getImage();
  const nativeW = image.getWidth(), nativeH = image.getHeight();
  // Read at a capped resolution: geotiff.js resamples on read, so even huge /
  // high-res DEMs stay fast and memory-bounded (the working array is <= 2048^2).
  const MAX_READ = 2048;
  const sc = Math.min(1, MAX_READ / Math.max(nativeW, nativeH));
  const w = Math.max(2, Math.round(nativeW * sc));
  const h = Math.max(2, Math.round(nativeH * sc));
  const src = (await image.readRasters({ samples: [0], width: w, height: h }))[0];
  const nodata = image.getGDALNoData();

  let resX = 30, latC = 0;
  try { resX = Math.abs(image.getResolution()[0]); const bb = image.getBoundingBox(); latC = (bb[1] + bb[3]) / 2; } catch (e) {}
  const geographic = resX > 0 && resX < 0.5;
  let mPerPx = geographic ? resX * 111320 * Math.cos(latC * Math.PI / 180) : resX;
  if (!isFinite(mPerPx) || mPerPx <= 0) mPerPx = 30;

  let zmin = Infinity, zmax = -Infinity;
  for (let i = 0; i < src.length; i++) {
    const v = src[i];
    if ((nodata != null && v === nodata) || !isFinite(v)) continue;
    if (v < zmin) zmin = v; if (v > zmax) zmax = v;
  }
  if (!isFinite(zmin)) { zmin = 0; zmax = 1; }

  const sample = (T) => {
    const out = new Float32Array(T * T);
    for (let j = 0; j < T; j++) {
      const sy = Math.min(h - 1, Math.round(j / (T - 1) * (h - 1)));
      for (let i = 0; i < T; i++) {
        const sx = Math.min(w - 1, Math.round(i / (T - 1) * (w - 1)));
        let v = src[sy * w + sx];
        if ((nodata != null && v === nodata) || !isFinite(v)) v = zmin;
        out[j * T + i] = v;
      }
    }
    return out;
  };

  const sizes = [256, 128, 64];
  const heights = sizes.map(sample);
  const lods = sizes.map((s, i) => ({ level: i, size: s, file: null, triangles: (s - 1) * (s - 1) * 2 }));
  const T = 512, grid = sample(T), cellM = (mPerPx * nativeW) / T;
  const textures = {
    hillshade: makeTexFromGrid(grid, T, zmin, zmax, cellM, true),
    colormap: makeTexFromGrid(grid, T, zmin, zmax, cellM, false),
  };
  const meta = {
    name: 'upload', title: title || 'Your DEM', location: { lat: latC, lon: 0 },
    source: `Uploaded GeoTIFF, ${nativeW}x${nativeH}px${sc < 1 ? ` (read at ${w}x${h})` : ''}, parsed in-browser.`,
    native_px: nativeW, metres_per_pixel: Math.round(mPerPx * 10) / 10,
    span_km: Math.round(mPerPx * Math.max(nativeW, nativeH) / 100) / 10,
    elevation_min_m: Math.round(zmin), elevation_max_m: Math.round(zmax), relief_m: Math.round(zmax - zmin),
    default_exaggeration: Math.max(2, Math.min(12, 1500 / Math.max(zmax - zmin, 1))),
    lods, textures: { hillshade: '', colormap: '' },
  };
  return { meta, heights, textures };
}

const MAX_UPLOAD_BYTES = 400 * 1024 * 1024;   // 400 MB - guard before loading into memory

async function handleUpload(file) {
  if (file.size > MAX_UPLOAD_BYTES) {
    showLoading(true, `File too large: ${Math.round(file.size / 1048576)} MB (max 400 MB)`);
    setTimeout(() => showLoading(false), 5000);
    return;
  }
  showLoading(true, 'Parsing ' + file.name);
  try {
    const ds = await ingestGeoTiff(await file.arrayBuffer(), file.name.replace(/\.[^.]+$/, ''));
    uploadDataset = ds;
    if (!uploadChip) {
      uploadChip = document.createElement('button');
      $('locations').appendChild(uploadChip);
      uploadChip.addEventListener('click', () => { selectLocation(uploadChip); applyDataset(uploadDataset); });
    }
    uploadChip.textContent = '* ' + ds.meta.title;
    selectLocation(uploadChip);
    applyDataset(ds);
  } catch (err) {
    showLoading(true, 'Upload failed: ' + (err.message || err));
    setTimeout(() => showLoading(false), 4000);
  }
}

// ── UI ───────────────────────────────────────────────────────────────────────────
function showLoading(on, msg) { const el = $('loading'); if (msg) el.textContent = msg; el.classList.toggle('hidden', !on); }

function selectLocation(btn) {
  [...$('locations').children].forEach((c) => c.classList.remove('active'));
  if (btn) btn.classList.add('active');
}

function buildLocationButtons() {
  const host = $('locations');
  TERRAINS.forEach((t, i) => {
    const b = document.createElement('button');
    b.textContent = t.label; b.dataset.name = t.name;
    if (i === 0) b.classList.add('active');
    b.addEventListener('click', () => {
      selectLocation(b); state.lod = 0;
      loadTerrainByName(t.name).catch((err) => {
        console.error(err);
        showLoading(true, 'Could not load ' + t.label + ' - check your connection');
        setTimeout(() => showLoading(false), 4500);
      });
    });
    host.appendChild(b);
  });
}

function buildLodButtons() {
  const host = $('lods'); host.innerHTML = '';
  meta.lods.forEach((l) => {
    const b = document.createElement('button');
    b.innerHTML = `LOD ${l.level}<br><small>${l.size}&sup2;</small>`;
    if (l.level === state.lod) b.classList.add('active');
    b.addEventListener('click', () => {
      state.lod = l.level;
      [...host.children].forEach((c) => c.classList.remove('active'));
      b.classList.add('active'); rebuildMesh();
    });
    host.appendChild(b);
  });
}

function updateStats() {
  const l = meta.lods[state.lod];
  $('stats').innerHTML = `
    <dt>Grid</dt><dd>${l.size} &times; ${l.size}</dd>
    <dt>Triangles</dt><dd>${l.triangles.toLocaleString()}</dd>
    <dt>Span</dt><dd>${meta.span_km} km</dd>
    <dt>Resolution</dt><dd>${meta.metres_per_pixel} m/px</dd>
    <dt>Elevation</dt><dd>${meta.elevation_min_m}&ndash;${meta.elevation_max_m} m</dd>
    <dt>Relief</dt><dd>${meta.relief_m} m</dd>`;
}

function updateReadout() {
  $('ro-title').textContent = meta.title.toUpperCase();
  if (meta.name === 'upload') $('ro-coord').textContent = `${meta.native_px}px / ${meta.metres_per_pixel} m px`;
  else $('ro-coord').textContent = `${meta.location.lat.toFixed(3)}, ${meta.location.lon.toFixed(3)}`;
}

$('surface').addEventListener('click', (e) => {
  const b = e.target.closest('button'); if (!b) return;
  [...e.currentTarget.children].forEach((c) => c.classList.remove('active'));
  b.classList.add('active'); state.mode = b.dataset.mode; applyMaterial();
});
exagInput.addEventListener('input', (e) => {
  state.exag = parseFloat(e.target.value); $('exagval').textContent = state.exag; rebuildMesh();
});
$('sun').addEventListener('input', (e) => { state.sun = parseFloat(e.target.value) / 100; updateSun(); });
$('upload-btn').addEventListener('click', () => $('upload-input').click());
$('upload-input').addEventListener('change', (e) => { if (e.target.files[0]) handleUpload(e.target.files[0]); });

// About modal
const about = $('about');
$('intro-about').addEventListener('click', () => about.classList.remove('hidden'));
$('open-about').addEventListener('click', () => about.classList.remove('hidden'));
$('about-close').addEventListener('click', () => about.classList.add('hidden'));
about.addEventListener('click', (e) => { if (e.target === about) about.classList.add('hidden'); });

// Idle auto-rotate
let idleTimer = null;
controls.addEventListener('start', () => { controls.autoRotate = false; if (idleTimer) clearTimeout(idleTimer); });
controls.addEventListener('end', () => { if (idleTimer) clearTimeout(idleTimer); idleTimer = setTimeout(() => { controls.autoRotate = true; }, 6000); });

// Intro -> fly-in
let fly = null;
$('enter').addEventListener('click', () => {
  $('intro').classList.add('hidden');
  $('titlebar').classList.remove('hidden');
  $('readout').classList.remove('hidden');
  $('panel').classList.remove('hidden');
  fly = { t: 0, from: camera.position.clone(), to: new THREE.Vector3(3.4, 2.5, 3.4) };
});

// ── Loop ──────────────────────────────────────────────────────────────────────
function resize() {
  const w = window.innerWidth, h = window.innerHeight;
  renderer.setSize(w, h, false); camera.aspect = w / h; camera.updateProjectionMatrix();
}
window.addEventListener('resize', resize);

const needle = $('needle');
function animate() {
  requestAnimationFrame(animate);
  if (fly) {
    fly.t = Math.min(1, fly.t + 0.012);
    camera.position.lerpVectors(fly.from, fly.to, 1 - Math.pow(1 - fly.t, 3));
    if (fly.t >= 1) { fly = null; controls.enabled = true; }
  }
  controls.update();
  if (needle) needle.style.transform = `translate(-50%,-100%) rotate(${-THREE.MathUtils.radToDeg(controls.getAzimuthalAngle())}deg)`;
  renderer.render(scene, camera);
}

// ── Boot ──────────────────────────────────────────────────────────────────────
buildLocationButtons();
updateSun();
resize();
animate();
loadTerrainByName(TERRAINS[0].name)
  .then(() => {
    const b = $('enter'); b.disabled = false; b.textContent = 'EXPLORE TERRAIN';
    preloadOthers(TERRAINS[0].name);   // warm the cache in the background
  })
  .catch((err) => { $('enter').textContent = 'LOAD FAILED'; console.error(err); });
