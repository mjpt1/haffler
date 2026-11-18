<?php
// Petbot panel installer (wizard).
// Step 1: check the host. Step 2: database -> writes config.php. Step 3: admin password + pet -> tables + device key.
// When done it writes install.lock and refuses to run again.
declare(strict_types=1);

$dir = __DIR__;
$lock = $dir . '/install.lock';
$cfgFile = $dir . '/config.php';

function page(string $title, string $html): void
{
    echo '<!doctype html><html lang="fa" dir="rtl"><head><meta charset="utf-8">'
        . '<meta name="viewport" content="width=device-width,initial-scale=1"><title>نصب پت‌بات</title>'
        . '<style>body{font-family:Tahoma,Vazirmatn,sans-serif;background:#0b0f14;color:#e8eef5;margin:0;padding:24px}'
        . '.box{max-width:520px;margin:auto;background:#121a23;border:1px solid #1f2b38;border-radius:16px;padding:24px}'
        . 'h1{font-size:20px;margin:0 0 6px}.steps{color:#7d8c99;font-size:13px;margin-bottom:18px}'
        . 'label{display:block;margin:12px 0 4px;color:#b8c4cf;font-size:14px}'
        . 'input{width:100%;box-sizing:border-box;padding:10px;border-radius:10px;border:1px solid #2a3a4a;background:#0b0f14;color:#fff}'
        . 'button,.btn{display:inline-block;margin-top:18px;padding:10px 22px;border:0;border-radius:10px;background:#ff4a3a;color:#fff;font-size:15px;cursor:pointer;text-decoration:none}'
        . '.ok{color:#5be37d}.bad{color:#ff6b6b}.err{background:#3a1212;border-radius:10px;padding:10px;margin:10px 0}'
        . 'code{direction:ltr;display:block;background:#0b0f14;padding:10px;border-radius:8px;word-break:break-all;margin:6px 0}'
        . 'li{margin:6px 0}</style></head><body><div class="box"><h1>🐾 نصب پنل پت‌بات</h1>'
        . '<div class="steps">' . $title . '</div>' . $html . '</div></body></html>';
    exit;
}

function e(string $s): string { return htmlspecialchars($s, ENT_QUOTES); }

if (is_file($lock)) {
    page('', '<p>پنل قبلاً نصب شده است.</p><p>برای امنیت، فایل <b>install.php</b> را از هاست پاک کنید.</p>'
        . '<a class="btn" href="index.php">ورود به پنل</a>');
}

$step = (int)($_GET['step'] ?? 1);

// ---------- Step 1: requirements ----------
if ($step === 1) {
    $checks = [
        ['PHP نسخه‌ی 7.4 یا بالاتر (' . PHP_VERSION . ')', version_compare(PHP_VERSION, '7.4', '>=')],
        ['افزونه‌ی PDO MySQL', extension_loaded('pdo_mysql')],
        ['افزونه‌ی mbstring', extension_loaded('mbstring')],
        ['اجازه‌ی نوشتن در پوشه‌ی پنل', is_writable($dir)],
        ['دسترسی به اینترنت برای آب‌وهوا و قیمت‌ها (اختیاری)', (bool)ini_get('allow_url_fopen') || function_exists('curl_init')],
    ];
    $ok = true;
    $html = '<ul>';
    foreach ($checks as $i => [$label, $pass]) {
        if (!$pass && $i < 4) $ok = false;
        $html .= '<li>' . ($pass ? '<span class="ok">✔</span> ' : '<span class="bad">✘</span> ') . e($label) . '</li>';
    }
    $html .= '</ul>';
    $html .= $ok ? '<a class="btn" href="?step=2">ادامه</a>'
        : '<p class="err">موارد قرمز را در کنترل‌پنل هاست فعال کنید و صفحه را دوباره باز کنید.</p>';
    page('مرحله ۱ از ۳: بررسی هاست', $html);
}

// ---------- Step 2: database ----------
if ($step === 2) {
    $err = '';
    $v = ['host' => 'localhost', 'name' => '', 'user' => '', 'pass' => ''];
    if ($_SERVER['REQUEST_METHOD'] === 'POST') {
        foreach ($v as $k => $_) $v[$k] = trim((string)($_POST[$k] ?? ''));
        try {
            $pdo = new PDO("mysql:host={$v['host']};dbname={$v['name']};charset=utf8mb4", $v['user'], $v['pass'],
                [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
            $pdo->query('SELECT 1');
            $config = "<?php\n// Written by install.php\nreturn " . var_export([
                'db_host' => $v['host'], 'db_name' => $v['name'], 'db_user' => $v['user'], 'db_pass' => $v['pass'],
                'enable_proxy' => true, 'timezone' => 'Asia/Tehran',
            ], true) . ";\n";
            if (file_put_contents($cfgFile, $config) === false) throw new RuntimeException('نوشتن config.php ممکن نشد.');
            @chmod($cfgFile, 0640);
            header('Location: ?step=3');
            exit;
        } catch (Throwable $ex) {
            $err = 'اتصال به دیتابیس ناموفق بود: ' . $ex->getMessage();
        }
    }
    $html = ($err ? '<div class="err">' . e($err) . '</div>' : '')
        . '<p>اول در کنترل‌پنل هاست (cPanel یا DirectAdmin) یک دیتابیس MySQL و یک کاربر بسازید و کاربر را به دیتابیس وصل کنید.</p>'
        . '<form method="post">'
        . '<label>آدرس دیتابیس</label><input name="host" value="' . e($v['host']) . '" dir="ltr">'
        . '<label>نام دیتابیس</label><input name="name" value="' . e($v['name']) . '" dir="ltr" required>'
        . '<label>نام کاربری دیتابیس</label><input name="user" value="' . e($v['user']) . '" dir="ltr" required>'
        . '<label>رمز دیتابیس</label><input name="pass" type="password" dir="ltr">'
        . '<button>اتصال و ادامه</button></form>';
    page('مرحله ۲ از ۳: دیتابیس', $html);
}

// ---------- Step 3: tables, admin, device ----------
if ($step === 3) {
    if (!is_file($cfgFile)) { header('Location: ?step=2'); exit; }
    require $dir . '/lib.php';
    $err = '';
    if ($_SERVER['REQUEST_METHOD'] === 'POST') {
        $pass = (string)($_POST['pass'] ?? '');
        $pass2 = (string)($_POST['pass2'] ?? '');
        $pet = trim((string)($_POST['pet'] ?? 'Petbot')) ?: 'Petbot';
        if (mb_strlen($pass) < 8) $err = 'رمز باید حداقل ۸ حرف باشد.';
        elseif ($pass !== $pass2) $err = 'دو رمز یکی نیستند.';
        else {
            try {
                $schema = file_get_contents($dir . '/schema.sql');
                foreach (array_filter(array_map('trim', explode(';', $schema))) as $sql) db()->exec($sql);
                q("REPLACE INTO meta (k, v) VALUES ('admin_hash', ?)", [password_hash($pass, PASSWORD_DEFAULT)]);
                $token = bin2hex(random_bytes(20));
                q('INSERT INTO devices (name, token) VALUES (?, ?)', [$pet, $token]);
                $id = (int)db()->lastInsertId();
                $s = default_settings();
                $s['pet_name'] = $pet;
                q('INSERT INTO settings (device_id, json) VALUES (?, ?)', [$id, json_encode($s, JSON_UNESCAPED_UNICODE)]);
                @mkdir($dir . '/fw', 0755);
                file_put_contents($lock, date('c'));
                $url = (empty($_SERVER['HTTPS']) ? 'http' : 'https') . '://' . $_SERVER['HTTP_HOST']
                    . rtrim(dirname($_SERVER['SCRIPT_NAME']), '/\\');
                page('نصب کامل شد ✅',
                    '<p>این دو مقدار را در صفحه‌ی تنظیم برد وارد کنید (یک بار):</p>'
                    . '<label>آدرس سرور</label><code>' . e($url) . '</code>'
                    . '<label>کلید دستگاه</label><code>' . e($token) . '</code>'
                    . '<p>کلید دستگاه را جایی امن نگه دارید. در پنل هم در بخش «دستگاه» دیده می‌شود.</p>'
                    . '<p class="bad">برای امنیت، فایل <b>install.php</b> را از هاست پاک کنید.</p>'
                    . '<a class="btn" href="index.php">ورود به پنل</a>');
            } catch (Throwable $ex) {
                $err = 'خطا: ' . $ex->getMessage();
            }
        }
    }
    $html = ($err ? '<div class="err">' . e($err) . '</div>' : '')
        . '<form method="post">'
        . '<label>اسم پت</label><input name="pet" value="Petbot">'
        . '<label>رمز ورود پنل (حداقل ۸ حرف)</label><input name="pass" type="password" minlength="8" required>'
        . '<label>تکرار رمز</label><input name="pass2" type="password" minlength="8" required>'
        . '<button>نصب</button></form>';
    page('مرحله ۳ از ۳: رمز پنل و ساخت پت', $html);
}

header('Location: ?step=1');
