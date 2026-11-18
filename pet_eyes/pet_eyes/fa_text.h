#pragma once
// Persian/English text on the sprite: UTF-8 decode, Arabic-script letter joining,
// simple right-to-left layout, anti-aliased 4-bit glyphs, word wrap.
// Needs: canvas, mix() from pet_eyes.ino.
#include "font_fa.h"

struct JoinInfo { uint16_t base, iso, fin, ini, med; };  // ini/med = 0 -> joins only to the right
static const JoinInfo JOIN[] = {
  {0x0621, 0xFE80, 0, 0, 0},           {0x0622, 0xFE81, 0xFE82, 0, 0},      {0x0623, 0xFE83, 0xFE84, 0, 0},
  {0x0624, 0xFE85, 0xFE86, 0, 0},      {0x0625, 0xFE87, 0xFE88, 0, 0},      {0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C},
  {0x0627, 0xFE8D, 0xFE8E, 0, 0},      {0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92}, {0x0629, 0xFE93, 0xFE94, 0, 0},
  {0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98}, {0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C}, {0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0},
  {0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4}, {0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8}, {0x062F, 0xFEA9, 0xFEAA, 0, 0},
  {0x0630, 0xFEAB, 0xFEAC, 0, 0},      {0x0631, 0xFEAD, 0xFEAE, 0, 0},      {0x0632, 0xFEAF, 0xFEB0, 0, 0},
  {0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4}, {0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8}, {0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC},
  {0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0}, {0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4}, {0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8},
  {0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC}, {0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0}, {0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4},
  {0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8}, {0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC}, {0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0},
  {0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4}, {0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8}, {0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC},
  {0x0648, 0xFEED, 0xFEEE, 0, 0},      {0x0649, 0xFEEF, 0xFEF0, 0, 0},      {0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4},
  {0x067E, 0xFB56, 0xFB57, 0xFB58, 0xFB59}, {0x0686, 0xFB7A, 0xFB7B, 0xFB7C, 0xFB7D}, {0x0698, 0xFB8A, 0xFB8B, 0, 0},
  {0x06A9, 0xFB8E, 0xFB8F, 0xFB90, 0xFB91}, {0x06AF, 0xFB92, 0xFB93, 0xFB94, 0xFB95}, {0x06CC, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF},
};

static const JoinInfo *faJoin(uint16_t cp) {
  for (auto &j : JOIN) if (j.base == cp) return &j;
  return nullptr;
}

static int faDecode(const char *s, uint16_t *out, int max) {
  int n = 0;
  const uint8_t *p = (const uint8_t *)s;
  while (*p && n < max) {
    uint32_t c = *p++;
    if (c >= 0xF0) { c = ((c & 0x07) << 18) | ((p[0] & 0x3F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
    else if (c >= 0xE0) { c = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F); p += 2; }
    else if (c >= 0xC0) { c = ((c & 0x1F) << 6) | (p[0] & 0x3F); p += 1; }
    if (c >= 0x064B && c <= 0x0652) continue;  // drop harakat
    if (c == 0x0670 || c > 0xFFFF) continue;
    if (c >= 0x0660 && c <= 0x0669) c = 0x06F0 + (c - 0x0660);  // Arabic -> Persian digits
    if (c == 0x0643) c = 0x06A9;  // Arabic kaf -> Persian
    if (c == 0x064A) c = 0x06CC;  // Arabic yeh -> Persian
    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    out[n++] = (uint16_t)c;
  }
  return n;
}

static bool faIsRtl(uint16_t c) {
  return (c >= 0x0600 && c <= 0x06EF) || (c >= 0xFB50 && c <= 0xFEFF);  // Persian digits (06F0..) are numbers -> LTR
}
static bool faIsLtr(uint16_t c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= 0x06F0 && c <= 0x06F9);
}

// Logical text -> shaped glyph codes in visual (left-to-right) order. Returns count.
static int faShape(const char *s, uint16_t *vis, int max) {
  static uint16_t cps[200], shp[200];
  int n = faDecode(s, cps, min(max, 200));
  bool rtl = false;
  for (int i = 0; i < n; i++) if (faIsRtl(cps[i])) { rtl = true; break; }

  int m = 0;
  for (int i = 0; i < n; i++) {
    uint16_t c = cps[i];
    if (c == 0x200C) continue;  // ZWNJ: breaks joining, not drawn
    const JoinInfo *j = faJoin(c);
    if (!j) { shp[m++] = c; continue; }
    const JoinInfo *pj = (i > 0) ? faJoin(cps[i - 1]) : nullptr;
    bool prevJoins = pj && pj->ini;
    // lam + alef ligatures
    if (c == 0x0644 && i + 1 < n) {
      uint16_t nx = cps[i + 1], lig = 0;
      if (nx == 0x0627) lig = 0xFEFB; else if (nx == 0x0622) lig = 0xFEF5;
      else if (nx == 0x0623) lig = 0xFEF7; else if (nx == 0x0625) lig = 0xFEF9;
      if (lig) { shp[m++] = prevJoins ? lig + 1 : lig; i++; continue; }
    }
    bool nextJoins = j->ini && i + 1 < n && faJoin(cps[i + 1]);
    uint16_t g = j->iso;
    if (prevJoins && nextJoins) g = j->med;
    else if (prevJoins) g = j->fin ? j->fin : j->iso;
    else if (nextJoins) g = j->ini;
    shp[m++] = g;
  }

  if (!rtl) { memcpy(vis, shp, m * 2); return m; }

  // RTL paragraph: reverse everything, then put LTR runs (latin words, numbers) back in reading order.
  for (int i = 0; i < m; i++) {
    uint16_t c = shp[m - 1 - i];
    if (c == '(') c = ')'; else if (c == ')') c = '(';
    else if (c == 0x00AB) c = 0x00BB; else if (c == 0x00BB) c = 0x00AB;
    vis[i] = c;
  }
  int i = 0;
  while (i < m) {
    if (!faIsLtr(vis[i])) { i++; continue; }
    int j = i;
    // extend run over LTR chars and inner neutrals (".", ",", ":", "/", "-", " ") followed by more LTR
    while (j + 1 < m && (faIsLtr(vis[j + 1]) ||
           ((vis[j + 1] == '.' || vis[j + 1] == ',' || vis[j + 1] == ':' || vis[j + 1] == '/' || vis[j + 1] == '-' ||
             vis[j + 1] == ' ') && j + 2 < m && faIsLtr(vis[j + 2]))))
      j++;
    for (int a = i, b = j; a < b; a++, b--) { uint16_t t = vis[a]; vis[a] = vis[b]; vis[b] = t; }
    i = j + 1;
  }
  return m;
}

static const FaGlyph *faGlyph(const FaFont &f, uint16_t cp) {
  int lo = 0, hi = f.count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (f.glyphs[mid].cp == cp) return &f.glyphs[mid];
    if (f.glyphs[mid].cp < cp) lo = mid + 1; else hi = mid - 1;
  }
  return nullptr;
}

static int faWidthVis(const uint16_t *v, int n, const FaFont &f) {
  int w = 0;
  for (int i = 0; i < n; i++) {
    const FaGlyph *g = faGlyph(f, v[i]);
    w += g ? g->adv : f.lineH / 3;
  }
  return w;
}

static int faWidth(const char *s, const FaFont &f) {
  static uint16_t v[200];
  int n = faShape(s, v, 200);
  return faWidthVis(v, n, f);
}

static uint32_t px888(int x, int y) {
  uint16_t c = canvas.readPixel(x, y);
  return (((uint32_t)((c >> 11) & 0x1F) * 255 / 31) << 16) | (((uint32_t)((c >> 5) & 0x3F) * 255 / 63) << 8) |
         ((uint32_t)(c & 0x1F) * 255 / 31);
}

// align: 0 = left, 1 = center, 2 = right. y = top of the line.
static void faDraw(const char *s, int x, int y, const FaFont &f, uint32_t col, uint8_t align = 0) {
  static uint16_t v[200];
  int n = faShape(s, v, 200);
  int w = faWidthVis(v, n, f);
  if (align == 1) x -= w / 2;
  else if (align == 2) x -= w;
  int base = y + f.ascent;
  int32_t cx0, cy0, cw, ch;  // only touch pixels inside the clip (strip rendering)
  canvas.getClipRect(&cx0, &cy0, &cw, &ch);
  if (base + 8 < cy0 || y - 8 > cy0 + ch) return;
  int W = min((int)canvas.width(), (int)(cx0 + cw)), H = min((int)canvas.height(), (int)(cy0 + ch));
  for (int i = 0; i < n; i++) {
    const FaGlyph *g = faGlyph(f, v[i]);
    if (!g) { x += f.lineH / 3; continue; }
    const uint8_t *bits = f.bits + g->off;
    for (int yy = 0; yy < g->h; yy++) {
      int py = base + g->yo + yy;
      if (py < cy0 || py >= H) continue;
      for (int xx = 0; xx < g->w; xx++) {
        int idx = yy * g->w + xx;
        uint8_t a = (idx & 1) ? (bits[idx >> 1] & 0x0F) : (bits[idx >> 1] >> 4);
        if (!a) continue;
        int px = x + g->xo + xx;
        if (px < cx0 || px >= W) continue;
        canvas.drawPixel(px, py, a == 15 ? col : mix(px888(px, py), col, a / 15.0f));
      }
    }
    x += g->adv;
  }
}

// Word-wrap logical text into lines that fit maxW. Returns number of lines.
static int faWrap(const char *s, const FaFont &f, int maxW, String *lines, int maxLines) {
  String text(s), cur;
  int n = 0, start = 0;
  while (start <= (int)text.length() && n < maxLines) {
    int sp = text.indexOf(' ', start);
    String word = sp < 0 ? text.substring(start) : text.substring(start, sp);
    String trial = cur.length() ? cur + " " + word : word;
    if (cur.length() && faWidth(trial.c_str(), f) > maxW) {
      lines[n++] = cur;
      cur = word;
    } else {
      cur = trial;
    }
    if (sp < 0) break;
    start = sp + 1;
  }
  if (cur.length() && n < maxLines) lines[n++] = cur;
  return n;
}

// "123" -> "۱۲۳"
static String faDigits(const String &s) {
  String o;
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c >= '0' && c <= '9') {
      uint16_t u = 0x06F0 + (c - '0');
      o += (char)(0xC0 | (u >> 6));
      o += (char)(0x80 | (u & 0x3F));
    } else {
      o += c;
    }
  }
  return o;
}
