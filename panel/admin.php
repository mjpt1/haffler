<?php
// Admin JSON API used by index.php (session login + CSRF header).
declare(strict_types=1);
require __DIR__ . '/lib.php';

$a = $_GET['a'] ?? '';
$b = body();

// ---------- login (no auth needed) ----------
if ($a === 'login') {
    session_start_safe();
    $tries = $_SESSION['tries'] ?? 0;
    if ($tries >= 5 && time() - ($_SESSION['last_try'] ?? 0) < 300) json_out(['error' => 'چند دقیقه صبر کنید'], 429);
    $hash = q("SELECT v FROM meta WHERE k = 'admin_hash'")->fetchColumn();
    if ($hash && password_verify((string)($b['pass'] ?? ''), $hash)) {
        session_regenerate_id(true);
        $_SESSION['admin'] = true;
        $_SESSION['tries'] = 0;
        json_out(['ok' => true, 'csrf' => csrf()]);
    }
    $_SESSION['tries'] = $tries + 1;
    $_SESSION['last_try'] = time();
    json_out(['error' => 'رمز اشتباه است'], 401);
}
if ($a === 'me') json_out(['admin' => is_admin(), 'csrf' => is_admin() ? csrf() : null]);

require_admin();

$dev = q('SELECT * FROM devices WHERE id = ?', [(int)($_GET['d'] ?? 0)])->fetch()
    ?: q('SELECT * FROM devices ORDER BY id LIMIT 1')->fetch();
if (!$dev) json_out(['error' => 'no device'], 404);
$id = (int)$dev['id'];
$now = date('Y-m-d H:i:s');

switch ($a) {
    case 'logout':
        $_SESSION = [];
        session_destroy();
        json_out(['ok' => true]);

    case 'dashboard':
        $state = json_decode($dev['state_json'] ?? '{}', true) ?: [];
        $online = $dev['last_seen'] && time() - strtotime($dev['last_seen']) < 90;
        json_out([
            'device' => ['id' => $id, 'name' => $dev['name'], 'last_seen' => $dev['last_seen'], 'online' => $online,
                'fw' => $dev['fw'], 'ip' => $dev['ip'], 'token' => $dev['token']],
            'devices' => q('SELECT id, name FROM devices ORDER BY id')->fetchAll(),
            'state' => $state,
            'history' => q('SELECT ts, mood, joy, hunger, energy, temp FROM history WHERE device_id = ? AND ts > NOW() - INTERVAL 1 DAY ORDER BY ts', [$id])->fetchAll(),
            'counts' => [
                'todos_open' => (int)q('SELECT COUNT(*) FROM todos WHERE device_id = ? AND done = 0', [$id])->fetchColumn(),
                'todos_done' => (int)q('SELECT COUNT(*) FROM todos WHERE device_id = ? AND done = 1', [$id])->fetchColumn(),
                'cmd_pending' => (int)q('SELECT COUNT(*) FROM commands WHERE device_id = ? AND done_at IS NULL', [$id])->fetchColumn(),
            ],
            'cache' => q("SELECT k, value, updated_at FROM cache WHERE k IN ('prices') OR k LIKE 'wx_%' ORDER BY updated_at DESC LIMIT 3")->fetchAll(),
        ]);

    // ---------- todos ----------
    case 'todos':
        json_out(q('SELECT * FROM todos WHERE device_id = ? ORDER BY done, prio DESC, id DESC', [$id])->fetchAll());
    case 'todo_add':
        $t = trim((string)($b['text'] ?? ''));
        if ($t === '') json_out(['error' => 'empty'], 400);
        q('INSERT INTO todos (device_id, text, prio, due, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?)',
            [$id, mb_substr($t, 0, 200), (int)($b['prio'] ?? 0), ($b['due'] ?? '') ?: null, $now, $now]);
        bump($id);
        json_out(['ok' => true]);
    case 'todo_update':
        q('UPDATE todos SET text = COALESCE(?, text), done = COALESCE(?, done), prio = COALESCE(?, prio), updated_at = ? WHERE id = ? AND device_id = ?',
            [isset($b['text']) ? mb_substr((string)$b['text'], 0, 200) : null, isset($b['done']) ? (int)$b['done'] : null,
                isset($b['prio']) ? (int)$b['prio'] : null, $now, (int)$b['id'], $id]);
        bump($id);
        json_out(['ok' => true]);
    case 'todo_delete':
        q('DELETE FROM todos WHERE id = ? AND device_id = ?', [(int)$b['id'], $id]);
        bump($id);
        json_out(['ok' => true]);
    case 'todo_clear_done':
        q('DELETE FROM todos WHERE device_id = ? AND done = 1', [$id]);
        bump($id);
        json_out(['ok' => true]);

    // ---------- messages + commands ----------
    case 'messages':
        json_out(q('SELECT * FROM messages WHERE device_id = ? ORDER BY id DESC LIMIT 50', [$id])->fetchAll());
    case 'message_send':
        $t = trim((string)($b['text'] ?? ''));
        if ($t === '') json_out(['error' => 'empty'], 400);
        q('INSERT INTO messages (device_id, text, created_at) VALUES (?, ?, ?)', [$id, mb_substr($t, 0, 300), $now]);
        json_out(['ok' => true]);
    case 'command':
        $cmd = preg_replace('/[^a-z_]/', '', (string)($b['cmd'] ?? ''));
        if ($cmd === '') json_out(['error' => 'empty'], 400);
        q('INSERT INTO commands (device_id, cmd, arg, created_at) VALUES (?, ?, ?, ?)',
            [$id, $cmd, mb_substr((string)($b['arg'] ?? ''), 0, 255), $now]);
        json_out(['ok' => true]);
    case 'commands':
        json_out(q('SELECT * FROM commands WHERE device_id = ? ORDER BY id DESC LIMIT 30', [$id])->fetchAll());

    // ---------- settings ----------
    case 'settings':
        json_out(device_settings($id));
    case 'settings_save':
        $cur = device_settings($id);
        foreach (default_settings() as $k => $def) {
            if (!array_key_exists($k, $b)) continue;
            $v = $b[$k];
            if (is_bool($def)) $v = (bool)$v;
            elseif (is_int($def)) $v = (int)$v;
            elseif (is_float($def)) $v = (float)$v;
            else $v = mb_substr((string)$v, 0, 64);
            $cur[$k] = $v;
        }
        q('REPLACE INTO settings (device_id, json) VALUES (?, ?)', [$id, json_encode($cur, JSON_UNESCAPED_UNICODE)]);
        if (isset($b['pet_name'])) q('UPDATE devices SET name = ? WHERE id = ?', [$cur['pet_name'], $id]);
        bump($id);
        json_out(['ok' => true]);

    // ---------- alarms / reminders ----------
    case 'alarms':
        json_out(q('SELECT * FROM alarms WHERE device_id = ? ORDER BY hhmm', [$id])->fetchAll());
    case 'alarm_save':
        $hh = preg_match('/^\d{2}:\d{2}$/', (string)($b['hhmm'] ?? '')) ? $b['hhmm'] : '07:00';
        $kind = in_array($b['kind'] ?? '', ['alarm', 'reminder', 'water', 'prayer'], true) ? $b['kind'] : 'alarm';
        if (!empty($b['id'])) {
            q('UPDATE alarms SET hhmm = ?, days = ?, label = ?, kind = ?, enabled = ? WHERE id = ? AND device_id = ?',
                [$hh, (int)($b['days'] ?? 127), mb_substr((string)($b['label'] ?? ''), 0, 80), $kind, empty($b['enabled']) ? 0 : 1, (int)$b['id'], $id]);
        } else {
            q('INSERT INTO alarms (device_id, hhmm, days, label, kind) VALUES (?, ?, ?, ?, ?)',
                [$id, $hh, (int)($b['days'] ?? 127), mb_substr((string)($b['label'] ?? ''), 0, 80), $kind]);
        }
        bump($id);
        json_out(['ok' => true]);
    case 'alarm_delete':
        q('DELETE FROM alarms WHERE id = ? AND device_id = ?', [(int)$b['id'], $id]);
        bump($id);
        json_out(['ok' => true]);

    // ---------- countdowns ----------
    case 'countdowns':
        json_out(q('SELECT * FROM countdowns WHERE device_id = ? ORDER BY date', [$id])->fetchAll());
    case 'countdown_save':
        q('INSERT INTO countdowns (device_id, title, date) VALUES (?, ?, ?)',
            [$id, mb_substr((string)($b['title'] ?? ''), 0, 80), (string)($b['date'] ?? date('Y-m-d'))]);
        bump($id);
        json_out(['ok' => true]);
    case 'countdown_delete':
        q('DELETE FROM countdowns WHERE id = ? AND device_id = ?', [(int)$b['id'], $id]);
        bump($id);
        json_out(['ok' => true]);

    // ---------- habits ----------
    case 'habits':
        $rows = q('SELECT * FROM habits WHERE device_id = ? ORDER BY id', [$id])->fetchAll();
        foreach ($rows as &$h) {
            $h['days'] = q('SELECT day FROM habit_log WHERE habit_id = ? AND day > CURDATE() - INTERVAL 14 DAY', [$h['id']])->fetchAll(PDO::FETCH_COLUMN);
        }
        json_out($rows);
    case 'habit_save':
        q('INSERT INTO habits (device_id, title) VALUES (?, ?)', [$id, mb_substr((string)($b['title'] ?? ''), 0, 80)]);
        bump($id);
        json_out(['ok' => true]);
    case 'habit_toggle':
        $hid = (int)$b['id'];
        $day = (string)($b['day'] ?? date('Y-m-d'));
        if (q('SELECT 1 FROM habits WHERE id = ? AND device_id = ?', [$hid, $id])->fetch()) {
            if (q('SELECT 1 FROM habit_log WHERE habit_id = ? AND day = ?', [$hid, $day])->fetch())
                q('DELETE FROM habit_log WHERE habit_id = ? AND day = ?', [$hid, $day]);
            else q('INSERT INTO habit_log (habit_id, day) VALUES (?, ?)', [$hid, $day]);
        }
        bump($id);
        json_out(['ok' => true]);
    case 'habit_delete':
        q('DELETE FROM habit_log WHERE habit_id = ?', [(int)$b['id']]);
        q('DELETE FROM habits WHERE id = ? AND device_id = ?', [(int)$b['id'], $id]);
        bump($id);
        json_out(['ok' => true]);

    // ---------- firmware (OTA) ----------
    case 'firmware':
        json_out(q('SELECT * FROM firmware ORDER BY id DESC')->fetchAll());
    case 'firmware_upload':
        $f = $_FILES['bin'] ?? null;
        $ver = preg_replace('/[^0-9A-Za-z._-]/', '', (string)($_POST['version'] ?? ''));
        if (!$f || $f['error'] !== UPLOAD_ERR_OK || $ver === '') json_out(['error' => 'فایل یا نسخه نامعتبر است'], 400);
        $head = file_get_contents($f['tmp_name'], false, null, 0, 1);
        if ($head !== "\xE9") json_out(['error' => 'این فایل firmware.bin برد نیست'], 400);
        $name = 'fw_' . $ver . '_' . time() . '.bin';
        @mkdir(__DIR__ . '/fw', 0755);
        move_uploaded_file($f['tmp_name'], __DIR__ . '/fw/' . $name);
        q('INSERT INTO firmware (version, file, size, md5, created_at) VALUES (?, ?, ?, ?, ?)',
            [$ver, $name, filesize(__DIR__ . '/fw/' . $name), md5_file(__DIR__ . '/fw/' . $name), $now]);
        json_out(['ok' => true]);
    case 'firmware_delete':
        $fw = q('SELECT * FROM firmware WHERE id = ?', [(int)$b['id']])->fetch();
        if ($fw) {
            @unlink(__DIR__ . '/fw/' . basename($fw['file']));
            q('DELETE FROM firmware WHERE id = ?', [(int)$b['id']]);
        }
        json_out(['ok' => true]);

    // ---------- device ----------
    case 'device_add':
        $token = bin2hex(random_bytes(20));
        q('INSERT INTO devices (name, token) VALUES (?, ?)', [mb_substr((string)($b['name'] ?? 'Petbot'), 0, 64), $token]);
        $nid = (int)db()->lastInsertId();
        q('INSERT INTO settings (device_id, json) VALUES (?, ?)', [$nid, json_encode(default_settings())]);
        json_out(['ok' => true, 'id' => $nid, 'token' => $token]);
    case 'token_regen':
        $token = bin2hex(random_bytes(20));
        q('UPDATE devices SET token = ? WHERE id = ?', [$token, $id]);
        json_out(['ok' => true, 'token' => $token]);
    case 'password':
        $p = (string)($b['pass'] ?? '');
        if (mb_strlen($p) < 8) json_out(['error' => 'حداقل ۸ حرف'], 400);
        q("REPLACE INTO meta (k, v) VALUES ('admin_hash', ?)", [password_hash($p, PASSWORD_DEFAULT)]);
        json_out(['ok' => true]);
}
json_out(['error' => 'unknown action'], 400);
