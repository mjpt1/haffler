// Date (Shamsi + Gregorian) and time pickers, plus Canadian holidays. No libraries.
const FA_M = ['فروردین','اردیبهشت','خرداد','تیر','مرداد','شهریور','مهر','آبان','آذر','دی','بهمن','اسفند'];
const EN_M = ['ژانویه','فوریه','مارس','آوریل','مه','ژوئن','ژوئیه','اوت','سپتامبر','اکتبر','نوامبر','دسامبر'];
const faNum = n => String(n).replace(/\d/g, d => '۰۱۲۳۴۵۶۷۸۹'[d]);
const pad2 = n => String(n).padStart(2, '0');
const isoOf = d => `${d.getFullYear()}-${pad2(d.getMonth() + 1)}-${pad2(d.getDate())}`;
const fromIso = s => { const [y, m, d] = s.split('-').map(Number); return new Date(y, m - 1, d); };
const addDays = (d, n) => { const x = new Date(d); x.setDate(x.getDate() + n); return x; };

const _pf = new Intl.DateTimeFormat('en-u-ca-persian-nu-latn', {year: 'numeric', month: 'numeric', day: 'numeric'});
function toJ(d) {  // Gregorian Date -> {y, m, d} Shamsi
  const p = {};
  _pf.formatToParts(d).forEach(x => { if (x.type !== 'literal') p[x.type] = parseInt(x.value); });
  return {y: p.year, m: p.month, d: p.day};
}

// ---------- Canadian statutory holidays ----------
function _nthDow(y, m, dow, n) { const f = new Date(y, m - 1, 1); return 1 + ((dow - f.getDay() + 7) % 7) + (n - 1) * 7; }
function _easter(y) {
  const a = y % 19, b = Math.floor(y / 100), c = y % 100, d = Math.floor(b / 4), e = b % 4, f = Math.floor((b + 8) / 25),
    g = Math.floor((b - f + 1) / 3), h = (19 * a + b - d - g + 15) % 30, i = Math.floor(c / 4), k = c % 4,
    l = (32 + 2 * e + 2 * i - h - k) % 7, m = Math.floor((a + 11 * h + 22 * l) / 451);
  return new Date(y, Math.floor((h + l - 7 * m + 114) / 31) - 1, ((h + l - 7 * m + 114) % 31) + 1);
}
const _hcache = {};
function canadaHolidays(y) {
  if (_hcache[y]) return _hcache[y];
  const h = {}, put = (m, d, n) => h[`${pad2(m)}-${pad2(d)}`] = n;
  put(1, 1, 'سال نو میلادی');
  put(2, _nthDow(y, 2, 1, 3), 'روز خانواده');
  const gf = addDays(_easter(y), -2); put(gf.getMonth() + 1, gf.getDate(), 'جمعه‌ی نیک');
  let v = new Date(y, 4, 24); while (v.getDay() !== 1) v.setDate(v.getDate() - 1); put(5, v.getDate(), 'روز ویکتوریا');
  put(7, 1, 'روز کانادا');
  put(8, _nthDow(y, 8, 1, 1), 'تعطیلی مدنی');
  put(9, _nthDow(y, 9, 1, 1), 'روز کارگر');
  put(9, 30, 'روز حقیقت و آشتی');
  put(10, _nthDow(y, 10, 1, 2), 'روز شکرگزاری');
  put(11, 11, 'روز یادبود');
  put(12, 25, 'کریسمس');
  put(12, 26, 'باکسینگ دی');
  return _hcache[y] = h;
}
function canadaHoliday(d) { return canadaHolidays(d.getFullYear())[`${pad2(d.getMonth() + 1)}-${pad2(d.getDate())}`] || null; }
function persianHoliday(d) {
  const j = toJ(d);
  if (j.m === 1 && j.d <= 4) return 'نوروز';
  if (j.m === 1 && j.d === 13) return 'سیزده‌بدر';
  if (j.m === 9 && j.d === 30) return 'شب یلدا';
  return null;
}

// "۱۴۰۵/۰۸/۰۲ · 24 اکتبر 2026"
function fmtBoth(iso) {
  if (!iso) return '';
  const d = fromIso(iso), j = toJ(d);
  return `${faNum(j.y)}/${faNum(pad2(j.m))}/${faNum(pad2(j.d))} · ${faNum(d.getDate())} ${EN_M[d.getMonth()]} ${faNum(d.getFullYear())}`;
}

// ---------- modal ----------
let _modal;
function _openModal(html, onClick) {
  if (!_modal) {
    _modal = document.createElement('div');
    _modal.className = 'pk-back';
    document.body.appendChild(_modal);
    _modal.addEventListener('click', e => { if (e.target === _modal) _closeModal(); });
  }
  _modal.innerHTML = `<div class="pk">${html}</div>`;
  _modal.onclick2 = onClick;
  _modal.querySelector('.pk').onclick = e => { const t = e.target.closest('[data-a]'); if (t) _modal.onclick2(t.dataset.a, t.dataset.v, t); };
  _modal.classList.add('show');
}
function _closeModal() { if (_modal) _modal.classList.remove('show'); }

// ---------- date picker ----------
function setDateValue(el, iso) {
  el.dataset.value = iso || '';
  el.value = iso ? fmtBoth(iso) : '';
}
function attachDate(el) {
  el.readOnly = true;
  el.classList.add('pk-input');
  el.addEventListener('click', () => openDate(el));
}
function openDate(el) {
  const st = {mode: localStorage.getItem('pkMode') || 'fa', anchor: el.dataset.value ? fromIso(el.dataset.value) : new Date()};
  const render = () => {
    const today = isoOf(new Date()), sel = el.dataset.value;
    let first, title, days = [];
    if (st.mode === 'fa') {
      const j = toJ(st.anchor);
      first = addDays(st.anchor, -(j.d - 1));
      for (let d = first; toJ(d).m === j.m; d = addDays(d, 1)) days.push(d);
      title = `${FA_M[j.m - 1]} ${faNum(j.y)}`;
    } else {
      first = new Date(st.anchor.getFullYear(), st.anchor.getMonth(), 1);
      for (let d = first; d.getMonth() === first.getMonth(); d = addDays(d, 1)) days.push(d);
      title = `${EN_M[first.getMonth()]} ${faNum(first.getFullYear())}`;
    }
    const heads = st.mode === 'fa' ? ['ش', 'ی', 'د', 'س', 'چ', 'پ', 'ج'] : ['یک', 'دو', 'سه', 'چها', 'پنج', 'جمع', 'شنب'];
    const offset = st.mode === 'fa' ? (first.getDay() + 1) % 7 : first.getDay();
    let cells = heads.map(h => `<b class="pk-h">${h}</b>`).join('') + '<i></i>'.repeat(offset);
    days.forEach(d => {
      const iso = isoOf(d), j = toJ(d);
      const hol = canadaHoliday(d) || persianHoliday(d) || (st.mode === 'fa' && d.getDay() === 5 ? 'جمعه' : null)
        || (st.mode === 'en' && d.getDay() === 0 ? 'یکشنبه' : null);
      const main = st.mode === 'fa' ? faNum(j.d) : faNum(d.getDate());
      const sub = st.mode === 'fa' ? d.getDate() : faNum(j.d);
      cells += `<button data-a="pick" data-v="${iso}" title="${hol || ''}" class="pk-d${iso === today ? ' today' : ''}${iso === sel ? ' sel' : ''}${hol ? ' hol' : ''}">
        ${main}<small>${sub}</small></button>`;
    });
    const hl = days.map(d => canadaHoliday(d) || persianHoliday(d) ? `${faNum(st.mode === 'fa' ? toJ(d).d : d.getDate())}: ${canadaHoliday(d) || persianHoliday(d)}` : null).filter(Boolean);
    _openModal(`
      <div class="pk-top">
        <button data-a="mode" data-v="fa" class="${st.mode === 'fa' ? 'on' : ''}">شمسی</button>
        <button data-a="mode" data-v="en" class="${st.mode === 'en' ? 'on' : ''}">میلادی</button>
      </div>
      <div class="pk-nav"><button data-a="prev">›</button><b>${title}</b><button data-a="next">‹</button></div>
      <div class="pk-grid" dir="${st.mode === 'fa' ? 'rtl' : 'ltr'}">${cells}</div>
      ${hl.length ? `<div class="pk-hol">${hl.join(' · ')}</div>` : ''}
      <div class="pk-foot"><button data-a="today">امروز</button><button data-a="clear">پاک کردن</button><button data-a="close">بستن</button></div>`,
      (a, v) => {
        if (a === 'mode') { st.mode = v; localStorage.setItem('pkMode', v); render(); }
        if (a === 'prev') { st.anchor = addDays(first, -1); render(); }
        if (a === 'next') { st.anchor = addDays(days[days.length - 1], 1); render(); }
        if (a === 'today') { setDateValue(el, isoOf(new Date())); _closeModal(); el.dispatchEvent(new Event('change')); }
        if (a === 'clear') { setDateValue(el, ''); _closeModal(); el.dispatchEvent(new Event('change')); }
        if (a === 'close') _closeModal();
        if (a === 'pick') { setDateValue(el, v); _closeModal(); el.dispatchEvent(new Event('change')); }
      });
  };
  render();
}

// ---------- time picker ----------
function setTimeValue(el, hhmm) { el.dataset.value = hhmm; el.value = faNum(hhmm); }
function attachTime(el, initial) {
  el.readOnly = true;
  el.classList.add('pk-input');
  setTimeValue(el, initial || el.dataset.value || '07:00');
  el.addEventListener('click', () => openTime(el));
}
function openTime(el) {
  let [h, m] = (el.dataset.value || '07:00').split(':').map(Number);
  const render = () => {
    const hrs = [...Array(24)].map((_, i) => `<button data-a="h" data-v="${i}" class="${i === h ? 'sel' : ''}">${faNum(pad2(i))}</button>`).join('');
    const mins = [...Array(12)].map((_, i) => `<button data-a="m" data-v="${i * 5}" class="${i * 5 === m ? 'sel' : ''}">${faNum(pad2(i * 5))}</button>`).join('');
    _openModal(`
      <div class="pk-time">${faNum(pad2(h))}:${faNum(pad2(m))}</div>
      <div class="pk-lbl">ساعت</div><div class="pk-tg">${hrs}</div>
      <div class="pk-lbl">دقیقه <button data-a="m1" data-v="-1" class="pk-s">−۱</button><button data-a="m1" data-v="1" class="pk-s">+۱</button></div>
      <div class="pk-tg">${mins}</div>
      <div class="pk-foot"><button data-a="ok" class="pk-ok">تأیید</button><button data-a="close">بستن</button></div>`,
      (a, v) => {
        if (a === 'h') { h = +v; render(); }
        if (a === 'm') { m = +v; render(); }
        if (a === 'm1') { m = (m + +v + 60) % 60; render(); }
        if (a === 'close') _closeModal();
        if (a === 'ok') { setTimeValue(el, `${pad2(h)}:${pad2(m)}`); _closeModal(); el.dispatchEvent(new Event('change')); }
      });
  };
  render();
}
