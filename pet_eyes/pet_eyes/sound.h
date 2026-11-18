#pragma once
// Non-blocking buzzer sounds on IO46 (board buzzer). Each sound is a list of
// tone segments {startHz, endHz, ms}; a glide happens when start != end. 0 Hz = silence.

#define PIN_BUZZER 46

struct Tone { uint16_t f0, f1, ms; };

static const Tone SND_BOOP[]    = {{900, 1300, 60}, {0, 0, 0}};
static const Tone SND_HAPPY[]   = {{1047, 1047, 70}, {1319, 1319, 70}, {1568, 1568, 70}, {2093, 2093, 120}, {0, 0, 0}};
static const Tone SND_LOVE[]    = {{1568, 1760, 120}, {0, 0, 60}, {1760, 2093, 160}, {0, 0, 0}};
static const Tone SND_GROWL[]   = {{140, 90, 160}, {90, 130, 140}, {130, 80, 260}, {0, 0, 0}};
static const Tone SND_BLEEP[]   = {{1000, 1000, 110}, {0, 0, 50}, {1000, 1000, 110}, {0, 0, 50}, {1000, 1000, 220}, {0, 0, 0}};
static const Tone SND_VOMIT[]   = {{500, 120, 350}, {0, 0, 120}, {450, 90, 500}, {120, 70, 300}, {0, 0, 0}};
static const Tone SND_CHOMP[]   = {{260, 180, 50}, {0, 0, 0}};
static const Tone SND_SURPRISE[] = {{600, 1600, 140}, {0, 0, 0}};
static const Tone SND_DIZZY[]   = {{700, 500, 120}, {500, 800, 120}, {800, 450, 120}, {450, 700, 160}, {0, 0, 0}};
static const Tone SND_SNORE[]   = {{90, 70, 600}, {0, 0, 0}};
static const Tone SND_DING[]    = {{1760, 1760, 90}, {2637, 2637, 260}, {0, 0, 0}};
static const Tone SND_ALARM[]   = {{2000, 2000, 120}, {0, 0, 80}, {2000, 2000, 120}, {0, 0, 80}, {2000, 2000, 120}, {0, 0, 500}, {0, 0, 0}};
static const Tone SND_FLAP[]    = {{500, 900, 50}, {0, 0, 0}};
static const Tone SND_POINT[]   = {{1319, 1319, 50}, {1976, 1976, 90}, {0, 0, 0}};
static const Tone SND_DIE[]     = {{600, 150, 450}, {0, 0, 0}};
static const Tone SND_CLICK[]   = {{1800, 1800, 15}, {0, 0, 0}};
static const Tone SND_MELODY[]  = {{659, 659, 150}, {784, 784, 150}, {880, 880, 300}, {784, 784, 150}, {880, 880, 150},
                                   {1047, 1047, 450}, {0, 0, 80}, {988, 988, 150}, {880, 880, 150}, {784, 784, 450}, {0, 0, 0}};

// Pomodoro
static const Tone SND_POMO_START[] = {{784, 784, 90}, {988, 988, 90}, {1319, 1319, 160}, {0, 0, 0}};
static const Tone SND_POMO_PAUSE[] = {{1175, 1175, 90}, {880, 880, 160}, {0, 0, 0}};
static const Tone SND_FOCUS_DONE[] = {{1047, 1047, 160}, {1319, 1319, 160}, {1568, 1568, 160}, {2093, 2093, 400}, {0, 0, 120},
                                      {1568, 1568, 120}, {2093, 2093, 500}, {0, 0, 0}};
static const Tone SND_BREAK_DONE[] = {{880, 880, 120}, {0, 0, 60}, {880, 880, 120}, {0, 0, 60}, {1175, 1175, 300}, {0, 0, 0}};

bool soundOn = true;
bool buzzerOn = true;       // board buzzer; turn off in the panel once the speaker is fitted
uint8_t soundVolume = 60;  // 0..100

static const Tone *sndSeq = nullptr;
static uint8_t sndIdx = 0;
static uint32_t sndSegStart = 0;
static uint16_t sndLastF = 0;

// ---------- speaker: MAX98357A I2S amp on IO4 (BCLK), IO5 (LRC), IO6 (DIN) ----------
// A small synth task turns the current frequency into a soft tone (sine + a little 2nd harmonic)
// with smooth fade in/out and glides, so the same sound list works on buzzer and speaker.
#include <ESP_I2S.h>
#define PIN_I2S_BCLK 4
#define PIN_I2S_LRC  5
#define PIN_I2S_DIN  6
#define SYNTH_RATE   22050
#define SPEAKER_MAX  0.45f   // amplitude cap: the 4 ohm 1.5 W speaker must not be overdriven

static I2SClass spk;
static volatile float synthFreq = 0, synthAmp = 0;  // targets set by soundSet()
static bool speakerOk = false;

static void synthTask(void *) {
  static int16_t buf[256];
  static int16_t sine[256];
  for (int i = 0; i < 256; i++) sine[i] = (int16_t)(sinf(i * 2 * PI / 256) * 32767);
  float phase = 0, phase2 = 0, freq = 0, amp = 0;
  for (;;) {
    float tf = synthFreq, ta = synthAmp;
    for (int i = 0; i < 256; i++) {
      if (tf > 0) freq += (tf - freq) * 0.02f;          // glide
      amp += (ta - amp) * (ta > amp ? 0.02f : 0.004f);  // ~2 ms attack, ~10 ms release
      float inc = freq * 256.0f / SYNTH_RATE;
      phase += inc;
      phase2 += inc * 2;
      if (phase >= 256) phase -= 256;
      if (phase2 >= 256) phase2 -= 256;
      float s = sine[(int)phase] * 0.8f + sine[(int)phase2] * 0.2f;
      buf[i] = (int16_t)(s * amp);
    }
    spk.write((uint8_t *)buf, sizeof(buf));  // blocks while the DMA plays (sets the pace)
  }
}

static void speakerBegin() {
  spk.setPins(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DIN);
  speakerOk = spk.begin(I2S_MODE_STD, SYNTH_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  if (speakerOk) xTaskCreatePinnedToCore(synthTask, "synth", 3072, nullptr, 2, nullptr, 0);
  Serial.printf("SPEAKER: I2S %s\n", speakerOk ? "ready" : "failed");
}

static void soundBegin() {
  ledcAttach(PIN_BUZZER, 2000, 10);
  ledcWrite(PIN_BUZZER, 0);
  speakerBegin();
}

static void soundSet(uint16_t f) {
  if (f == sndLastF) return;
  sndLastF = f;
  bool on = f && soundOn && soundVolume;
  // speaker
  if (on) synthFreq = f;
  synthAmp = on ? SPEAKER_MAX * soundVolume / 100.0f : 0;
  // buzzer
  if (!on || !buzzerOn) { ledcWrite(PIN_BUZZER, 0); return; }
  ledcChangeFrequency(PIN_BUZZER, f, 10);
  ledcWrite(PIN_BUZZER, 512 * soundVolume / 100);  // 50% duty = loudest
}

static void soundPlay(const Tone *seq) {
  if (!soundOn) return;
  sndSeq = seq;
  sndIdx = 0;
  sndSegStart = millis();
}

static bool soundBusy() { return sndSeq != nullptr; }

// Continuous tone (piano): call every frame while held, soundHold(0) to stop.
static void soundHold(uint16_t f) {
  sndSeq = nullptr;
  soundSet(f);
}

static void soundUpdate() {
  if (!sndSeq) return;
  const Tone &t = sndSeq[sndIdx];
  if (t.ms == 0) { sndSeq = nullptr; soundSet(0); return; }
  uint32_t el = millis() - sndSegStart;
  if (el >= t.ms) {
    sndIdx++;
    sndSegStart = millis();
    return;
  }
  float k = el / (float)t.ms;
  soundSet(t.f0 ? (uint16_t)(t.f0 + (t.f1 - (int)t.f0) * k) : 0);
}

static const Tone *soundByName(const String &n) {
  if (n == "happy") return SND_HAPPY;
  if (n == "growl") return SND_GROWL;
  if (n == "alarm") return SND_ALARM;
  if (n == "melody") return SND_MELODY;
  if (n == "ding") return SND_DING;
  if (n == "love") return SND_LOVE;
  if (n == "vomit") return SND_VOMIT;
  return SND_BOOP;
}
