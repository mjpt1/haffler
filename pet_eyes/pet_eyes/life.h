#pragma once
// Pet life: coins, XP/level, shop + accessories, sickness, "missed you". Saved in NVS.
// Needs: canvas, mix, Palette, fillAAEllipse, faDraw, faDigits, appFrame/appClose, soundPlay.
#include <Preferences.h>

enum ItemId { IT_WIZARD, IT_CROWN, IT_HEADPHONES, IT_PARTY, IT_SUNGLASSES, IT_BOWTIE,
              IT_SKIN_BLUE, IT_SKIN_PURPLE, IT_SKIN_GOLD, IT_MEDICINE, IT_COUNT };
struct ShopItem { const char *name; int price; uint8_t slot; uint32_t col; };  // slot: 0 hat, 1 face, 2 skin, 3 use-once
static const ShopItem SHOP[IT_COUNT] = {
  {"کلاه جادوگر", 40, 0, 0x7A3DB8}, {"تاج", 80, 0, 0xFFC93C}, {"هدفون", 60, 0, 0x48E6FF}, {"کلاه تولد", 30, 0, 0xFF72B6},
  {"عینک آفتابی", 35, 1, 0x2A2F38}, {"پاپیون", 25, 1, 0xE8283A},
  {"چشم آبی", 50, 2, 0x48E6FF}, {"چشم بنفش", 50, 2, 0xB98CFF}, {"چشم طلایی", 120, 2, 0xFFD45C},
  {"دارو", 15, 3, 0x5BE37D},
};
static const Palette SKINS[3] = {{0x48E6FF, 0x0A262E, 0x124A58}, {0xB98CFF, 0x1E1030, 0x3D2266}, {0xFFD45C, 0x2E2408, 0x5C4812}};

struct Life {
  int coins = 20, xp = 0;
  uint32_t owned = 0;
  int8_t hat = -1, face = -1, skin = -1;
  bool sick = false;
  uint32_t lastSeen = 0;  // epoch seconds
  int lastDay = -1;       // day-of-year of the last daily bonus
} life;
Preferences lifePrefs;
bool levelUpPending = false;

static int lifeLevel() { return 1 + life.xp / 100; }
bool lifeSick() { return life.sick; }

static void lifeSave() {
  lifePrefs.begin("life", false);
  lifePrefs.putInt("coins", life.coins);
  lifePrefs.putInt("xp", life.xp);
  lifePrefs.putUInt("owned", life.owned);
  lifePrefs.putChar("hat", life.hat);
  lifePrefs.putChar("face", life.face);
  lifePrefs.putChar("skin", life.skin);
  lifePrefs.putBool("sick", life.sick);
  lifePrefs.putUInt("seen", life.lastSeen);
  lifePrefs.putInt("day", life.lastDay);
  lifePrefs.end();
}

static void lifeLoad() {
  lifePrefs.begin("life", true);
  life.coins = lifePrefs.getInt("coins", 20);
  life.xp = lifePrefs.getInt("xp", 0);
  life.owned = lifePrefs.getUInt("owned", 0);
  life.hat = lifePrefs.getChar("hat", -1);
  life.face = lifePrefs.getChar("face", -1);
  life.skin = lifePrefs.getChar("skin", -1);
  life.sick = lifePrefs.getBool("sick", false);
  life.lastSeen = lifePrefs.getUInt("seen", 0);
  life.lastDay = lifePrefs.getInt("day", -1);
  lifePrefs.end();
}

void lifeAddCoins(int n) { life.coins = max(0, life.coins + n); lifeSave(); }
void lifeAddXp(int n) {
  int before = lifeLevel();
  life.xp += n;
  if (lifeLevel() > before) { levelUpPending = true; life.coins += 10 * lifeLevel(); }
  lifeSave();
}

// ---------- accessories on the pet (band coordinates) ----------
static void drawAccessories(float cx, float cy, float dx, float sL, float sR, float openK) {
  float lx = cx - dx, rx2 = cx + dx;
  float rxL = 38 * sL, ryL = 52 * sL, rxR = 38 * sR, ryR = 52 * sR;
  if (life.face == IT_SUNGLASSES && openK > 0.3f) {
    for (int s = 0; s < 2; s++) {
      float ex = s ? rx2 : lx, rx = s ? rxR : rxL, ry = s ? ryR : ryL;
      canvas.fillSmoothRoundRect(ex - rx - 6, cy - ry * 0.75f, rx * 2 + 12, ry * 0.95f, 14, 0x15181E);
      canvas.drawWideLine(ex - rx * 0.6f, cy - ry * 0.55f, ex - rx * 0.1f, cy - ry * 0.05f, 2.5f, 0x3A4252);
    }
    canvas.fillRect(lx + rxL + 6, cy - ryL * 0.55f, rx2 - lx - rxL - rxR - 12, 6, 0x15181E);
  }
  if (life.face == IT_BOWTIE) {
    float by = cy + max(ryL, ryR) + 12;
    canvas.fillTriangle(cx, by, cx - 22, by - 11, cx - 22, by + 11, 0xE8283A);
    canvas.fillTriangle(cx, by, cx + 22, by - 11, cx + 22, by + 11, 0xE8283A);
    canvas.fillSmoothCircle(cx, by, 5, 0xB01828);
  }
  switch (life.hat) {
    case IT_WIZARD: {
      float top = cy - ryR - 8;
      canvas.fillTriangle(rx2 - rxR * 0.9f, top, rx2 + rxR * 0.9f, top, rx2 + rxR * 0.5f, top - 58, 0x5A2A9E);
      fillAAEllipse(rx2, top, rxR + 8, 6, 0x7A3DB8, 0x000000);
      canvas.fillSmoothCircle(rx2 + 2, top - 22, 3, 0xFFD45C);
      canvas.fillSmoothCircle(rx2 + 12, top - 36, 2, 0xFFD45C);
      break;
    }
    case IT_CROWN: {
      float top = cy - max(ryL, ryR) - 14;
      canvas.fillRect(cx - 40, top - 12, 80, 14, 0xFFC93C);
      for (int i = 0; i < 3; i++) canvas.fillTriangle(cx - 40 + i * 30, top - 12, cx - 20 + i * 30, top - 12, cx - 30 + i * 30 + 10, top - 34, 0xFFC93C);
      canvas.fillSmoothCircle(cx, top - 6, 4, 0xE8283A);
      canvas.fillSmoothCircle(cx - 24, top - 6, 3, 0x48E6FF);
      canvas.fillSmoothCircle(cx + 24, top - 6, 3, 0x5BE37D);
      break;
    }
    case IT_HEADPHONES: {
      float r = dx + max(rxL, rxR) + 14;
      canvas.fillArc(cx, cy + 6, r, r - 7, 200, 340, 0x2A3644);
      canvas.fillSmoothRoundRect(lx - rxL - 22, cy - 18, 16, 40, 7, 0x48E6FF);
      canvas.fillSmoothRoundRect(rx2 + rxR + 6, cy - 18, 16, 40, 7, 0x48E6FF);
      break;
    }
    case IT_PARTY: {
      float top = cy - ryL - 6;
      canvas.fillTriangle(lx - 18, top, lx + 18, top, lx - 4, top - 44, 0xFF72B6);
      canvas.drawWideLine(lx - 12, top - 10, lx + 12, top - 10, 2, 0xFFD45C);
      canvas.drawWideLine(lx - 8, top - 24, lx + 6, top - 24, 2, 0x48E6FF);
      canvas.fillSmoothCircle(lx - 4, top - 46, 5, 0xFFD45C);
      break;
    }
  }
}

// ---------- pet screen: stats + shop ----------
static float shopScroll = 0;
static bool petApp(float hunger, float joy, float energy) {
  if (appClose()) return false;
  appFrame("پت من", 0xFF72B6);
  // stats
  String lv = "سطح " + faDigits(String(lifeLevel())) + "   ·   سکه " + faDigits(String(life.coins));
  faDraw(lv.c_str(), 306, 46, faSmall, 0xFFD45C, 2);
  int xpIn = life.xp % 100;
  canvas.fillSmoothRoundRect(14, 50, 120, 8, 4, 0x1C2630);
  canvas.fillSmoothRoundRect(14, 50, 120 * xpIn / 100, 8, 4, 0xFFD45C);
  const char *L[3] = {"شادی", "انرژی", "سیری"};
  float V[3] = {joy, energy, 1 - hunger};
  for (int i = 0; i < 3; i++) {
    int x = 214 - i * 100;
    faDraw(L[i], x + 92, 68, faSmall, A_DIM, 2);
    canvas.fillSmoothRoundRect(x, 90, 92, 6, 3, 0x1C2630);
    canvas.fillSmoothRoundRect(x, 90, 92 * constrain(V[i], 0.0f, 1.0f), 6, 3, i == 2 && V[i] < 0.3f ? 0xFF4A3A : 0x5BE37D);
  }
  if (life.sick) faDraw("مریض است — از فروشگاه دارو بخرید", 160, 100, faSmall, 0x9BE15D, 1);

  // shop list
  static int lastY = 0;
  static bool dragging = false;
  if (in.pressed) { lastY = in.y; dragging = false; }
  if (in.down && in.y > 120) {
    if (abs(in.y - in.sy) > 8) dragging = true;
    shopScroll += lastY - in.y;
    lastY = in.y;
  }
  shopScroll = constrain(shopScroll, 0.0f, (float)max(0, IT_COUNT * 30 - 110));
  canvas.setClipRect(0, 124, 320, 116);
  for (int i = 0; i < IT_COUNT; i++) {
    int y = 126 + i * 30 - (int)shopScroll;
    if (y < 96 || y > 240) continue;
    const ShopItem &it = SHOP[i];
    bool own = life.owned & (1u << i);
    bool worn = (it.slot == 0 && life.hat == i) || (it.slot == 1 && life.face == i) || (it.slot == 2 && life.skin == i - IT_SKIN_BLUE);
    canvas.fillSmoothRoundRect(10, y, 300, 26, 8, worn ? mix(A_CARD, it.col, 0.3f) : A_CARD);
    canvas.fillSmoothCircle(296, y + 13, 7, it.col);
    faDraw(it.name, 282, y + 4, faSmall, A_TEXT, 2);
    String right = it.slot == 3 ? faDigits(String(it.price)) + " سکه"
                 : (worn ? "پوشیده" : (own ? "داری · بپوش" : faDigits(String(it.price)) + " سکه"));
    faDraw(right.c_str(), 22, y + 4, faSmall, own || worn ? 0x5BE37D : 0xFFD45C, 0);
    if (in.released && !dragging && in.y > y && in.y < y + 26 && in.y > 124) {
      if (it.slot == 3) {
        if (life.coins >= it.price && life.sick) { life.coins -= it.price; life.sick = false; soundPlay(SND_HAPPY); }
        else soundPlay(SND_CLICK);
      } else if (!own) {
        if (life.coins >= it.price) { life.coins -= it.price; life.owned |= 1u << i; soundPlay(SND_POINT); }
        else soundPlay(SND_GROWL);
      } else {  // toggle wear
        if (it.slot == 0) life.hat = worn ? -1 : i;
        if (it.slot == 1) life.face = worn ? -1 : i;
        if (it.slot == 2) life.skin = worn ? -1 : i - IT_SKIN_BLUE;
        soundPlay(SND_CLICK);
      }
      lifeSave();
    }
  }
  canvas.clearClipRect();
  return true;
}
