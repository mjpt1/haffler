<?php
// Shared helpers: config, database, auth, JSON responses.

declare(strict_types=1);

function cfg(?string $key = null)
{
    static $cfg = null;
    if ($cfg === null) {
        $file = __DIR__ . '/config.php';
        if (!is_file($file)) {
            header('Location: install.php');
            exit;
        }
        $cfg = require $file;
        date_default_timezone_set($cfg['timezone'] ?? 'Asia/Tehran');
    }
    return $key === null ? $cfg : ($cfg[$key] ?? null);
}

function db(): PDO
{
    static $pdo = null;
    if ($pdo === null) {
        $pdo = new PDO(
            'mysql:host=' . cfg('db_host') . ';dbname=' . cfg('db_name') . ';charset=utf8mb4',
            cfg('db_user'),
            cfg('db_pass'),
            [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION, PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC]
        );
    }
    return $pdo;
}

function q(string $sql, array $args = []): PDOStatement
{
    $st = db()->prepare($sql);
    $st->execute($args);
    return $st;
}

function json_out($data, int $code = 200): void
{
    http_response_code($code);
    header('Content-Type: application/json; charset=utf-8');
    header('Cache-Control: no-store');
    echo json_encode($data, JSON_UNESCAPED_UNICODE);
    exit;
}

function body(): array
{
    $raw = file_get_contents('php://input');
    $j = json_decode($raw ?: '[]', true);
    return is_array($j) ? $j : $_POST;
}

// ---------- admin session ----------
function session_start_safe(): void
{
    if (session_status() !== PHP_SESSION_ACTIVE) {
        session_set_cookie_params(['httponly' => true, 'samesite' => 'Lax',
            'secure' => !empty($_SERVER['HTTPS'])]);
        session_start();
    }
}

function is_admin(): bool
{
    session_start_safe();
    return !empty($_SESSION['admin']);
}

function require_admin(): void
{
    if (!is_admin()) json_out(['error' => 'login required'], 401);
    $t = $_SERVER['HTTP_X_CSRF'] ?? '';
    if ($_SERVER['REQUEST_METHOD'] !== 'GET' && !hash_equals($_SESSION['csrf'] ?? '', $t)) {
        json_out(['error' => 'bad csrf'], 403);
    }
}

function csrf(): string
{
    session_start_safe();
    if (empty($_SESSION['csrf'])) $_SESSION['csrf'] = bin2hex(random_bytes(16));
    return $_SESSION['csrf'];
}

// ---------- device ----------
function device_by_token(string $token): ?array
{
    if ($token === '') return null;
    $d = q('SELECT * FROM devices WHERE token = ?', [$token])->fetch();
    return $d ?: null;
}

function bump(int $deviceId): void
{
    q('UPDATE devices SET rev = rev + 1 WHERE id = ?', [$deviceId]);
}

function default_settings(): array
{
    return [
        'pet_name' => 'Petbot',
        'grumpy' => true,
        'brightness' => 140,
        'sound' => true,
        'buzzer' => true,  // board buzzer; turn off after fitting the speaker
        'volume' => 60,
        'theme' => -1,
        'night_from' => 23,
        'night_to' => 7,
        'city' => 'Tehran',
        'tz' => '<+0330>-3:30',  // POSIX TZ string used by the board
        'lat' => 35.6892,
        'lon' => 51.3890,
        'birthday' => '',
        'sync_sec' => 15,
        'pomodoro_min' => 25,
        'water_every_min' => 0,
        'show_prices' => true,
        'show_weather' => true,
    ];
}

function device_settings(int $deviceId): array
{
    $row = q('SELECT json FROM settings WHERE device_id = ?', [$deviceId])->fetch();
    $s = $row ? (json_decode($row['json'], true) ?: []) : [];
    return array_merge(default_settings(), $s);
}

// ---------- proxy cache (outside data fetched by the server) ----------
function cached(string $key, int $ttl, callable $fetch)
{
    $row = q('SELECT value, updated_at FROM cache WHERE k = ?', [$key])->fetch();
    if ($row && time() - strtotime($row['updated_at']) < $ttl) return json_decode($row['value'], true);
    try {
        $val = $fetch();
    } catch (Throwable $e) {
        $val = null;
    }
    if ($val !== null) {
        q('REPLACE INTO cache (k, value, updated_at) VALUES (?, ?, NOW())', [$key, json_encode($val, JSON_UNESCAPED_UNICODE)]);
        return $val;
    }
    return $row ? json_decode($row['value'], true) : null;
}

function http_get_json(string $url, int $timeout = 6)
{
    $ctx = stream_context_create(['http' => ['timeout' => $timeout, 'header' => "User-Agent: petbot\r\n"],
        'ssl' => ['verify_peer' => true]]);
    $raw = @file_get_contents($url, false, $ctx);
    if ($raw === false) return null;
    return json_decode($raw, true);
}
