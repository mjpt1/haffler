// Petbot panel (vanilla JS, no build step)
const $ = s => document.querySelector(s);
let CSRF = null, DEV = 0, refreshTimer = null;

const MOODS = ['عادی','خوشحال','کنجکاو','خواب‌آلود','متعجب','گیج','عصبانی','ناراحت','عاشق','در حال غذا','فحش می‌دهد','حالش بد است'];
const FACES = ['😠','😄','🧐','😴','😲','😵‍💫','😡','😢','😍','😋','🤬','🤢'];
const THEMES = ['پاییز','زمستان','بهار','تابستان','هالووین','یلدا','کریسمس','نوروز'];
const DAYS = ['ش','ی','د','س','چ','پ','ج'];  // bit0 = Saturday
const KIND = {alarm:'⏰ زنگ', reminder:'🔔 یادآور', water:'💧 آب', prayer:'🕌 اذان'};

const MOOD_ACTS = [['feed','🍎 غذا بده'],['pet','🤗 نوازش'],['love','😍 عاشق شو'],['angry','😡 عصبانی شو'],['swear','🤬 فحش بده'],
  ['vomit','🤢 استفراغ'],['dizzy','😵 گیج شو'],['surprise','😲 تعجب کن'],['sleep','😴 بخواب'],['wake','☀️ بیدار شو']];
const SCREEN_ACTS = [['screen','pet','🐾 چشم‌ها'],['screen','clock','🕒 ساعت'],['screen','todo','✅ کارها'],['screen','calendar','📅 تقویم'],
  ['screen','weather','⛅ هوا'],['screen','prices','💵 قیمت‌ها'],['hafez','','📜 فال حافظ'],['screen','games','🎮 بازی'],['reboot','','🔄 ری‌استارت']];
const SOUND_ACTS = [['sound','happy','🎵 شاد'],['sound','growl','👹 غرش'],['sound','alarm','⏰ زنگ'],['sound','melody','🎶 ملودی'],['mute','1','🔇 بی‌صدا'],['mute','0','🔊 با صدا']];

function toast(t){const e=$('#toast');e.textContent=t;e.classList.add('show');clearTimeout(e._t);e._t=setTimeout(()=>e.classList.remove('show'),1800)}
function esc(s){return String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function fa(n){return String(n).replace(/\d/g,d=>'۰۱۲۳۴۵۶۷۸۹'[d])}

async function api(a, body, isForm){
  const opt = {method: body===undefined ? 'GET' : 'POST', headers:{}, credentials:'same-origin'};
  if (CSRF) opt.headers['X-CSRF'] = CSRF;
  if (body !== undefined) {
    if (isForm) opt.body = body; else { opt.headers['Content-Type']='application/json'; opt.body = JSON.stringify(body); }
  }
  const r = await fetch(`admin.php?a=${a}&d=${DEV}`, opt);
  const j = await r.json().catch(()=>({error:'خطای سرور'}));
  if (r.status === 401) { showLogin(); throw j; }
  if (j && j.error) { toast(j.error); throw j; }
  return j;
}

// ---------- login ----------
function showLogin(){ $('#app').classList.add('hidden'); $('#login').classList.remove('hidden'); }
async function doLogin(e){
  e.preventDefault();
  try { const j = await api('login', {pass: $('#pass').value}); CSRF = j.csrf; start(); }
  catch(err){ $('#loginErr').textContent = err.error || 'خطا'; }
  return false;
}
async function logout(){ await api('logout', {}); location.reload(); }

// ---------- tabs ----------
const loaders = {dash:loadDash, control:loadControl, todo:loadTodos, msg:loadMsgs, alarm:loadAlarms, habit:loadHabits,
  count:loadCounts, settings:loadSettings, fw:loadFw, device:loadDevice};
let curTab = 'dash';
document.querySelectorAll('#tabs button').forEach(b => b.onclick = () => openTab(b.dataset.tab));
function openTab(t){
  curTab = t;
  document.querySelectorAll('#tabs button').forEach(b => b.classList.toggle('on', b.dataset.tab===t));
  document.querySelectorAll('.tab').forEach(s => s.classList.toggle('hidden', s.id!==t));
  loaders[t]();
}

async function start(){
  $('#login').classList.add('hidden'); $('#app').classList.remove('hidden');
  buildStatic();
  openTab('dash');
  clearInterval(refreshTimer);
  refreshTimer = setInterval(()=>{
    if (curTab==='dash') loadDash();
    if (curTab==='control') loadCmds();
    if (curTab==='msg') loadMsgs();
  }, 5000);
}
function selectDevice(v){ DEV = +v; openTab(curTab); }

function buildStatic(){
  const mk = (box, list) => { $(box).innerHTML=''; list.forEach(x => { const b=document.createElement('button');
    b.textContent = x[x.length-1]; b.onclick = () => cmd(x[0], x.length===3 ? x[1] : ''); $(box).appendChild(b); }); };
  mk('#quick', MOOD_ACTS.slice(0,6)); mk('#moodActs', MOOD_ACTS); mk('#screenActs', SCREEN_ACTS); mk('#soundActs', SOUND_ACTS);
  $('#themeSel').innerHTML = '<option value="-1">خودکار (تقویم)</option>' + THEMES.map((t,i)=>`<option value="${i}">${t}</option>`).join('');
  attachDate($('#todoDue')); attachDate($('#cdDate')); attachTime($('#alTime'), '07:00');
  $('#alDays').innerHTML = DAYS.map((d,i)=>`<label title="${['شنبه','یکشنبه','دوشنبه','سه‌شنبه','چهارشنبه','پنجشنبه','جمعه'][i]}"><input type="checkbox" value="${i}" checked><span>${d}</span></label>`).join('');
}

async function cmd(c, arg){ await api('command', {cmd:c, arg:String(arg??'')}); toast('ارسال شد ✓'); if(curTab==='control') loadCmds(); }

// ---------- dashboard ----------
async function loadDash(){
  const j = await api('dashboard');
  const s = j.state || {};
  $('#petName').textContent = j.device.name;
  $('#online').classList.toggle('on', j.device.online);
  $('#deviceSel').innerHTML = j.devices.map(d=>`<option value="${d.id}" ${d.id==j.device.id?'selected':''}>${esc(d.name)}</option>`).join('');
  DEV = j.device.id;
  const m = +(s.mood ?? 0);
  $('#moodFace').textContent = s.asleep ? '😴' : (FACES[m] || '😠');
  $('#moodName').textContent = s.asleep ? 'خواب' : (MOODS[m] || '-');
  $('#lastSeen').textContent = j.device.online ? 'آنلاین' : (j.device.last_seen ? 'آخرین اتصال: ' + j.device.last_seen : 'هنوز وصل نشده');
  const pct = v => Math.round((+v||0)*100) + '%';
  $('#bJoy').style.width = pct(s.joy); $('#bEnergy').style.width = pct(s.energy); $('#bHunger').style.width = pct(s.hunger);
  $('#sTemp').textContent = s.temp ? fa(Math.round(s.temp)) + '°' : '-';
  $('#sFw').textContent = j.device.fw || '-';
  $('#sCoins').textContent = s.coins!=null ? fa(s.coins) : '-';
  $('#sLevel').textContent = s.level!=null ? fa(s.level) : '-';
  $('#cOpen').textContent = fa(j.counts.todos_open); $('#cDone').textContent = fa(j.counts.todos_done);
  drawChart(j.history);
  $('#cacheBox').innerHTML = j.cache.map(c => {
    let v = {}; try { v = JSON.parse(c.value) || {}; } catch(e){}
    if (c.k === 'prices') return `💵 دلار: ${esc(v.usd||'-')} · سکه: ${esc(v.coin||'-')} · طلا۱۸: ${esc(v.gold18||'-')}`;
    return `⛅ دما: ${v.temp ?? '-'}° (${v.min ?? '-'} تا ${v.max ?? '-'})`;
  }).join('<br>') || 'هنوز داده‌ای نیست';
}

function drawChart(h){
  const svg = $('#chart');
  if (!h.length) { svg.innerHTML = '<text x="150" y="55" fill="#7d8c99" font-size="10" text-anchor="middle">داده‌ای نیست</text>'; return; }
  const line = (key, col) => {
    const pts = h.map((p,i) => `${(i/(Math.max(1,h.length-1))*300).toFixed(1)},${(95 - (+p[key]||0)*90).toFixed(1)}`).join(' ');
    return `<polyline points="${pts}" fill="none" stroke="${col}" stroke-width="2" vector-effect="non-scaling-stroke"/>`;
  };
  svg.innerHTML = line('joy','#5be37d') + line('hunger','#ffc24b') + line('energy','#48e6ff');
}

// ---------- control ----------
async function loadControl(){
  const s = await api('settings');
  $('#br').value = s.brightness; $('#brVal').textContent = s.brightness; $('#themeSel').value = s.theme;
  loadCmds();
}
async function loadCmds(){
  const l = await api('commands');
  $('#cmdList').innerHTML = l.map(c => `<div class="item"><span class="t">${esc(c.cmd)} ${esc(c.arg||'')}</span>
    <span class="muted small">${c.done_at ? '✓ اجرا شد' : '⏳ در صف'}</span></div>`).join('') || '<p class="muted">-</p>';
}

// ---------- todos ----------
async function loadTodos(){
  const l = await api('todos');
  const pr = ['', '<span class="tag p1">مهم</span>', '<span class="tag p2">فوری</span>'];
  $('#todoList').innerHTML = l.map(t => `<div class="item ${+t.done?'done':''}">
    <input type="checkbox" ${+t.done?'checked':''} onchange="api('todo_update',{id:${t.id},done:this.checked?1:0}).then(loadTodos)">
    <span class="t">${esc(t.text)}</span>${pr[+t.prio]||''}${t.due?`<span class="muted small">📅 ${fmtBoth(t.due)}</span>`:''}
    <button onclick="api('todo_delete',{id:${t.id}}).then(loadTodos)">🗑</button></div>`).join('') || '<p class="muted">کاری ثبت نشده</p>';
}
async function addTodo(e){ e.preventDefault();
  await api('todo_add', {text:$('#todoText').value, prio:+$('#todoPrio').value, due:$('#todoDue').dataset.value||''});
  $('#todoText').value=''; setDateValue($('#todoDue'), ''); loadTodos(); return false; }

// ---------- messages ----------
async function loadMsgs(){
  const l = await api('messages');
  $('#msgList').innerHTML = l.map(m => `<div class="item"><span class="t">${esc(m.text)}</span>
    <span class="muted small">${m.shown_at ? '✓ دیده شد' : '⏳'}</span></div>`).join('') || '<p class="muted">پیامی نیست</p>';
}
async function sendMsg(e){ e.preventDefault(); await api('message_send', {text:$('#msgText').value}); $('#msgText').value=''; toast('ارسال شد ✓'); loadMsgs(); return false; }

// ---------- alarms ----------
async function loadAlarms(){
  const l = await api('alarms');
  $('#alarmList').innerHTML = l.map(a => `<div class="item"><b dir="ltr">${fa(esc(a.hhmm))}</b><span class="t">${KIND[a.kind]||''} ${esc(a.label)}
    <span class="muted small">${DAYS.filter((d,i)=>a.days&(1<<i)).join(' ')}</span></span>
    <input type="checkbox" ${+a.enabled?'checked':''} title="فعال" onchange="api('alarm_save',{...${esc(JSON.stringify(a))},enabled:this.checked?1:0}).then(loadAlarms)">
    <button onclick="api('alarm_delete',{id:${a.id}}).then(loadAlarms)">🗑</button></div>`).join('') || '<p class="muted">یادآوری نیست</p>';
}
async function addAlarm(e){ e.preventDefault();
  let days = 0; document.querySelectorAll('#alDays input:checked').forEach(c => days |= 1 << +c.value);
  await api('alarm_save', {hhmm:$('#alTime').dataset.value, kind:$('#alKind').value, label:$('#alLabel').value, days});
  $('#alLabel').value=''; loadAlarms(); return false; }

// ---------- habits ----------
async function loadHabits(){
  const l = await api('habits');
  const days = [...Array(14)].map((_,i) => { const d = new Date(); d.setDate(d.getDate()-13+i); return d.toISOString().slice(0,10); });
  $('#habitList').innerHTML = l.map(h => `<div class="habit"><div class="row"><b>${esc(h.title)}</b>
    <button class="btn ghost" onclick="api('habit_delete',{id:${h.id}}).then(loadHabits)">🗑</button></div>
    <div class="cells">${days.map(d => `<span title="${d}" class="${h.days.includes(d)?'on':''}" onclick="api('habit_toggle',{id:${h.id},day:'${d}'}).then(loadHabits)"></span>`).join('')}</div></div>`).join('')
    || '<p class="muted">عادتی ثبت نشده</p>';
}
async function addHabit(e){ e.preventDefault(); await api('habit_save', {title:$('#habTitle').value}); $('#habTitle').value=''; loadHabits(); return false; }

// ---------- countdowns ----------
async function loadCounts(){
  const l = await api('countdowns');
  const today = new Date(new Date().toDateString());
  $('#countList').innerHTML = l.map(c => { const n = Math.round((new Date(c.date) - today)/864e5);
    return `<div class="item"><span class="t">${esc(c.title)}<br><span class="muted small">${fmtBoth(c.date)}</span></span><b>${n>=0 ? fa(n)+' روز مانده' : 'گذشته'}</b>
    <button onclick="api('countdown_delete',{id:${c.id}}).then(loadCounts)">🗑</button></div>`; }).join('') || '<p class="muted">-</p>';
}
async function addCount(e){ e.preventDefault();
  if (!$('#cdDate').dataset.value) { toast('تاریخ را انتخاب کنید'); return false; }
  await api('countdown_save', {title:$('#cdTitle').value, date:$('#cdDate').dataset.value});
  $('#cdTitle').value=''; setDateValue($('#cdDate'), ''); loadCounts(); return false; }

// ---------- settings ----------
// time zones (POSIX TZ strings, with daylight saving where it applies)
const TZ_IR = '<+0330>-3:30', TZ_NL = 'NST3:30NDT,M3.2.0,M11.1.0', TZ_AT = 'AST4ADT,M3.2.0,M11.1.0',
  TZ_ET = 'EST5EDT,M3.2.0,M11.1.0', TZ_CT = 'CST6CDT,M3.2.0,M11.1.0', TZ_SK = 'CST6',
  TZ_MT = 'MST7MDT,M3.2.0,M11.1.0', TZ_PT = 'PST8PDT,M3.2.0,M11.1.0';
const TZS = [[TZ_IR,'ایران (+۳:۳۰)'],[TZ_NL,'کانادا · نیوفاندلند'],[TZ_AT,'کانادا · آتلانتیک (مونکتون، هلیفکس)'],
  [TZ_ET,'کانادا · شرقی (تورنتو، اتاوا، مونترال)'],[TZ_CT,'کانادا · مرکزی (وینیپگ)'],[TZ_SK,'کانادا · ساسکاچوان'],
  [TZ_MT,'کانادا · کوهستانی (کلگری، ادمونتون)'],[TZ_PT,'کانادا · اقیانوس آرام (ونکوور)']];
// city presets: [name shown on the pet, lat, lon, time zone]
const CITIES = [['پردیس',35.7378,51.8156,TZ_IR],['تهران',35.6892,51.3890,TZ_IR],['کرج',35.8400,50.9391,TZ_IR],
  ['اصفهان',32.6546,51.6680,TZ_IR],['شیراز',29.5918,52.5837,TZ_IR],['مشهد',36.2605,59.6168,TZ_IR],['تبریز',38.0800,46.2919,TZ_IR],
  ['تورنتو',43.6532,-79.3832,TZ_ET],['ونکوور',49.2827,-123.1207,TZ_PT],['مونترال',45.5019,-73.5674,TZ_ET],['اتاوا',45.4215,-75.6972,TZ_ET],
  ['کلگری',51.0447,-114.0719,TZ_MT],['ادمونتون',53.5461,-113.4938,TZ_MT],['وینیپگ',49.8951,-97.1384,TZ_CT],
  ['هلیفکس',44.6488,-63.5752,TZ_AT],['مونکتون',46.0878,-64.7782,TZ_AT]];
const opts = (list, cur) => list.map(([v,l]) => `<option value="${v}" ${String(v)===String(cur)?'selected':''}>${l}</option>`).join('');
const HOURS = [...Array(24)].map((_,i) => [i, fa(String(i).padStart(2,'0')) + ':۰۰']);

// [key, label, type, options]
const FIELDS = [
  ['pet_name','اسم پت','text'],
  ['grumpy','اخمو باشد (حالت پایه)','bool'],
  ['brightness','نور صفحه','range',[10,255]],
  ['sound','صدا روشن','bool'],
  ['buzzer','بازر برد روشن (بعد از نصب بلندگو خاموشش کنید)','bool'],
  ['volume','بلندی صدا','range',[0,100]],
  ['night_from','شروع شب','select',HOURS],
  ['night_to','پایان شب','select',HOURS],
  ['city','شهر','city'],
  ['tz','منطقه‌ی زمانی (ساعت برد)','tz'],
  ['birthday','تاریخ تولد شما','date'],
  ['sync_sec','فاصله‌ی همگام‌سازی','select',[[5,'۵ ثانیه'],[10,'۱۰ ثانیه'],[15,'۱۵ ثانیه'],[30,'۳۰ ثانیه'],[60,'۱ دقیقه']]],
  ['pomodoro_min','زمان پومودورو','select',[15,20,25,30,40,45,50,60].map(m=>[m, fa(m)+' دقیقه'])],
  ['water_every_min','یادآور آب خوردن','select',[[0,'خاموش'],[30,'هر ۳۰ دقیقه'],[45,'هر ۴۵ دقیقه'],[60,'هر ۱ ساعت'],[90,'هر ۱.۵ ساعت'],[120,'هر ۲ ساعت']]],
  ['show_prices','نمایش قیمت‌ها','bool'],
  ['show_weather','نمایش آب‌وهوا','bool'],
];
let SET = {};
async function loadSettings(){
  const s = SET = await api('settings');
  const cityIdx = CITIES.findIndex(c => c[0] === s.city || (Math.abs(c[1]-s.lat) < 0.01 && Math.abs(c[2]-s.lon) < 0.01));
  $('#setForm').innerHTML = FIELDS.map(([k,l,t,o]) => {
    if (t === 'bool') return `<label class="check"><input type="checkbox" name="${k}" ${s[k]?'checked':''}>${l}</label>`;
    if (t === 'range') return `<label>${l}</label><div class="range-row"><input type="range" name="${k}" min="${o[0]}" max="${o[1]}" value="${s[k]}"
      oninput="this.nextElementSibling.textContent=fa(this.value)"><b>${fa(s[k])}</b></div>`;
    if (t === 'select') return `<label>${l}</label><select name="${k}">${opts(o, s[k])}</select>`;
    if (t === 'city') return `<label>${l}</label><select name="city">${CITIES.map((c,i)=>`<option value="${i}" ${i===cityIdx?'selected':''}>${c[0]}</option>`).join('')}
      ${cityIdx<0?`<option value="-1" selected>${esc(s.city)} (سفارشی)</option>`:''}</select>`;
    if (t === 'date') return `<label>${l}</label><input name="${k}" class="pk-date">`;
    if (t === 'tz') return `<label>${l}</label><select name="tz">${opts(TZS, s.tz)}</select>`;
    return `<label>${l}</label><input name="${k}" value="${esc(s[k])}">`;
  }).join('') + '<button class="btn wide">ذخیره</button>';
  // choosing a city also picks its time zone
  $('#setForm').elements['city'].onchange = e => { const c = CITIES[+e.target.value]; if (c) $('#setForm').elements['tz'].value = c[3]; };
  const bd = $('#setForm').elements['birthday'];
  attachDate(bd); setDateValue(bd, s.birthday || '');
}
async function saveSettings(e){ e.preventDefault();
  const F = $('#setForm').elements, o = {};
  FIELDS.forEach(([k,,t]) => {
    const el = F[k];
    if (t === 'bool') o[k] = el.checked;
    else if (t === 'range' || t === 'select') o[k] = parseInt(el.value);
    else if (t === 'tz') o.tz = el.value;
    else if (t === 'date') o[k] = el.dataset.value || '';
    else if (t === 'city') { const i = +el.value; if (i >= 0) { o.city = CITIES[i][0]; o.lat = CITIES[i][1]; o.lon = CITIES[i][2]; } }
    else o[k] = el.value;
  });
  await api('settings_save', o); toast('ذخیره شد ✓'); return false; }

// ---------- firmware ----------
async function loadFw(){
  const l = await api('firmware');
  $('#fwList').innerHTML = l.map(f => `<div class="item"><b dir="ltr">${esc(f.version)}</b><span class="t muted small">${(f.size/1024).toFixed(0)} KB · ${esc(f.created_at)}</span>
    <button class="btn" onclick="cmd('ota','${f.id}')">نصب روی برد</button>
    <button onclick="api('firmware_delete',{id:${f.id}}).then(loadFw)">🗑</button></div>`).join('') || '<p class="muted">فایلی آپلود نشده</p>';
}
async function uploadFw(e){ e.preventDefault();
  const fd = new FormData(); fd.append('version', $('#fwVer').value); fd.append('bin', $('#fwFile').files[0]);
  await api('firmware_upload', fd, true); toast('آپلود شد ✓'); loadFw(); return false; }

// ---------- device ----------
async function loadDevice(){
  const j = await api('dashboard');
  $('#token').textContent = j.device.token;
  $('#srvUrl').textContent = location.origin + location.pathname.replace(/\/[^/]*$/, '');
}
function copyToken(){ navigator.clipboard.writeText($('#token').textContent); toast('کپی شد'); }
async function regen(){ if(!confirm('کلید فعلی باطل می‌شود و باید کلید جدید را در برد وارد کنید. ادامه؟')) return;
  const j = await api('token_regen', {}); $('#token').textContent = j.token; }
async function addDevice(e){ e.preventDefault(); const j = await api('device_add', {name:$('#newDev').value});
  alert('کلید پت جدید:\n' + j.token); loadDash(); return false; }
async function changePass(e){ e.preventDefault(); await api('password', {pass:$('#newPass').value}); $('#newPass').value=''; toast('رمز عوض شد ✓'); return false; }

// ---------- boot ----------
(async () => {
  const r = await fetch('admin.php?a=me', {credentials:'same-origin'}).then(r=>r.json()).catch(()=>({}));
  if (r.admin) { CSRF = r.csrf; start(); } else showLogin();
})();
