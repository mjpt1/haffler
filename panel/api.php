<?php
// Device API. The board calls this with its key in the X-Token header (or ?t=).
//   POST api.php?a=sync    body: state  -> everything the board needs
//   POST api.php?a=ack     body: {commands:[ids], messages:[ids]}
//   POST api.php?a=todo    body: {id, done}
//   POST api.php?a=habit   body: {id}           (toggle today)
//   GET  api.php?a=fw&id=N                       (firmware binary for OTA)
//   GET  api.php?a=hafez                         (random Hafez poem)
declare(strict_types=1);
require __DIR__ . '/lib.php';

$token = $_SERVER['HTTP_X_TOKEN'] ?? ($_GET['t'] ?? '');
$dev = device_by_token((string)$token);
if (!$dev) json_out(['error' => 'bad token'], 403);
$id = (int)$dev['id'];
$a = $_GET['a'] ?? 'sync';

switch ($a) {
    case 'sync':
        $s = body();
        $ip = $_SERVER['REMOTE_ADDR'] ?? '';
        q('UPDATE devices SET last_seen = NOW(), state_json = ?, fw = ?, ip = ? WHERE id = ?',
            [json_encode($s, JSON_UNESCAPED_UNICODE), substr((string)($s['fw'] ?? ''), 0, 32), $ip, $id]);

        // history point every 5 minutes
        $last = q('SELECT ts FROM history WHERE device_id = ? ORDER BY id DESC LIMIT 1', [$id])->fetchColumn();
        if (!$last || time() - strtotime($last) >= 300) {
            q('INSERT INTO history (device_id, ts, mood, joy, hunger, energy, temp) VALUES (?, NOW(), ?, ?, ?, ?, ?)',
                [$id, (int)($s['mood'] ?? 0), (float)($s['joy'] ?? 0), (float)($s['hunger'] ?? 0),
                    (float)($s['energy'] ?? 0), (float)($s['temp'] ?? 0)]);
            q('DELETE FROM history WHERE device_id = ? AND ts < NOW() - INTERVAL 30 DAY', [$id]);
        }

        $set = device_settings($id);
        $today = date('Y-m-d');
        $out = [
            'rev' => (int)$dev['rev'],
            'time' => time(),
            'tz' => (int)date('Z'),
            'settings' => $set,
            'todos' => q('SELECT id, text, done, prio, due FROM todos WHERE device_id = ? ORDER BY done, prio DESC, id DESC LIMIT 30', [$id])->fetchAll(),
            'messages' => q('SELECT id, text FROM messages WHERE device_id = ? AND shown_at IS NULL ORDER BY id LIMIT 5', [$id])->fetchAll(),
            'commands' => q('SELECT id, cmd, arg FROM commands WHERE device_id = ? AND done_at IS NULL ORDER BY id LIMIT 10', [$id])->fetchAll(),
            'alarms' => q('SELECT id, hhmm, days, label, kind FROM alarms WHERE device_id = ? AND enabled = 1', [$id])->fetchAll(),
            'countdowns' => q('SELECT id, title, date FROM countdowns WHERE device_id = ? ORDER BY date LIMIT 10', [$id])->fetchAll(),
            'habits' => q('SELECT h.id, h.title, (l.day IS NOT NULL) AS done FROM habits h
                           LEFT JOIN habit_log l ON l.habit_id = h.id AND l.day = ? WHERE h.device_id = ?', [$today, $id])->fetchAll(),
        ];
        if (cfg('enable_proxy')) {
            if ($set['show_weather']) $out['weather'] = weather((float)$set['lat'], (float)$set['lon']);
            if ($set['show_prices']) $out['prices'] = prices();
        }
        $fw = q('SELECT id, version, size, md5 FROM firmware ORDER BY id DESC LIMIT 1')->fetch();
        if ($fw) $out['firmware'] = $fw;
        json_out($out);

    case 'ack':
        $b = body();
        foreach ((array)($b['commands'] ?? []) as $cid)
            q('UPDATE commands SET done_at = NOW() WHERE id = ? AND device_id = ?', [(int)$cid, $id]);
        foreach ((array)($b['messages'] ?? []) as $mid)
            q('UPDATE messages SET shown_at = NOW() WHERE id = ? AND device_id = ?', [(int)$mid, $id]);
        json_out(['ok' => true]);

    case 'todo':
        $b = body();
        q('UPDATE todos SET done = ?, updated_at = NOW() WHERE id = ? AND device_id = ?',
            [empty($b['done']) ? 0 : 1, (int)($b['id'] ?? 0), $id]);
        bump($id);
        json_out(['ok' => true]);

    case 'habit':
        $b = body();
        $hid = (int)($b['id'] ?? 0);
        $own = q('SELECT id FROM habits WHERE id = ? AND device_id = ?', [$hid, $id])->fetch();
        if ($own) {
            $day = date('Y-m-d');
            $done = q('SELECT 1 FROM habit_log WHERE habit_id = ? AND day = ?', [$hid, $day])->fetch();
            if ($done) q('DELETE FROM habit_log WHERE habit_id = ? AND day = ?', [$hid, $day]);
            else q('INSERT INTO habit_log (habit_id, day) VALUES (?, ?)', [$hid, $day]);
        }
        json_out(['ok' => true]);

    case 'fw':
        $fw = q('SELECT * FROM firmware WHERE id = ?', [(int)($_GET['id'] ?? 0)])->fetch();
        $path = $fw ? __DIR__ . '/fw/' . basename($fw['file']) : '';
        if (!$fw || !is_file($path)) json_out(['error' => 'not found'], 404);
        header('Content-Type: application/octet-stream');
        header('Content-Length: ' . filesize($path));
        readfile($path);
        exit;

    case 'hafez':
        $p = cached('hafez_' . mt_rand(1, 30), 3600 * 24, function () {
            $j = http_get_json('https://api.ganjoor.net/api/ganjoor/hafez/faal', 8);
            if (!$j || empty($j['plainText'])) return null;
            return ['title' => $j['title'] ?? '', 'text' => trim($j['plainText'])];
        });
        json_out($p ?: ['title' => '', 'text' => '']);
}
json_out(['error' => 'unknown action'], 400);

// ---------- outside data (cached on the server) ----------
function weather(float $lat, float $lon): ?array
{
    $key = 'wx_' . round($lat, 2) . '_' . round($lon, 2);
    return cached($key, 1200, function () use ($lat, $lon) {
        $u = 'https://api.open-meteo.com/v1/forecast?latitude=' . $lat . '&longitude=' . $lon
            . '&current=temperature_2m,weather_code,is_day&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset'
            . '&timezone=auto&forecast_days=1';
        $j = http_get_json($u);
        if (!$j || !isset($j['current'])) return null;
        return [
            'temp' => $j['current']['temperature_2m'],
            'code' => $j['current']['weather_code'],
            'day' => $j['current']['is_day'],
            'max' => $j['daily']['temperature_2m_max'][0] ?? null,
            'min' => $j['daily']['temperature_2m_min'][0] ?? null,
            'sunrise' => substr($j['daily']['sunrise'][0] ?? '', 11, 5),
            'sunset' => substr($j['daily']['sunset'][0] ?? '', 11, 5),
        ];
    });
}

function prices(): ?array
{
    return cached('prices', 600, function () {
        $j = http_get_json('https://call1.tgju.org/ajax.json', 8);
        $c = $j['current'] ?? null;
        if (!$c) return null;
        $pick = function ($k) use ($c) { return isset($c[$k]['p']) ? (string)$c[$k]['p'] : null; };
        return [
            'usd' => $pick('price_dollar_rl'),
            'eur' => $pick('price_eur'),
            'coin' => $pick('sekee'),
            'gold18' => $pick('geram18'),
        ];
    });
}
