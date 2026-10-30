#pragma once

// The single page the phone shows. All state lives on the ESP32 (/api/status),
// so reloading the page or the phone sleeping never loses a recording.
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WayPoint Collect</title>
<style>
  :root { --bg:#f6f7f9; --card:#fff; --ink:#1b1f24; --muted:#5f6b7a; --line:#dde2e8;
          --go:#16803c; --go-ink:#fff; --warn:#b45309; --bad:#b91c1c; --blue:#1d4ed8; --dot:#e11d48; }
  @media (prefers-color-scheme: dark) {
    :root { --bg:#111418; --card:#1b1f25; --ink:#eef1f4; --muted:#9aa5b1; --line:#2c333c;
            --go:#22a355; --warn:#f59e0b; --bad:#f87171; --blue:#60a5fa; }
  }
  * { box-sizing:border-box; }
  body { margin:0; background:var(--bg); color:var(--ink); font:17px/1.4 system-ui, sans-serif; }
  main { max-width:560px; margin:0 auto; padding:16px 16px 48px; }
  h1 { font-size:22px; margin:4px 0 12px; }
  h2 { font-size:20px; margin:0 0 4px; }
  .step { color:var(--muted); font-size:14px; font-weight:600; letter-spacing:.04em; text-transform:uppercase; }
  .muted { color:var(--muted); font-size:15px; }
  .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin:12px 0; }
  button, .btn { display:block; width:100%; font:inherit; font-weight:600; padding:16px; border-radius:12px;
                 border:1px solid var(--line); background:var(--card); color:var(--ink); text-align:center;
                 text-decoration:none; cursor:pointer; }
  button.primary { background:var(--go); color:var(--go-ink); border-color:var(--go); font-size:19px; }
  button.primary:disabled { opacity:.35; }
  button.small { display:inline-block; width:auto; padding:8px 14px; font-size:15px; }
  button.danger { color:var(--bad); }
  .tabs { display:flex; gap:8px; margin-bottom:12px; }
  .tabs button { padding:12px; }
  .tabs button.on { background:var(--ink); color:var(--bg); border-color:var(--ink); }
  .loc { display:flex; align-items:center; gap:12px; text-align:left; margin:8px 0; font-weight:400; }
  .loc .num { color:var(--muted); min-width:2.2em; font-variant-numeric:tabular-nums; }
  .loc .name { flex:1; min-width:0; overflow-wrap:anywhere; }
  .loc .name b { display:block; font-weight:600; }
  .loc .name small { color:var(--muted); }
  .badge { font-size:13px; font-weight:700; padding:4px 8px; border-radius:99px; white-space:nowrap; }
  .badge.todo { color:var(--muted); border:1px solid var(--line); }
  .badge.done { color:#fff; background:var(--go); }
  .mapbox { overflow:auto; border-radius:12px; border:1px solid var(--line); max-height:55vh; background:#fff; }
  .mapinner { position:relative; }
  .mapinner img { display:block; width:100%; }
  .dot { position:absolute; width:22px; height:22px; margin:-11px 0 0 -11px; border-radius:50%;
         background:var(--dot); border:3px solid #fff; box-shadow:0 0 0 2px var(--dot); }
  .dot::after { content:""; position:absolute; inset:-14px; border-radius:50%; border:3px solid var(--dot);
                animation:pulse 1.4s ease-out infinite; }
  @keyframes pulse { from { transform:scale(.4); opacity:1 } to { transform:scale(1.3); opacity:0 } }
  label.check { display:flex; gap:12px; align-items:center; font-weight:600; padding:14px;
                border:2px solid var(--line); border-radius:12px; margin:12px 0; }
  label.check input { width:28px; height:28px; flex:none; }
  .timer { font-size:72px; font-weight:800; text-align:center; font-variant-numeric:tabular-nums; line-height:1; margin:8px 0; }
  .turn { background:var(--warn); color:#000; font-size:26px; font-weight:800; text-align:center;
          padding:18px; border-radius:12px; margin:12px 0; }
  .still { text-align:center; font-size:20px; font-weight:700; color:var(--blue); margin:12px 0; }
  svg.compass { display:block; margin:0 auto; width:180px; height:180px; }
  .stats { display:flex; justify-content:space-around; text-align:center; color:var(--muted); font-size:14px; }
  .stats b { display:block; color:var(--ink); font-size:20px; }
  .offline { background:var(--bad); color:#fff; padding:10px 14px; border-radius:12px; margin-bottom:12px; font-size:15px; }
  .ok { font-size:56px; text-align:center; color:var(--go); line-height:1; }
  .bar { height:10px; border-radius:5px; background:var(--line); overflow:hidden; }
  .bar div { height:100%; background:var(--blue); }
  .take { display:flex; align-items:center; gap:8px; padding:8px 0; border-bottom:1px solid var(--line); }
  .take span { flex:1; }
  .hidden { display:none !important; }
  .row { display:flex; gap:8px; }
</style>
</head>
<body>
<main>
  <div id="offline" class="offline hidden">Lost connection to the device. Stay where you are: it keeps
    recording on its own (see its screen). Make sure the phone is still on the <b>WayPoint-Collect</b> WiFi.</div>

  <!-- Step 1: pick a location -->
  <section id="v-list" class="hidden">
    <div class="step">Step 1 of 3</div>
    <h1>Which spot are you scanning?</h1>
    <div class="tabs">
      <button id="f0" onclick="setFloor(0)">Ground floor</button>
      <button id="f1" onclick="setFloor(1)">First floor</button>
    </div>
    <p class="muted" id="progress"></p>
    <div id="locs"></div>
    <p><button class="small" onclick="show('data')">Saved data &amp; download</button></p>
  </section>

  <!-- Step 2: go there and confirm -->
  <section id="v-confirm" class="hidden">
    <button class="small" onclick="show('list')">&larr; Back to the list</button>
    <div class="step" style="margin-top:12px">Step 2 of 3</div>
    <h1>Walk to the red dot</h1>
    <div class="card">
      <h2 id="c-label"></h2>
      <div class="muted" id="c-file"></div>
      <div class="muted" id="c-floor"></div>
    </div>
    <div class="mapbox" id="mapbox"><div class="mapinner" id="mapinner"><img id="mapimg" alt="Floor map"><div class="dot" id="dot"></div></div></div>
    <div class="row" style="margin-top:8px">
      <button class="small" onclick="setZoom(zoom*1.5)">Zoom in</button>
      <button class="small" onclick="setZoom(zoom/1.5)">Zoom out</button>
    </div>
    <div class="card">
      <b>Before you start:</b>
      <ol style="margin:8px 0 0; padding-left:20px">
        <li>Stand <b>exactly</b> on the red dot.</li>
        <li>Hold the device flat in front of your chest, like a phone.</li>
        <li>Face any direction. Every 11 seconds it beeps: turn 45&deg; to your <b>right</b> (7 turns in all).</li>
        <li>Keep this screen on and the volume up: it beeps when to turn.</li>
      </ol>
    </div>
    <p id="c-again" class="muted hidden"></p>
    <label class="check"><input type="checkbox" id="here" onchange="$('#startbtn').disabled=!this.checked">
      I am standing on the red dot</label>
    <button class="primary" id="startbtn" disabled onclick="start()">Start 90-second scan</button>
    <p id="c-error" class="offline hidden"></p>
  </section>

  <!-- Step 3: recording -->
  <section id="v-run" class="hidden">
    <div class="step">Step 3 of 3</div>
    <h1 id="r-label"></h1>
    <div id="r-countdown" class="hidden">
      <div class="still">Get ready. Hold the device at your chest.</div>
      <div class="timer" id="r-cd"></div>
    </div>
    <div id="r-recording" class="hidden">
      <div class="timer" id="r-timer"></div>
      <div id="r-turn" class="turn hidden">&#8635; TURN RIGHT 45&deg; NOW<br><small style="font-size:16px">one-eighth of a circle, then stand still</small></div>
      <div id="r-still" class="still">Stand still</div>
      <svg class="compass" viewBox="-100 -100 200 200" id="compass"></svg>
      <p class="muted" style="text-align:center" id="r-facing"></p>
      <div class="stats"><div><b id="r-scans">0</b>scans</div><div><b id="r-aps">0</b>networks heard</div></div>
      <p style="margin-top:24px"><button class="danger" onclick="cancelRun()">Cancel this recording</button></p>
    </div>
    <div id="r-done" class="hidden">
      <div class="ok">&#10004;</div>
      <h2 style="text-align:center">Saved</h2>
      <p class="muted" style="text-align:center" id="r-donetext"></p>
      <button class="primary" onclick="ack()">Next location</button>
    </div>
    <div id="r-error" class="hidden">
      <div class="offline" id="r-errtext"></div>
      <button onclick="ack()">OK</button>
    </div>
  </section>

  <!-- Data -->
  <section id="v-data" class="hidden">
    <button class="small" onclick="show('list')">&larr; Back</button>
    <h1 style="margin-top:12px">Saved data</h1>
    <div class="card">
      <div class="muted" id="d-space"></div>
      <div class="bar" style="margin-top:6px"><div id="d-bar"></div></div>
    </div>
    <a class="btn" href="/download.csv" download>Download all (CSV)</a>
    <p class="muted">Download from a laptop or a phone browser (not the pop-up sign-in window).</p>
    <div id="d-takes"></div>
  </section>
</main>

<script>
const $ = s => document.querySelector(s);
let L = null, S = null, view = 'list', floor = 0, sel = null, zoom = 2.5;
let endAt = 0, lastHeading = -1, cueAt = -1e9, fails = 0, audio = null, busy = false;

async function api(path, method = 'GET') {
  const abort = new AbortController();
  const timer = setTimeout(() => abort.abort(), 5000);
  try {
    const r = await fetch(path, { method, signal: abort.signal, cache: 'no-store' });
    return await r.json();
  } finally { clearTimeout(timer); }
}

function show(v) {
  view = v;
  if (v === 'data') loadTakes();
  render();
}

function render() {
  if (!L || !S) return;
  const running = S.state !== 'idle';
  const v = running ? 'run' : view;
  for (const id of ['list', 'confirm', 'run', 'data']) $('#v-' + id).classList.toggle('hidden', id !== v);
  if (v === 'list') renderList();
  if (v === 'run') renderRun();
  if (v === 'data') renderSpace();
}

function setFloor(f) { floor = f; render(); }

function renderList() {
  $('#f0').classList.toggle('on', floor === 0);
  $('#f1').classList.toggle('on', floor === 1);
  const done = S.takes.filter(n => n > 0).length;
  $('#progress').textContent = `${done} of ${L.locations.length} spots scanned so far. Pick the one you are at.`;
  const box = $('#locs');
  box.innerHTML = '';
  for (const l of L.locations.filter(l => l.floor === floor)) {
    const n = S.takes[l.i];
    const b = document.createElement('button');
    b.className = 'loc';
    b.innerHTML = `<span class="num">#${l.id}</span><span class="name"><b></b><small></small></span>` +
      (n ? `<span class="badge done">&#10004; done${n > 1 ? ' x' + n : ''}</span>` : '<span class="badge todo">to do</span>');
    b.querySelector('b').textContent = l.label;
    b.querySelector('small').textContent = l.file.replace(/^\d+_/, '');
    b.onclick = () => pick(l);
    box.appendChild(b);
  }
}

function pick(l) {
  sel = l;
  $('#c-label').textContent = l.label;
  $('#c-file').textContent = 'Phone recording: ' + l.file;
  $('#c-floor').textContent = l.floor ? 'First floor' : 'Ground floor';
  const n = S.takes[l.i];
  $('#c-again').textContent = n ? `Already scanned here ${n} time(s). Scanning again adds another take (that is fine).` : '';
  $('#c-again').classList.toggle('hidden', !n);
  $('#here').checked = false;
  $('#startbtn').disabled = true;
  $('#c-error').classList.add('hidden');
  $('#mapimg').src = `/map${l.floor}.jpg`;
  $('#dot').style.left = (100 * l.x / L.map_w) + '%';
  $('#dot').style.top = (100 * l.y / L.map_h) + '%';
  show('confirm');
  window.scrollTo(0, 0);
  setZoom(zoom);
}

function setZoom(z) {
  zoom = Math.min(6, Math.max(1, z));
  $('#mapinner').style.width = (zoom * 100) + '%';
  const box = $('#mapbox'), center = () => {
    const w = box.scrollWidth, h = box.scrollHeight;
    box.scrollLeft = w * sel.x / L.map_w - box.clientWidth / 2;
    box.scrollTop = h * sel.y / L.map_h - box.clientHeight / 2;
  };
  const img = $('#mapimg');
  img.complete ? center() : (img.onload = center);
}

function beep() {
  try {
    const o = audio.createOscillator(), g = audio.createGain();
    o.frequency.value = 880; o.connect(g); g.connect(audio.destination);
    g.gain.setValueAtTime(0.4, audio.currentTime);
    o.start(); o.stop(audio.currentTime + 0.25);
    setTimeout(() => { const o2 = audio.createOscillator(); o2.frequency.value = 1175; o2.connect(g); o2.start(); o2.stop(audio.currentTime + 0.35); }, 330);
  } catch (e) {}
}

async function start() {
  try { audio = audio || new (window.AudioContext || window.webkitAudioContext)(); audio.resume(); } catch (e) {}
  $('#startbtn').disabled = true;
  const epoch = Date.now() - new Date().getTimezoneOffset() * 60000;  // local time, like the phone CSVs
  try {
    const r = await api(`/api/start?i=${sel.i}&epoch=${epoch}`, 'POST');
    if (!r.ok) throw new Error(r.error);
    lastHeading = -1;
    await poll();
  } catch (e) {
    $('#c-error').textContent = 'Could not start: ' + (e.message || 'no answer from the device');
    $('#c-error').classList.remove('hidden');
    $('#startbtn').disabled = false;
  }
}

async function cancelRun() {
  if (!confirm('Stop and throw away this recording?')) return;
  await api('/api/cancel', 'POST').catch(() => {});
  view = sel ? 'confirm' : 'list';
  await poll();
}

async function ack() {
  await api('/api/ack', 'POST').catch(() => {});
  view = 'list';
  if (sel) floor = sel.floor;
  await poll();
}

function renderRun() {
  const l = L.locations[S.loc];
  if (l) { sel = l; $('#r-label').textContent = l.label; }
  for (const s of ['countdown', 'recording', 'done', 'error']) $('#r-' + s).classList.toggle('hidden', S.state !== s);
  $('#r-scans').textContent = S.scans;
  $('#r-aps').textContent = S.aps;
  $('#r-donetext').textContent = `${S.scans} scans saved for ${l ? l.label : 'this spot'}.`;
  $('#r-errtext').textContent = S.error;
  tick();
}

function drawCompass(h) {
  const n = L.headings;
  let svg = '<circle r="88" fill="none" stroke="currentColor" stroke-opacity=".2" stroke-width="2"/>';
  for (let i = 0; i < n; i++) {
    const a = (i * 360 / n - 90) * Math.PI / 180, x = 72 * Math.cos(a), y = 72 * Math.sin(a);
    const fill = i < h ? 'var(--go)' : i === h ? 'var(--blue)' : 'none';
    svg += `<circle cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="11" fill="${fill}" stroke="currentColor" stroke-opacity=".4"/>`;
  }
  svg += `<g transform="rotate(${h * 360 / n})"><path d="M0,-55 L16,-20 L5,-20 L5,20 L-5,20 L-5,-20 L-16,-20 Z" fill="var(--blue)"/></g>`;
  svg += '<circle r="6" fill="currentColor"/>';
  $('#compass').innerHTML = svg;
}

// Runs every 200 ms: smooth timer and turn cues between status polls.
function tick() {
  if (!S || !L) return;
  const left = Math.max(0, endAt - performance.now());
  if (S.state === 'countdown') $('#r-cd').textContent = Math.ceil(left / 1000);
  if (S.state !== 'recording') return;
  const s = Math.ceil(left / 1000);
  $('#r-timer').textContent = Math.floor(s / 60) + ':' + String(s % 60).padStart(2, '0');
  const h = Math.min(L.headings - 1, Math.floor((L.session_ms - left) * L.headings / L.session_ms));
  if (h !== lastHeading) {
    if (lastHeading >= 0 && h > lastHeading) {
      cueAt = performance.now();
      beep();
      if (navigator.vibrate) navigator.vibrate([400, 150, 400]);
    }
    lastHeading = h;
    drawCompass(h);
  }
  const turning = performance.now() - cueAt < 3000;
  $('#r-turn').classList.toggle('hidden', !turning);
  $('#r-still').classList.toggle('hidden', turning);
  $('#r-facing').textContent = `Direction ${h + 1} of ${L.headings}` +
    (h < L.headings - 1 ? '' : ' (last one)');
}

async function poll() {
  if (busy) return;
  busy = true;
  try {
    if (!L) L = await api('/api/locations');
    S = await api('/api/status');
    fails = 0;
    if (S.state === 'countdown' || S.state === 'recording') endAt = performance.now() + S.ms_left;
    render();
  } catch (e) {
    fails++;
  } finally { busy = false; }
  $('#offline').classList.toggle('hidden', fails < 3);
}

function renderSpace() {
  const used = S.fs_used / 1024, total = S.fs_total / 1024;
  $('#d-space').textContent = `Flash used: ${used.toFixed(0)} KB of ${total.toFixed(0)} KB (about ${Math.max(0, Math.floor((total - used) / 40))} more spots fit)`;
  $('#d-bar').style.width = (100 * used / total) + '%';
}

async function loadTakes() {
  const box = $('#d-takes');
  box.textContent = 'Loading...';
  try {
    const r = await api('/api/takes');
    box.innerHTML = r.takes.length ? '' : '<p class="muted">Nothing recorded yet.</p>';
    for (const t of r.takes) {
      const row = document.createElement('div');
      row.className = 'take';
      row.innerHTML = '<span></span><a class="small btn" style="width:auto;padding:8px 12px">CSV</a><button class="small danger">Delete</button>';
      row.querySelector('span').textContent = `#${t.id} ${t.label}, take ${t.take} (${t.kb} KB)`;
      row.querySelector('a').href = '/download.csv?id=' + t.id;
      row.querySelector('button').onclick = async () => {
        if (!confirm(`Delete take ${t.take} of ${t.label}? This cannot be undone.`)) return;
        await api('/api/delete?name=' + t.name, 'POST').catch(() => {});
        await poll();
        loadTakes();
      };
      box.appendChild(row);
    }
  } catch (e) { box.textContent = 'Could not load the list.'; }
}

poll();
setInterval(poll, 700);
setInterval(tick, 200);
</script>
</body>
</html>
)HTML";
