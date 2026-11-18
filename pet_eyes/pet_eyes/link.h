#pragma once
// Extra senses without extra hardware:
//  - PC monitor: tools/pc_monitor.py sends {"cpu":..,"ram":..,"temp":..} lines over USB serial
//  - ESP-NOW: pets on the same WiFi channel find each other (two boards)
//  - WiFi CSI: detects people moving in the room from changes in the WiFi signal
#include <esp_now.h>
#include <esp_wifi.h>
#include <ping/ping_sock.h>

extern bool espnowOk;  // defined below

// ---------- PC monitor ----------
float pcCpu = 0, pcRam = 0, pcTemp = 0;
uint32_t pcMs = 0;
uint8_t cpuHist[64];
int cpuHead = 0;

static bool pcOnline() { return pcMs && millis() - pcMs < 5000; }

static void pcPoll() {
  static char buf[160];
  static int n = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      buf[n] = 0;
      n = 0;
      if (buf[0] == '!') {  // diagnostics from the PC
        if (!strncmp(buf, "!mute", 5)) { ledcWrite(46, 0); ledcDetach(46); pinMode(46, OUTPUT); digitalWrite(46, LOW); Serial.println("DIAG: buzzer pin forced LOW"); }
        if (!strncmp(buf, "!low ", 5)) { int pin = atoi(buf + 5); pinMode(pin, OUTPUT); digitalWrite(pin, LOW); Serial.printf("DIAG: IO%d LOW\n", pin); }
        if (!strncmp(buf, "!in ", 4)) { int pin = atoi(buf + 4); pinMode(pin, INPUT); Serial.printf("DIAG: IO%d input = %d\n", pin, digitalRead(pin)); }
        if (!strncmp(buf, "!quiet", 6)) { espnowOk = false; esp_wifi_set_csi(false); Serial.println("DIAG: ESP-NOW + CSI off"); }
        if (!strncmp(buf, "!gtreset", 8)) {
          gtWrite8(0x8040, 0x02); delay(100); gtWrite8(0x8040, 0x00); delay(50);
          pinMode(40, OUTPUT); digitalWrite(40, HIGH); delay(60); pinMode(40, INPUT);
          Serial.println("DIAG: GT911 reset");
        }
        if (!strncmp(buf, "!snd", 4)) Serial.printf("DIAG: ledc duty %u freq %u\n", (unsigned)ledcRead(46), (unsigned)ledcReadFreq(46));
        if (!strncmp(buf, "!gt", 3)) {
          uint8_t s = 0, cfg = 0, p[6] = {0};
          gtRead(0x814E, &s, 1); gtRead(0x8047, &cfg, 1); gtRead(0x8150, p, 6);
          Serial.printf("DIAG: gt status 0x%02X cfgver 0x%02X pt %u,%u size %u\n", s, cfg, p[0] | (p[1] << 8), p[2] | (p[3] << 8), p[4]);
        }
        continue;
      }
      JsonDocument d;
      if (!deserializeJson(d, buf) && d["cpu"].is<float>()) {
        pcCpu = d["cpu"];
        pcRam = d["ram"] | 0.0f;
        pcTemp = d["temp"] | 0.0f;
        pcMs = millis();
        cpuHist[cpuHead] = (uint8_t)constrain(pcCpu, 0.0f, 100.0f);
        cpuHead = (cpuHead + 1) % 64;
      }
    } else if (n < (int)sizeof(buf) - 1) {
      buf[n++] = c;
    }
  }
}

// ---------- ESP-NOW friends ----------
struct PetHello { char magic[4]; char name[20]; uint8_t mood, level; };
volatile bool friendNew = false;
char friendName[20] = "";
volatile int friendRssi = 0;
volatile uint32_t friendMs = 0;
bool espnowOk = false;
static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static void onNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(PetHello) || memcmp(data, "PETB", 4)) return;
  const PetHello *h = (const PetHello *)data;
  bool fresh = !friendMs || millis() - friendMs > 300000UL;  // not seen for 5 min -> greet again
  memcpy(friendName, h->name, sizeof(friendName));
  friendName[sizeof(friendName) - 1] = 0;
  friendRssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
  friendMs = millis();
  if (fresh) friendNew = true;
}

static void espnowBegin() {
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW: init failed"); return; }
  esp_now_register_recv_cb(onNowRecv);
  esp_now_peer_info_t p = {};
  memcpy(p.peer_addr, BCAST, 6);
  p.channel = 0;  // current channel
  p.encrypt = false;
  esp_now_add_peer(&p);
  espnowOk = true;
}

static void espnowHello(const String &name, int mood, int level) {
  if (!espnowOk) return;
  PetHello h = {};
  memcpy(h.magic, "PETB", 4);
  strlcpy(h.name, name.c_str(), sizeof(h.name));
  h.mood = mood;
  h.level = level;
  esp_now_send(BCAST, (const uint8_t *)&h, sizeof(h));
}

// ---------- WiFi CSI motion ----------
volatile float csiScore = 0, csiBase = 1;
volatile uint32_t csiPackets = 0, csiMotionMs = 0;
static uint8_t apMac[6];
bool csiOk = false;

static void IRAM_ATTR onCsi(void *, wifi_csi_info_t *info) {
  if (!info || !info->buf) return;  // all frames (router replies alone were too rare)
  int pairs = min(64, info->len / 2);
  if (pairs < 8) return;
  // per-subcarrier change vs. its own slow average (people moving change some subcarriers a lot)
  static float ema[64];
  static bool init = false;
  float dev = 0, tot = 0;
  for (int i = 0; i < pairs; i++) {
    float im = info->buf[2 * i], re = info->buf[2 * i + 1];
    float a = sqrtf(im * im + re * re);
    if (!init) ema[i] = a;
    dev += fabsf(a - ema[i]);
    tot += ema[i];
    ema[i] += (a - ema[i]) * 0.03f;
  }
  init = true;
  dev = tot > 1 ? dev / tot * 100 : 0;  // percent change
  csiScore += (dev - csiScore) * 0.1f;
  csiBase += (csiScore - csiBase) * 0.002f;
  csiPackets++;
  if (csiPackets > 200 && csiScore > csiBase * 2.5f + 0.4f) csiMotionMs = millis();
}

static void csiBegin() {
  memcpy(apMac, WiFi.BSSID(), 6);
  wifi_csi_config_t cfg = {};
  cfg.lltf_en = true;
  cfg.htltf_en = true;
  cfg.stbc_htltf2_en = true;
  cfg.ltf_merge_en = true;
  cfg.channel_filter_en = true;
  cfg.manu_scale = false;
  cfg.shift = 0;
  esp_err_t e1 = esp_wifi_set_csi_config(&cfg);
  esp_err_t e2 = esp_wifi_set_csi_rx_cb(onCsi, nullptr);
  esp_err_t e3 = esp_wifi_set_csi(true);
  csiOk = e1 == ESP_OK && e2 == ESP_OK && e3 == ESP_OK;
  // steady traffic from the router = steady CSI samples: ping the gateway 20x per second
  esp_ping_config_t pc = ESP_PING_DEFAULT_CONFIG();
  IPAddress gw = WiFi.gatewayIP();
  pc.target_addr.type = IPADDR_TYPE_V4;
  pc.target_addr.u_addr.ip4.addr = (uint32_t)gw;
  pc.count = ESP_PING_COUNT_INFINITE;
  pc.interval_ms = 50;
  pc.timeout_ms = 200;
  pc.data_size = 8;
  esp_ping_callbacks_t cbs = {};
  esp_ping_handle_t ph;
  if (esp_ping_new_session(&pc, &cbs, &ph) == ESP_OK) esp_ping_start(ph);
  Serial.printf("CSI: %s (%d %d %d)\n", csiOk ? "on" : "not available", e1, e2, e3);
}

static bool motionNow() { return csiMotionMs && millis() - csiMotionMs < 3000; }
