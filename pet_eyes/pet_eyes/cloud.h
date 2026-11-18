#pragma once
// WiFi + local setup portal + sync with the online panel + OTA.
// Runs in its own FreeRTOS task so HTTP calls never freeze the animation.
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_sntp.h>

#define FW_VERSION "1.0.0"
#define TZ_IRAN "<+0330>-3:30"

// ---------- data shared with the UI (guarded by cloudMux) ----------
struct TodoItem { int id; String text; bool done; int prio; };
struct AlarmItem { String hhmm, label, kind; int days; };
struct CountItem { String title, date; };
struct HabitItem { int id; String title; bool done; };

struct CloudData {
  // settings from the panel
  String petName = "Petbot";
  bool grumpy = true, sound = true, buzzer = true, showPrices = true, showWeather = true;
  int brightness = 140, volume = 60, theme = -1, nightFrom = 23, nightTo = 7, syncSec = 15;
  int pomodoroMin = 25, waterEvery = 0;
  float lat = 35.6892f, lon = 51.389f;
  String city = "Tehran", birthday, tz = TZ_IRAN;
  // lists
  TodoItem todos[30]; int todoCount = 0;
  AlarmItem alarms[20]; int alarmCount = 0;
  CountItem counts[10]; int countCount = 0;
  HabitItem habits[10]; int habitCount = 0;
  // outside data
  bool wxValid = false; float wxTemp = 0, wxMin = 0, wxMax = 0; int wxCode = 0; bool wxDay = true;
  String sunrise, sunset;
  String usd, eur, coin, gold18;
  // firmware
  int fwId = 0; String fwVersion;
  // status
  int rev = -1; uint32_t lastOkMs = 0; bool lastOk = false; int server = 0; String lastError;
  // inbox for the UI
  String msgs[5]; int msgCount = 0;
  String hafezTitle, hafezText; bool hafezReady = false;
  // OTA
  int otaProgress = -1; bool otaFailed = false;
};
CloudData cloud;
SemaphoreHandle_t cloudMux;

struct PetState { int mood = 0; float joy = 0, hunger = 0, energy = 0, temp = 0; bool asleep = false; int coins = 0, level = 1; };
PetState petState;  // written by the UI, read by the task (guarded by cloudMux)

struct CloudCmd { char cmd[16]; char arg[64]; };
QueueHandle_t cmdQueue;

// requests from the UI to the task
volatile bool reqSync = false, reqHafez = false, reqPortal = false;
struct ToggleReq { char kind; int id; bool done; };  // 't' todo, 'h' habit
QueueHandle_t toggleQueue;

// ---------- local config (NVS) ----------
struct NetCfg { String ssid, pass, srv1, srv2, token, tz; } net;
Preferences netPrefs;

static void netLoad() {
  netPrefs.begin("net", true);
  net.ssid = netPrefs.getString("ssid", "");
  net.pass = netPrefs.getString("pass", "");
  net.srv1 = netPrefs.getString("srv1", "");
  net.srv2 = netPrefs.getString("srv2", "");
  net.token = netPrefs.getString("token", "");
  net.tz = netPrefs.getString("tz", TZ_IRAN);
  netPrefs.end();
}

static void netSave() {
  netPrefs.begin("net", false);
  netPrefs.putString("ssid", net.ssid);
  netPrefs.putString("pass", net.pass);
  netPrefs.putString("srv1", net.srv1);
  netPrefs.putString("srv2", net.srv2);
  netPrefs.putString("token", net.token);
  netPrefs.putString("tz", net.tz);
  netPrefs.end();
}

// ---------- local web portal ----------
WebServer web(80);
DNSServer dns;
bool apActive = false;
bool timeSynced = false;

static String htmlEsc(const String &s) {
  String o;
  for (char c : s) {
    if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;"; else if (c == '&') o += "&amp;";
    else o += c;
  }
  return o;
}

static void handleRoot() {
  String opts;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) WiFi.scanNetworks(true);
  for (int i = 0; i < n && i < 20; i++) opts += "<option>" + htmlEsc(WiFi.SSID(i)) + "</option>";
  if (n >= 0) { WiFi.scanDelete(); WiFi.scanNetworks(true); }

  String st = WiFi.isConnected() ? ("وصل به " + htmlEsc(WiFi.SSID()) + " · IP " + WiFi.localIP().toString()) : "وصل نیست";
  xSemaphoreTake(cloudMux, portMAX_DELAY);
  String sync = cloud.lastOk ? "اتصال به پنل برقرار است" : ("اتصال به پنل: " + htmlEsc(cloud.lastError));
  xSemaphoreGive(cloudMux);

  String h = F("<!doctype html><html dir=rtl lang=fa><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>تنظیم پت‌بات</title><style>body{font-family:Tahoma;background:#0b0f14;color:#e8eef5;padding:16px;max-width:460px;margin:auto}"
               "input,select{width:100%;box-sizing:border-box;padding:10px;margin:4px 0 12px;border-radius:10px;border:1px solid #2a3a4a;background:#121a23;color:#fff}"
               "button{padding:12px 24px;border:0;border-radius:10px;background:#ff4a3a;color:#fff;font-size:16px}"
               ".s{background:#121a23;padding:12px;border-radius:12px;margin-bottom:16px;font-size:14px}</style>"
               "<h2>🐾 تنظیم پت‌بات</h2>");
  h += "<div class=s>وای‌فای: " + st + "<br>" + sync + "<br>نسخه: " FW_VERSION "</div>";
  h += F("<form method=post action=/save><label>شبکه‌ی وای‌فای</label><input name=ssid list=nets value='");
  h += htmlEsc(net.ssid) + "'><datalist id=nets>" + opts + "</datalist>";
  h += F("<label>رمز وای‌فای</label><input name=pass type=password placeholder='بدون تغییر خالی بگذارید'>"
         "<label>آدرس سرور اصلی (مثلاً https://site.com/petbot)</label><input name=srv1 dir=ltr value='");
  h += htmlEsc(net.srv1) + F("'><label>آدرس سرور پشتیبان (اختیاری)</label><input name=srv2 dir=ltr value='");
  h += htmlEsc(net.srv2) + F("'><label>کلید دستگاه (از پنل)</label><input name=token dir=ltr value='");
  h += htmlEsc(net.token) + F("'><button>ذخیره و راه‌اندازی دوباره</button></form>");
  web.send(200, "text/html; charset=utf-8", h);
}

static void handleSave() {
  net.ssid = web.arg("ssid");
  if (web.arg("pass").length()) net.pass = web.arg("pass");
  net.srv1 = web.arg("srv1");
  net.srv2 = web.arg("srv2");
  net.token = web.arg("token");
  net.srv1.trim(); net.srv2.trim(); net.token.trim();
  while (net.srv1.endsWith("/")) net.srv1.remove(net.srv1.length() - 1);
  while (net.srv2.endsWith("/")) net.srv2.remove(net.srv2.length() - 1);
  netSave();
  web.send(200, "text/html; charset=utf-8",
           "<meta charset=utf-8><body dir=rtl style='font-family:Tahoma;background:#0b0f14;color:#fff;padding:24px'>"
           "<h3>ذخیره شد ✅</h3><p>پت دوباره راه‌اندازی می‌شود…</p>");
  delay(800);
  ESP.restart();
}

static void startPortal() {
  if (apActive) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Petbot-Setup");
  dns.start(53, "*", WiFi.softAPIP());
  apActive = true;
  Serial.printf("NET: setup portal on WiFi 'Petbot-Setup' -> http://%s\n", WiFi.softAPIP().toString().c_str());
}

// ---------- HTTP helpers ----------
static String serverUrl(int i) { return i == 0 ? net.srv1 : net.srv2; }

static bool httpCall(const String &url, const String &body, String &out, int timeoutMs = 8000) {
  // clients first: they must outlive HTTPClient (destroyed in reverse order)
  WiFiClientSecure tls;
  WiFiClient plain;
  HTTPClient http;
  bool ok;
  if (url.startsWith("https")) {
    tls.setInsecure();  // shared hosts often use certificates we cannot verify on the board
    ok = http.begin(tls, url);
  } else {
    ok = http.begin(plain, url);
  }
  if (!ok) return false;
  http.setTimeout(timeoutMs);
  http.addHeader("X-Token", net.token);
  int code;
  if (body.length()) {
    http.addHeader("Content-Type", "application/json");
    code = http.POST(body);
  } else {
    code = http.GET();
  }
  if (code == 200) out = http.getString();
  else out = "HTTP " + String(code);
  http.end();
  return code == 200;
}

// Try the active server, then the other one. Returns the server index used, or -1.
static int callAny(const String &path, const String &body, String &out) {
  for (int k = 0; k < 2; k++) {
    int i = (cloud.server + k) % 2;
    String base = serverUrl(i);
    if (!base.length()) continue;
    if (httpCall(base + path, body, out)) return i;
  }
  return -1;
}

static void doSync() {
  JsonDocument st;
  xSemaphoreTake(cloudMux, portMAX_DELAY);
  st["mood"] = petState.mood;
  st["joy"] = petState.joy;
  st["hunger"] = petState.hunger;
  st["energy"] = petState.energy;
  st["asleep"] = petState.asleep;
  st["temp"] = petState.temp;
  st["coins"] = petState.coins;
  st["level"] = petState.level;
  xSemaphoreGive(cloudMux);
  st["fw"] = FW_VERSION;
  st["rssi"] = WiFi.RSSI();
  st["heap"] = ESP.getFreeHeap();
  st["uptime"] = millis() / 1000;
  String body, resp;
  serializeJson(st, body);

  int used = callAny("/api.php?a=sync", body, resp);
  if (used < 0) {
    xSemaphoreTake(cloudMux, portMAX_DELAY);
    cloud.lastOk = false;
    cloud.lastError = resp;
    xSemaphoreGive(cloudMux);
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, resp)) {
    xSemaphoreTake(cloudMux, portMAX_DELAY);
    cloud.lastOk = false;
    cloud.lastError = "bad json";
    xSemaphoreGive(cloudMux);
    return;
  }

  // time from the server when NTP is blocked
  if (!timeSynced && doc["time"].is<long>()) {
    struct timeval tv = {(time_t)doc["time"].as<long>(), 0};
    settimeofday(&tv, nullptr);
  }

  String ackCmds = "[", ackMsgs = "[";
  xSemaphoreTake(cloudMux, portMAX_DELAY);
  cloud.server = used;
  cloud.lastOk = true;
  cloud.lastOkMs = millis();
  cloud.lastError = "";
  cloud.rev = doc["rev"] | cloud.rev;

  JsonObject s = doc["settings"];
  cloud.petName = s["pet_name"] | cloud.petName;
  cloud.grumpy = s["grumpy"] | cloud.grumpy;
  cloud.sound = s["sound"] | cloud.sound;
  cloud.buzzer = s["buzzer"] | cloud.buzzer;
  cloud.brightness = s["brightness"] | cloud.brightness;
  cloud.volume = s["volume"] | cloud.volume;
  cloud.theme = s["theme"] | cloud.theme;
  cloud.nightFrom = s["night_from"] | cloud.nightFrom;
  cloud.nightTo = s["night_to"] | cloud.nightTo;
  cloud.syncSec = max(5, (int)(s["sync_sec"] | cloud.syncSec));
  cloud.pomodoroMin = s["pomodoro_min"] | cloud.pomodoroMin;
  cloud.waterEvery = s["water_every_min"] | cloud.waterEvery;
  cloud.lat = s["lat"] | cloud.lat;
  cloud.lon = s["lon"] | cloud.lon;
  cloud.city = s["city"] | cloud.city;
  cloud.birthday = s["birthday"] | cloud.birthday;
  cloud.tz = s["tz"] | cloud.tz;
  cloud.showPrices = s["show_prices"] | cloud.showPrices;
  cloud.showWeather = s["show_weather"] | cloud.showWeather;

  cloud.todoCount = 0;
  for (JsonObject t : doc["todos"].as<JsonArray>()) {
    if (cloud.todoCount >= 30) break;
    TodoItem &it = cloud.todos[cloud.todoCount++];
    it.id = t["id"].as<int>();
    it.text = t["text"].as<String>();
    it.done = t["done"].as<int>() != 0;
    it.prio = t["prio"].as<int>();
  }
  cloud.alarmCount = 0;
  for (JsonObject a : doc["alarms"].as<JsonArray>()) {
    if (cloud.alarmCount >= 20) break;
    AlarmItem &it = cloud.alarms[cloud.alarmCount++];
    it.hhmm = a["hhmm"].as<String>();
    it.label = a["label"].as<String>();
    it.kind = a["kind"].as<String>();
    it.days = a["days"].as<int>();
  }
  cloud.countCount = 0;
  for (JsonObject c : doc["countdowns"].as<JsonArray>()) {
    if (cloud.countCount >= 10) break;
    cloud.counts[cloud.countCount].title = c["title"].as<String>();
    cloud.counts[cloud.countCount++].date = c["date"].as<String>();
  }
  cloud.habitCount = 0;
  for (JsonObject h : doc["habits"].as<JsonArray>()) {
    if (cloud.habitCount >= 10) break;
    HabitItem &it = cloud.habits[cloud.habitCount++];
    it.id = h["id"].as<int>();
    it.title = h["title"].as<String>();
    it.done = h["done"].as<int>() != 0;
  }
  if (doc["weather"].is<JsonObject>()) {
    JsonObject w = doc["weather"];
    cloud.wxValid = true;
    cloud.wxTemp = w["temp"] | 0.0f;
    cloud.wxMin = w["min"] | 0.0f;
    cloud.wxMax = w["max"] | 0.0f;
    cloud.wxCode = w["code"] | 0;
    cloud.wxDay = (w["day"] | 1) != 0;
    cloud.sunrise = w["sunrise"].as<String>();
    cloud.sunset = w["sunset"].as<String>();
  }
  if (doc["prices"].is<JsonObject>()) {
    JsonObject p = doc["prices"];
    cloud.usd = p["usd"] | "";
    cloud.eur = p["eur"] | "";
    cloud.coin = p["coin"] | "";
    cloud.gold18 = p["gold18"] | "";
  }
  if (doc["firmware"].is<JsonObject>()) {
    cloud.fwId = doc["firmware"]["id"].as<int>();
    cloud.fwVersion = doc["firmware"]["version"].as<String>();
  }
  for (JsonObject m : doc["messages"].as<JsonArray>()) {
    if (cloud.msgCount < 5) cloud.msgs[cloud.msgCount++] = m["text"].as<String>();
    if (ackMsgs.length() > 1) ackMsgs += ",";
    ackMsgs += String(m["id"].as<int>());
  }
  xSemaphoreGive(cloudMux);

  for (JsonObject c : doc["commands"].as<JsonArray>()) {
    CloudCmd cc;
    strlcpy(cc.cmd, c["cmd"] | "", sizeof(cc.cmd));
    strlcpy(cc.arg, c["arg"] | "", sizeof(cc.arg));
    xQueueSend(cmdQueue, &cc, 0);
    if (ackCmds.length() > 1) ackCmds += ",";
    ackCmds += String(c["id"].as<int>());
  }
  if (ackCmds.length() > 1 || ackMsgs.length() > 1) {
    String out;
    callAny("/api.php?a=ack", "{\"commands\":" + ackCmds + "],\"messages\":" + ackMsgs + "]}", out);
  }
}

static void doOta(int fwId) {
  String base = serverUrl(cloud.server);
  if (!base.length()) return;
  String url = base + "/api.php?a=fw&id=" + fwId + "&t=" + net.token;
  // clients first: they must outlive HTTPClient (destroyed in reverse order)
  WiFiClientSecure tls;
  WiFiClient plain;
  HTTPClient http;
  if (url.startsWith("https")) { tls.setInsecure(); http.begin(tls, url); }
  else http.begin(plain, url);
  http.setTimeout(15000);
  int code = http.GET();
  int len = http.getSize();
  bool ok = false;
  if (code == 200 && len > 0 && Update.begin(len)) {
    WiFiClient *s = http.getStreamPtr();
    uint8_t buf[1024];
    int got = 0;
    uint32_t lastData = millis();
    while (got < len && millis() - lastData < 15000) {
      int n = s->available();
      if (n > 0) {
        n = s->readBytes(buf, min(n, (int)sizeof(buf)));
        Update.write(buf, n);
        got += n;
        lastData = millis();
        cloud.otaProgress = got * 100 / len;
      } else {
        delay(2);
      }
    }
    ok = got == len && Update.end(true);
  }
  http.end();
  if (ok) {
    Serial.println("OTA: done, restarting");
    cloud.otaProgress = 100;
    delay(1500);
    ESP.restart();
  }
  Serial.printf("OTA: failed (HTTP %d, %s)\n", code, Update.errorString());
  cloud.otaFailed = true;
  cloud.otaProgress = -1;
}

static void doToggle(const ToggleReq &r) {
  String out;
  if (r.kind == 't')
    callAny("/api.php?a=todo", "{\"id\":" + String(r.id) + ",\"done\":" + (r.done ? "1" : "0") + "}", out);
  else
    callAny("/api.php?a=habit", "{\"id\":" + String(r.id) + "}", out);
}

static void doHafez() {
  String out;
  bool ok = callAny("/api.php?a=hafez", "", out) >= 0;
  JsonDocument d;
  xSemaphoreTake(cloudMux, portMAX_DELAY);
  if (ok && !deserializeJson(d, out) && d["text"].as<String>().length()) {
    cloud.hafezTitle = d["title"].as<String>();
    cloud.hafezText = d["text"].as<String>();
  } else {
    cloud.hafezTitle = "";
    cloud.hafezText = "فال گرفته نشد. اتصال به پنل را بررسی کنید.";
  }
  cloud.hafezReady = true;
  xSemaphoreGive(cloudMux);
}

static void cloudTask(void *) {
  uint32_t lastSync = 0, wifiStart = millis();
  bool wasConnected = false;
  for (;;) {
    web.handleClient();
    if (apActive) dns.processNextRequest();
    if (reqPortal) { reqPortal = false; startPortal(); }

    bool conn = WiFi.isConnected();
    if (conn && !wasConnected) {
      Serial.printf("NET: connected, IP %s\n", WiFi.localIP().toString().c_str());
      configTzTime(net.tz.c_str(), "pool.ntp.org", "time.cloudflare.com", "time.google.com");
      MDNS.begin("petbot");
      reqSync = true;
    }
    wasConnected = conn;
    if (!conn && !apActive && millis() - wifiStart > 25000) startPortal();
    if (!timeSynced && time(nullptr) > 1700000000) {
      struct tm tmv;
      if (getLocalTime(&tmv, 0) && sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) timeSynced = true;
    }

    if (conn && net.token.length()) {
      if (reqSync || millis() - lastSync > (uint32_t)cloud.syncSec * 1000) {
        reqSync = false;
        lastSync = millis();
        doSync();
      }
      ToggleReq tr;
      while (xQueueReceive(toggleQueue, &tr, 0)) doToggle(tr);
      if (reqHafez) { reqHafez = false; doHafez(); }
      if (cloud.otaProgress == 0) doOta(cloud.fwId);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

static void cloudBegin() {
  cloudMux = xSemaphoreCreateMutex();
  cmdQueue = xQueueCreate(10, sizeof(CloudCmd));
  toggleQueue = xQueueCreate(10, sizeof(ToggleReq));
  netLoad();
  cloud.tz = net.tz;
  setenv("TZ", net.tz.c_str(), 1);  // correct local time even before the first sync
  tzset();
  WiFi.persistent(false);
  WiFi.setHostname("petbot");
  WiFi.mode(WIFI_STA);
  if (net.ssid.length()) WiFi.begin(net.ssid.c_str(), net.pass.c_str());
  else startPortal();
  web.on("/", handleRoot);
  web.on("/save", HTTP_POST, handleSave);
  web.onNotFound(handleRoot);  // captive portal: every URL shows the setup page
  web.begin();
  xTaskCreatePinnedToCore(cloudTask, "cloud", 16384, nullptr, 1, nullptr, 0);
}
