<?php
declare(strict_types=1);
require __DIR__ . '/lib.php';
cfg(); // redirects to install.php when not installed
?><!doctype html>
<html lang="fa" dir="rtl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0b0f14">
<title>پنل پت‌بات</title>
<link rel="stylesheet" href="assets/style.css?v=3">
</head>
<body>
<div id="login" class="center hidden">
  <form class="card login" onsubmit="return doLogin(event)">
    <div class="logo">🐾</div>
    <h1>پنل پت‌بات</h1>
    <input id="pass" type="password" placeholder="رمز ورود" autocomplete="current-password" required>
    <button class="btn wide">ورود</button>
    <p id="loginErr" class="err"></p>
  </form>
</div>

<div id="app" class="hidden">
  <header>
    <div class="brand">🐾 <span id="petName">پت‌بات</span> <span id="online" class="dot"></span></div>
    <select id="deviceSel" onchange="selectDevice(this.value)"></select>
  </header>
  <nav id="tabs">
    <button data-tab="dash" class="on">داشبورد</button>
    <button data-tab="control">کنترل</button>
    <button data-tab="todo">کارها</button>
    <button data-tab="msg">پیام</button>
    <button data-tab="alarm">یادآورها</button>
    <button data-tab="habit">عادت‌ها</button>
    <button data-tab="count">شمارش معکوس</button>
    <button data-tab="settings">تنظیمات</button>
    <button data-tab="fw">به‌روزرسانی</button>
    <button data-tab="device">دستگاه</button>
  </nav>

  <main>
    <!-- Dashboard -->
    <section id="dash" class="tab">
      <div class="grid">
        <div class="card mood"><div id="moodFace" class="face">😠</div><div id="moodName">-</div><div id="lastSeen" class="muted"></div></div>
        <div class="card">
          <h3>حال پت</h3>
          <div class="bar"><span>شادی</span><i><b id="bJoy"></b></i></div>
          <div class="bar"><span>انرژی</span><i><b id="bEnergy"></b></i></div>
          <div class="bar"><span>گرسنگی</span><i><b id="bHunger" class="warn"></b></i></div>
          <div class="row muted"><span>دمای چیپ: <b id="sTemp">-</b></span><span>نسخه: <b id="sFw">-</b></span></div>
          <div class="row muted"><span>سکه: <b id="sCoins">-</b></span><span>سطح: <b id="sLevel">-</b></span></div>
        </div>
        <div class="card"><h3>۲۴ ساعت گذشته</h3><svg id="chart" viewBox="0 0 300 100" preserveAspectRatio="none"></svg>
          <div class="legend"><i class="l1"></i>شادی <i class="l2"></i>گرسنگی <i class="l3"></i>انرژی</div></div>
        <div class="card"><h3>کارها</h3><div class="big"><span id="cOpen">0</span> باز · <span id="cDone">0</span> انجام‌شده</div>
          <h3>اطلاعات اینترنتی</h3><div id="cacheBox" class="muted small"></div></div>
      </div>
      <div class="card"><h3>دستور سریع</h3><div class="actions" id="quick"></div></div>
    </section>

    <!-- Control -->
    <section id="control" class="tab hidden">
      <div class="card"><h3>حالت‌ها</h3><div class="actions" id="moodActs"></div></div>
      <div class="card"><h3>صفحه</h3><div class="actions" id="screenActs"></div></div>
      <div class="card"><h3>صدا</h3><div class="actions" id="soundActs"></div></div>
      <div class="card">
        <h3>تم</h3>
        <select id="themeSel"></select> <button class="btn" onclick="cmd('theme', $('#themeSel').value)">اعمال</button>
        <h3>نور صفحه <span id="brVal"></span></h3>
        <input type="range" id="br" min="10" max="255" oninput="$('#brVal').textContent=this.value" onchange="cmd('brightness', this.value)">
      </div>
      <div class="card"><h3>دستورهای اخیر</h3><div id="cmdList" class="list"></div></div>
    </section>

    <!-- Todo -->
    <section id="todo" class="tab hidden">
      <form class="card add" onsubmit="return addTodo(event)">
        <input id="todoText" placeholder="کار جدید… (مثلاً خرید نان)" maxlength="200" required>
        <select id="todoPrio"><option value="0">عادی</option><option value="1">مهم</option><option value="2">فوری</option></select>
        <input id="todoDue" placeholder="موعد (اختیاری)">
        <button class="btn">افزودن</button>
      </form>
      <div class="card"><div id="todoList" class="list"></div>
        <button class="btn ghost" onclick="api('todo_clear_done',{}).then(loadTodos)">پاک کردن انجام‌شده‌ها</button></div>
    </section>

    <!-- Messages -->
    <section id="msg" class="tab hidden">
      <form class="card add" onsubmit="return sendMsg(event)">
        <input id="msgText" placeholder="پیامی که روی صفحه‌ی پت نمایش داده شود" maxlength="300" required>
        <button class="btn">ارسال</button>
      </form>
      <div class="card"><div id="msgList" class="list"></div></div>
    </section>

    <!-- Alarms -->
    <section id="alarm" class="tab hidden">
      <form class="card add" onsubmit="return addAlarm(event)">
        <input id="alTime">
        <select id="alKind"><option value="alarm">زنگ بیدارباش</option><option value="reminder">یادآور</option><option value="water">آب خوردن</option></select>
        <input id="alLabel" placeholder="عنوان (اختیاری)" maxlength="80">
        <div id="alDays" class="days"></div>
        <button class="btn">افزودن</button>
      </form>
      <div class="card"><div id="alarmList" class="list"></div></div>
    </section>

    <!-- Habits -->
    <section id="habit" class="tab hidden">
      <form class="card add" onsubmit="return addHabit(event)">
        <input id="habTitle" placeholder="عادت جدید (مثلاً ورزش، مطالعه)" maxlength="80" required>
        <button class="btn">افزودن</button>
      </form>
      <div class="card"><div id="habitList"></div></div>
    </section>

    <!-- Countdowns -->
    <section id="count" class="tab hidden">
      <form class="card add" onsubmit="return addCount(event)">
        <input id="cdTitle" placeholder="عنوان (مثلاً تولد من)" maxlength="80" required>
        <input id="cdDate" placeholder="انتخاب تاریخ">
        <button class="btn">افزودن</button>
      </form>
      <div class="card"><div id="countList" class="list"></div></div>
    </section>

    <!-- Settings -->
    <section id="settings" class="tab hidden">
      <form class="card form" id="setForm" onsubmit="return saveSettings(event)"></form>
    </section>

    <!-- Firmware -->
    <section id="fw" class="tab hidden">
      <form class="card add" onsubmit="return uploadFw(event)">
        <input id="fwVer" placeholder="شماره نسخه (مثلاً 1.2.0)" required dir="ltr">
        <input id="fwFile" type="file" accept=".bin" required>
        <button class="btn">آپلود</button>
      </form>
      <p class="muted small card">فایل <code>firmware.bin</code> را از پوشه‌ی <code>.pio/build/coderun/</code> آپلود کنید، بعد دکمه‌ی «نصب روی برد» را بزنید. برد فایل را خودش دانلود و نصب می‌کند.</p>
      <div class="card"><div id="fwList" class="list"></div></div>
    </section>

    <!-- Device -->
    <section id="device" class="tab hidden">
      <div class="card">
        <h3>کلید دستگاه</h3>
        <code id="token" class="token"></code>
        <div class="row"><button class="btn ghost" onclick="copyToken()">کپی</button>
          <button class="btn danger" onclick="regen()">ساخت کلید جدید</button></div>
        <p class="muted small">آدرس سرور برای برد: <code id="srvUrl" dir="ltr"></code></p>
      </div>
      <div class="card"><h3>افزودن پت دیگر</h3>
        <form class="add" onsubmit="return addDevice(event)"><input id="newDev" placeholder="اسم پت" required><button class="btn">افزودن</button></form></div>
      <div class="card"><h3>تغییر رمز پنل</h3>
        <form class="add" onsubmit="return changePass(event)"><input id="newPass" type="password" minlength="8" required><button class="btn">ذخیره</button></form></div>
      <button class="btn ghost" onclick="logout()">خروج</button>
    </section>
  </main>
</div>
<div id="toast"></div>
<script src="assets/picker.js?v=3"></script>
<script src="assets/app.js?v=3"></script>
</body>
</html>
