#pragma GCC optimize ("no-inline-functions-called-once", "no-reorder-blocks", "no-align-functions", "no-tree-loop-optimize", "no-inline-small-functions", "no-split-wide-types", "no-tree-pre", "no-code-hoisting", "no-ipa-sra", "no-tree-loop-im")   // keeps the game small enough to fit with plain Arduino settings
// Tire launcher prototype v2: pick PSI and ramp, rev, release on the red line, LAUNCH.
#include <Arduboy2.h>
#include "font.h"
#include <ArduboyFX.h>
#include <avr/eeprom.h>
#include "fxdata.h"
#include <ArduboyTones.h>
Arduboy2Base ab;
ARDUBOY_NO_USB
ArduboyTones snd(ab.audio.enabled);

// ---------------------------------------------------------------- tuning
const float RPM_MAX = 8.0f, RED = 7.0f, P_LO = 7.1f, P_HI = 7.8f;   // perfect notch: about 7 frames
const float REV_A = 0.020f, REV_C = 0.0002f;   // rev rate = A + C*rpm^3 per frame (~4 s to the notch)
const float VMAX = 14.0f;                      // px/frame on a perfect launch (1 px = 1 ft)
const float GRAV = 0.12f;
const float MPH = 20.0f;                       // px/frame -> mph shown
const int16_t GROUND = 56;
const int16_t START_X = 30, RAMP_X = 520, RR = 120, R = 13;   // RR: radius of the curved ramp

enum { S_TITLE, S_SOUND, S_TYPE, S_PSI, S_RSEL, S_SET, S_REV, S_ROLL, S_RAMP, S_RUN, S_DONE };
enum { O_NONE, O_CAP, O_TRAMP, O_RING, O_BOOST, O_KICK, O_STACK, O_CAR, O_BLIMP, O_BIRD, O_BUMP };
uint8_t st = S_TITLE;
float shownPsi = 30; uint8_t airT;   // tire pressure screen: animated pressure and air puffs
float rpm, x, y, vx, vy, spin, spd, target, camx, camy;
uint8_t ang = 45, psi = 30, tt = 2;   // tt: tire type
// tire types: SUMMER, WINTER, ALL SEASON, A/T, M/T
const char TN0[] PROGMEM = "SUMMER\0", TN1[] PROGMEM = "WINTER\0", TN2[] PROGMEM = "ALL SEASON\0", TN3[] PROGMEM = "A/T\0", TN4[] PROGMEM = "M/T\0";
// what each tire is good (+) and bad (-) at, one line per \0, matching TSPD/TBNC/TROLL/TLAND below
const char TD0[] PROGMEM = "+FAST START\0-LOW BOUNCE\0", TD1[] PROGMEM = "+SOFT LAND\0-SLOW START\0", TD2[] PROGMEM = "+ROLLS FAR\0ALL ROUND\0",
           TD3[] PROGMEM = "+BOUNCY\0-SHORT ROLL\0", TD4[] PROGMEM = "+BIG BOUNCE\0-SLOW START\0";
PGM_P const TNAME[5] PROGMEM = { TN0, TN1, TN2, TN3, TN4 };
PGM_P const TDA[5] PROGMEM = { TD0, TD1, TD2, TD3, TD4 };
const float TSPD[5] = { 1.06f, 0.97f, 1.0f, 0.98f, 0.95f };          // launch speed
const float TBNC[5] = { -0.07f, -0.02f, 0.0f, 0.04f, 0.08f };        // extra bounce
const float TROLL[5] = { 0.0f, -0.0008f, 0.0012f, -0.0008f, -0.0018f };  // rolls farther (+) or shorter (-)
const float TLAND[5] = { 0.92f, 0.98f, 0.94f, 0.95f, 0.95f };        // speed kept on each landing
uint8_t grade; int16_t gradeT;
uint16_t limT, spinT;
uint8_t landT; float landAmt;   // squash after a bounce
int16_t hitstop, shake, msgT;
float skidEnd;
int32_t dist;
uint16_t caps; uint32_t score, best; bool newRec;
bool feet = true, lock = false;
PGM_P msg;
const char GN[] PROGMEM = "\0PERFECT LAUNCH!\0GREAT LAUNCH\0GOOD LAUNCH\0WEAK...\0LATE!\0WHEELSPIN!\0ENGINE BLOWN!\0";
const char GS[] PROGMEM = "\0PERFECT\0GREAT\0GOOD\0WEAK\0LATE\0SPIN\0BLOWN\0";
PGM_P nth(PGM_P s, uint8_t n) { while (n) { if (!pgm_read_byte(s++)) n--; } return s; }
const uint16_t GP[] = {0, 500, 300, 150, 50, 200, 100, 0};

// ---- fast text: 5x7 font written straight into the screen buffer
int16_t tx_, ty_; bool tw_;           // cursor and colour (tw_ = white ink)
void at(int16_t x, int16_t y) { tx_ = x; ty_ = y; }
void glyph(uint8_t g) {
  for (uint8_t i = 0; i < 6; i++, tx_++) {
    if (tx_ < 0 || tx_ > 127) continue;
    uint8_t b = i < 5 ? pgm_read_byte(FONT + g * 5 + i) : 0;
    if (!b) continue;
    int16_t p = ty_ >> 3; uint8_t o = ty_ & 7;
    uint8_t lo = b << o, hi = o ? b >> (8 - o) : 0;
    if (ty_ < 0) { if (ty_ > -8) { lo = b >> (-ty_); hi = 0; p = 0; } else continue; }
    uint8_t* q = ab.sBuffer + p * 128 + tx_;
    if (p < 8) { if (tw_) *q |= lo; else *q &= ~lo; }
    if (hi && p < 7) { q += 128; if (tw_) *q |= hi; else *q &= ~hi; }
  }
}
void pc(char c) {
  uint8_t g;
  if (c == '\x18') g = 60; else if (c == '\x19') g = 61; else if (c == '\x1B') g = 62; else if (c == '\x1A') g = 63;
  else if (c == '\xF8') g = 59; else if (c >= 32 && c <= 90) g = c - 32; else g = 0;
  glyph(g);
}
void pp(PGM_P s) { char c; while ((c = pgm_read_byte(s++))) pc(c); }
void pn(int32_t v) { char b[11]; uint8_t n = 0; if (v < 0) { pc('-'); v = -v; } do { b[n++] = '0' + v % 10; v /= 10; } while (v); while (n) pc(b[--n]); }
#define PS(s) pp(PSTR(s))
// tiny 3x5 digits (+ M P H): each glyph is 3 columns, low 5 bits
// 0-9, A-Z, then + degree up down space
const uint8_t TINY[] PROGMEM = {31,17,31, 0,31,0, 29,21,23, 21,21,31, 7,4,31, 23,21,29, 31,21,29, 1,1,31, 31,21,31, 23,21,31,
  30,5,30, 31,21,10, 14,17,17, 31,17,14, 31,21,17, 31,5,1, 14,17,29, 31,4,31, 17,31,17, 8,16,15, 31,4,27, 31,16,16, 31,2,31,
  31,1,30, 14,17,14, 31,5,2, 14,17,30, 31,5,26, 18,21,9, 1,31,1, 15,16,15, 7,24,7, 31,8,31, 27,4,27, 3,28,3, 25,21,19,
  4,14,4, 2,5,2, 2,31,2, 8,31,8, 0,0,0, 0,23,0, 0,16,0};
void tiny(int16_t x, int16_t y, uint8_t g) {
  for (uint8_t i = 0; i < 3; i++) {
    uint8_t b = pgm_read_byte(TINY + g * 3 + i); int16_t xx = x + i;
    if (xx < 0 || xx > 127 || y < 0 || y > 58) continue;
    uint8_t* q = ab.sBuffer + (y >> 3) * 128 + xx; uint8_t o = y & 7;
    *q &= ~(b << o); if (o > 3 && (y >> 3) < 7) q[128] &= ~(b >> (8 - o));
  }
}
int16_t tinyStr(int16_t x, int16_t y, PGM_P s) {
  char c;
  while ((c = pgm_read_byte(s++))) {
    uint8_t g = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'Z' ? c - 'A' + 10 : c == '+' ? 36 : c == '*' ? 37 : c == '^' ? 38 : c == 'v' ? 39 : c == '!' ? 41 : c == '.' ? 42 : 40;
    tiny(x, y, g); x += 4;
  }
  return x;
}
int16_t tinyNum(int16_t x, int16_t y, uint16_t v) {
  char b[6]; uint8_t n = 0; do { b[n++] = v % 10; v /= 10; } while (v);
  while (n) { tiny(x, y, b[--n]); x += 4; } return x;
}
// fast filled rectangle: writes whole bytes instead of single pixels
void frect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t col) {
  if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
  if (x + w > 128) w = 128 - x; if (y + h > 64) h = 64 - y;
  if (w <= 0 || h <= 0) return;
  for (int16_t yy = y; yy < y + h;) {
    uint8_t pg = yy >> 3, o = yy & 7, n = min(8 - o, y + h - yy);
    uint8_t m = ((1 << n) - 1) << o;
    uint8_t* q = ab.sBuffer + pg * 128 + x;
    if (col) for (int16_t i = 0; i < w; i++) q[i] |= m; else for (int16_t i = 0; i < w; i++) q[i] &= ~m;
    yy += n;
  }
}
int8_t SINT[64];
inline float tsin(uint8_t i) { return SINT[i & 63] / 127.0f; }
inline float tcos(uint8_t i) { return SINT[(i + 16) & 63] / 127.0f; }
uint8_t HILL[128];
struct Pt { float x, y, r, vx, vy; uint8_t t; } sm[14];
struct Ob { float x; uint8_t type, w, h; } ob[10];
float nextOb, nextSign;   // nextSign: the next distance sign ahead of the course builder

void puff(float px, float py, float vx_, float vy_) {
  for (auto& p : sm) if (!p.t) { p.x = px; p.y = py; p.r = 1 + random(2); p.vx = vx_; p.vy = vy_; p.t = 18 + random(14); return; }
}
float rampW() { return RR * sin(ang * PI / 180); }
float rampH() { return RR * (1 - cos(ang * PI / 180)); }
uint8_t rprof[110], rprofAng;   // ramp surface height per column
float phi;                      // how far around the curve the tire is
void makeProfile() {
  if (rprofAng == ang) return; rprofAng = ang;
  int16_t w = (int16_t)rampW();
  for (int16_t i = 0; i < 110; i++) rprof[i] = i <= w ? (uint8_t)(RR - sqrt((float)RR * RR - (float)i * i)) : 0;
}
float bounceE() { return 0.45f + (psi - 15) * 0.010f + TBNC[tt]; }   // high PSI bounces more; 0.38 .. 0.83, never close to a perfect bounce
float rollF()   { return 0.9935f + (psi - 15) * 0.0001f + TROLL[tt]; }    // like real tires: firm rolls on, soft squishes and stops sooner

void spawn(Ob& o) {
  uint8_t r = random(100);
  while (nextSign + 14 < nextOb) nextSign += 250;            // leave a little room around each distance sign
  if (nextOb > nextSign - 74) nextOb = nextSign + 14;
  o.x = nextOb;
  o.w = 0; o.h = 0;
  if (r < 6) { o.type = O_BUMP; o.w = 12; }
  else if (r < 18) { o.type = O_CAP; o.w = 8; o.h = 14 + random(30); }
  else if (r < 27) { o.type = O_TRAMP; o.w = 24; }
  else if (r < 38) { o.type = O_RING; o.w = 6; o.h = 16 + random(26); }
  else if (r < 48) { o.type = O_BOOST; o.w = 18; }
  else if (r < 58) { o.type = O_KICK; o.w = 28 + random(20); o.h = 12 + random(14); }   // big kicker ramps
  else if (r < 65) { o.type = O_STACK; o.w = 20; o.h = 3 + random(3); }                 // 3..5 tires high
  else if (r < 72) { o.type = O_CAR; o.w = 50; }
  else if (r < 80) { o.type = O_BLIMP; o.w = 60; o.h = 46 + random(30); }
  else { o.type = O_BIRD; o.w = 17; o.h = 18 + random(140); }
#ifdef TESTPROPS
  { static uint8_t q; static const uint8_t T[] = { O_KICK, O_STACK, O_CAR, O_KICK, O_STACK, O_CAR }; o.type = T[q++ % 6];
    o.w = o.type == O_CAR ? 50 : o.type == O_BLIMP ? 60 : o.type == O_STACK ? 20 : o.type == O_KICK ? 44 : 17;
    o.h = o.type == O_BIRD ? 30 + random(60) : o.type == O_BLIMP ? 50 : o.type == O_STACK ? 5 : o.type == O_KICK ? 24 : 0; }
#endif
  nextOb += (o.type == O_BIRD ? 40 + random(80) : 150 + random(200));   // birds come closer together
}

void reset() {
  st = S_SET; rpm = 0; x = START_X; y = 0; vx = vy = 0; spin = 0; spd = 0; grade = 0; gradeT = 0;
  limT = 0; spinT = 0; landT = 0; hitstop = 0; shake = 0; skidEnd = 0; dist = 0; feet = true;
  camx = 0; camy = 0; caps = 0; score = 0; msgT = 0;
  for (auto& p : sm) p.t = 0;
  nextSign = RAMP_X + rampW() + 250; nextOb = RAMP_X + 220;
  for (auto& o : ob) spawn(o);
}

void setup() {
  ab.boot(); ab.display(); FX::begin(FX_DATA_PAGE); ab.systemButtons(); ab.audio.begin(); ab.waitNoButtons();
  ab.setFrameRate(60);
  for (uint8_t i = 0; i < 64; i++) SINT[i] = (int8_t)round(sin(i * 2 * PI / 64) * 127);
  randomSeed(ab.generateRandomSeed());
  best = eeprom_read_dword((uint32_t*)112) ^ 0x71BE7E57UL; if (best > 9999999UL) best = 0;
  for (int16_t w = 0; w < 128; w++) { int16_t a = w % 64, b = (w + 17) % 32; HILL[w] = 2 + (32 - abs(a - 32)) / 2 + (16 - abs(b - 16)) / 3; }
  reset(); st = S_TITLE;
}

// ---------------------------------------------------------------- drawing
int16_t SX(float wx) { return (int16_t)(wx - camx); }
int16_t SY(float wy) { return (int16_t)(GROUND - wy + camy); }

// The tire guy. (cx, by) = middle of his bottom edge; rx, ry = his half width and height.
void ellipseDots(int16_t cx, int16_t cy, float rx, float ry, uint8_t step, uint8_t off, uint8_t col) {
  int16_t qx = (int16_t)(rx * 16), qy = (int16_t)(ry * 16);
  for (uint8_t a = off; a < 64 + off; a += step)
    ab.drawPixel(cx + ((SINT[(a + 16) & 63] * qx + 1024) >> 11), cy + ((SINT[a & 63] * qy + 1024) >> 11), col);
}
void drawTire(int16_t cx, int16_t by, int16_t rx, int16_t ry) {
  int16_t cy = by - ry;
  {                                                              // solid rubber, row by row
    int32_t rx2 = (int32_t)rx * rx, ry2 = (int32_t)ry * ry; int16_t hw = rx;
    for (int16_t dy = 0; dy <= ry; dy++) {
      while (hw > 0 && (int32_t)hw * hw * ry2 > rx2 * (ry2 - (int32_t)dy * dy) + ry2 / 2) hw--;
      frect(cx - hw, cy + dy, 2 * hw + 1, 1, BLACK);
      if (dy) frect(cx - hw, cy - dy, 2 * hw + 1, 1, BLACK);
    }
  }
  uint8_t si = (uint8_t)(spin * (64 / (2 * PI)));
  {                                                              // tread for this tire type, from FX (8 turn frames per block)
    static const uint8_t TBLK[5] = { 16, 32, 16, 16, 8 };
    uint8_t f = (uint8_t)(-(int32_t)(spin * TBLK[tt] * (8 / (2 * PI)))) & 7;
    if (rx > 18) FX::drawBitmap(cx - 25, cy - 25, FX_TREADB, tt * 8 + f, dbmMasked);
    else FX::drawBitmap(cx - 16, cy - 16, FX_TREAD, tt * 8 + f, dbmMasked);
  }
  float hx = rx * 0.62f, hy = ry * 0.62f;
  ellipseDots(cx, cy, hx, hy, 1, 0, WHITE);                       // hubcap ring
  for (uint8_t k = 0; k < 5; k++) {                              // lug nuts
    uint8_t a = si + (k * 64) / 5;
    ab.drawPixel(cx + ((SINT[(a + 16) & 63] * (int16_t)(hx * 12.5f) + 1024) >> 11), cy + ((SINT[a & 63] * (int16_t)(hy * 12.5f) + 1024) >> 11), WHITE);
  }
  // ---- face (stays upright)
  int8_t lx = 1, ly = 0;                                          // where he's looking
  if (st == S_SET) lx = ((ab.frameCount >> 6) & 1) ? 1 : 0;
  if (st == S_RUN) ly = vy > 2 ? -1 : (vy < -2 ? 1 : 0);
  bool blink = (ab.frameCount & 127) < 5 && st != S_REV;
  int16_t ey = cy - 4;
  if (st == S_REV && rpm > RED) {                                // straining: squeezed shut, angry brows
    ab.drawLine(cx - 6, ey + 1, cx - 2, ey + 3, WHITE); ab.drawLine(cx + 6, ey + 1, cx + 2, ey + 3, WHITE);
  } else if (blink) {
    ab.drawFastHLine(cx - 6, ey + 2, 4, WHITE); ab.drawFastHLine(cx + 2, ey + 2, 4, WHITE);
  } else {
    frect(cx - 6, ey, 4, 5, WHITE); frect(cx + 2, ey, 4, 5, WHITE);
    frect(cx - 5 + lx, ey + 1 + ly + (ly < 0 ? 0 : 1), 2, 2, BLACK); frect(cx + 3 + lx, ey + 1 + ly + (ly < 0 ? 0 : 1), 2, 2, BLACK);
    if (st == S_REV && rpm > 3) { ab.drawLine(cx - 7, ey - 2, cx - 3, ey - 1, WHITE); ab.drawLine(cx + 6, ey - 2, cx + 2, ey - 1, WHITE); }
  }
  int16_t my = cy + 3;
  if (st == S_REV && rpm > 3) {                                  // gritted teeth
    frect(cx - 3, my, 7, 3, WHITE); ab.drawFastHLine(cx - 3, my + 1, 7, BLACK);
    ab.drawFastVLine(cx - 1, my, 3, BLACK); ab.drawFastVLine(cx + 1, my, 3, BLACK);
  } else if (st == S_RUN && y > 6 && vy > 0) {                   // "woooo"
    frect(cx - 1, my, 3, 4, WHITE); ab.drawPixel(cx, my + 1, BLACK); ab.drawPixel(cx, my + 2, BLACK);
  } else if (st == S_RUN && vy < -3) {                           // uh oh
    ab.drawFastHLine(cx - 2, my + 1, 5, WHITE); ab.drawPixel(cx - 3, my + 2, WHITE); ab.drawPixel(cx + 3, my + 2, WHITE);
  } else {                                                       // big grin
    ab.drawFastHLine(cx - 3, my + 1, 7, WHITE); ab.drawFastHLine(cx - 2, my + 2, 5, WHITE);
    ab.drawPixel(cx - 4, my, WHITE); ab.drawPixel(cx + 4, my, WHITE);
  }
  if (feet) {                                                    // sneakers at the start line
    frect(cx - 9, by, 7, 3, BLACK); ab.drawFastHLine(cx - 9, by + 2, 7, WHITE); ab.drawPixel(cx - 10, by + 2, BLACK);
    frect(cx + 2, by, 7, 3, BLACK); ab.drawFastHLine(cx + 2, by + 2, 7, WHITE); ab.drawPixel(cx + 9, by + 2, BLACK);
  }
}

void drawTach() {                                                // big rev counter: shakes as the revs climb, slides away once he's rolling
  int8_t k = st == S_REV ? (rpm > RED ? 2 : rpm > 4.0f ? 1 : 0) : 0;
  int16_t ox = 66 + random(-k, k + 1), oy = 1 + random(-k, k + 1);
  if (st == S_ROLL) oy -= (int16_t)(x - START_X) / 2;
  FX::drawBitmap(ox, oy, FX_TACH, 0, dbmMasked);
  int16_t cx = ox + 31, cy = oy + 31;
  float a = (225 - rpm / RPM_MAX * 270) * PI / 180, c = cos(a), sn = sin(a);
  ab.drawLine(cx, cy, cx + c * 23, cy - sn * 23, BLACK);
  ab.drawLine(cx + 1, cy, cx + 1 + c * 22, cy - sn * 22, BLACK);
  ab.fillCircle(cx, cy, 2, BLACK);
}


void drawOb(Ob& o) {
  int16_t sx = SX(o.x), gy = SY(0);
  if (sx > 132 || sx + o.w < -4) return;
  switch (o.type) {
    case O_CAP: {
      int16_t cy = SY(o.h) + ((ab.frameCount >> 3) & 1);
      FX::drawBitmap(sx, cy - 4, FX_HUBCAP, 0, dbmMasked);
      break; }
    case O_TRAMP:                                     // trampoline (sags after a bounce)
      FX::drawBitmap(sx, gy - 7, FX_TRAMP, o.h != 0, dbmMasked);
      if (o.h) o.h--;
      break;
    case O_RING:                                      // boost ring in the air
      FX::drawBitmap(sx, SY(o.h) - 14, FX_RING, (ab.frameCount >> 3) & 1, dbmMasked);
      break;
    case O_BOOST:
      FX::drawBitmap(sx, gy - 4, FX_BOOST, (ab.frameCount >> 2) & 1, dbmMasked);
      break;
    case O_STACK:                                     // a tall stack of old tires, 3..5 high
      FX::drawBitmap(sx, gy - 35, FX_STACK, o.h - 3, dbmMasked);
      break;
    case O_BUMP:                                      // speed bump with warning stripes
      FX::drawBitmap(sx, gy - 4, FX_BUMP, 0, dbmMasked);
      break;
    case O_CAR:                                       // a big pickup truck
      FX::drawBitmap(sx, gy - 21, FX_TRUCK, 0, dbmMasked);
      break;
    case O_BLIMP:                                     // a big blimp to bounce off
      FX::drawBitmap(sx, SY(o.h) - 1, FX_BLIMP, 0, dbmMasked);
      break;
    case O_BIRD:                                      // birds: hit them for points
      FX::drawBitmap(sx, SY(o.h) - 5, FX_BIRD, (ab.frameCount >> 3) & 1, dbmMasked);
      break;
    case O_KICK: {                                    // kicker ramp, solid with a lip
      for (int16_t i = 0; i <= o.w; i++) {
        int16_t hh = i * o.h / o.w;
        ab.drawFastVLine(sx + i, gy - hh, hh, (i % 6 == 3 && hh > 4) ? WHITE : BLACK);
        if (i % 6 == 3 && hh > 4) { ab.drawPixel(sx + i, gy - hh, BLACK); ab.drawPixel(sx + i, gy - hh + 1, BLACK); }
      }
      frect(sx + o.w - 1, gy - o.h - 2, 3, o.h + 2, BLACK);
      break; }
  }
}

// The launch ramp: plank deck on a curve, posts with cross braces, a lip and a flag.
void drawRamp(int16_t x0, int16_t gy) {
  makeProfile();
  int16_t w = (int16_t)rampW();
  if (x0 + w < -6 || x0 > 132) return;
  int16_t prev = gy;
  for (int16_t i = 0; i <= w; i++) {
    int16_t sx = x0 + i, top = gy - rprof[i], y0 = min(top, prev);
    prev = top;
    if (sx < 0 || sx > 127) continue;
    ab.drawFastVLine(sx, y0 - 2, top - y0 + 4, BLACK);          // deck, 3 px thick
    if (i % 9 == 4) ab.drawPixel(sx, top - 1, WHITE);             // plank seams
    if (i % 8 == 6 && rprof[i] > 2) {
      ab.drawFastVLine(sx, top + 2, gy - top - 2, BLACK);         // post
      if (i + 8 <= w) ab.drawLine(sx, top + 2, sx + 8, gy - 1, BLACK);     // brace
      if (i >= 8 && rprof[i - 8] > 2) ab.drawLine(sx, gy - 1, sx - 8, gy - rprof[i - 8] + 2, BLACK);
    }
  }
  int16_t lx = x0 + w, top = gy - rprof[w];
  frect(lx - 1, top - 2, 3, gy - top + 2, BLACK);                // back post
  frect(lx - 4, gy - 1, 8, 2, BLACK);                            // footing
  if (top < 22 && st == S_RSEL) return;                          // steep ramp on the setup screen: no room for the flag
  ab.drawFastVLine(lx + 1, top - 10, 8, BLACK);                  // flag pole + waving flag
  uint8_t f = (ab.frameCount >> 3) & 1;
  ab.drawLine(lx + 2, top - 10, lx + 6, top - 9 + f, BLACK); ab.drawLine(lx + 2, top - 7, lx + 6, top - 9 + f, BLACK);
  ab.drawFastVLine(lx + 2, top - 10, 4, BLACK);
}

void drawWorld() {
  int16_t gy = SY(0);
  // clouds drift behind at half speed
  int16_t cbase = (int16_t)(camx * 0.5f) / 80;
  for (int16_t k = cbase; k < cbase + 3; k++) {
    int16_t cx = k * 80 + (k * 37) % 30 - (int16_t)(camx * 0.5f), cy = ((k * 29) % 40 + (int16_t)(camy * 0.4f)) % 90 - 24;
    if (cy < gy - 30) FX::drawBitmap(cx, cy, FX_CLOUD, 0, dbmMasked);           // they repeat going up, so the sky never runs out
  }
  float sp = st == S_RUN ? vx : spd;
  if (gy < 64) {
    ab.drawFastHLine(0, gy, 128, BLACK);
    // road dashes (stretch into streaks when fast)
    int16_t dl = 4 + (int16_t)(sp * 1.6f);
    for (int16_t k = (int16_t)(camx / 19); k < (int16_t)(camx / 19) + 9; k++) {
      int16_t dx = SX(k * 19 + (k * 7) % 5);
      ab.drawFastHLine(dx, gy + 4, dl, BLACK);
    }
  }
  // distance signs every 250 ft after the ramp
  float r0 = RAMP_X + rampW();
  int32_t k0 = (int32_t)((camx - r0) / 250); if (k0 < 1) k0 = 1;
  for (int32_t k = k0; k < k0 + 2; k++) {
    int16_t sx = SX(r0 + k * 250); if (sx < -24 || sx > 140) continue;
    uint16_t n = k * 250; int16_t w = (n >= 10000 ? 5 : n >= 1000 ? 4 : 3) * 4 + 3;   // sign sized to its number
    ab.drawFastVLine(sx, gy - 9, 9, BLACK);
    frect(sx - w / 2, gy - 18, w, 9, WHITE); ab.drawRect(sx - w / 2, gy - 18, w, 9, BLACK);
    tinyNum(sx - w / 2 + 2, gy - 16, n);
  }
  // start line
  int16_t fx = SX(START_X + 16);
  for (uint8_t j = 0; j < 3; j++) for (uint8_t i = 0; i < 2; i++)
    if ((i + j) & 1) frect(fx + i * 2, gy + 1 + j * 2, 2, 2, BLACK);
  ab.drawFastVLine(fx, gy - 26, 26, BLACK);
  for (uint8_t j = 0; j < 3; j++) for (uint8_t i = 0; i < 4; i++)
    if ((i + j) & 1) frect(fx + 1 + i * 2, gy - 26 + j * 2, 2, 2, BLACK);
  if (skidEnd > START_X) frect(SX(START_X), gy - 1, (int16_t)(skidEnd - START_X), 2, BLACK);
  drawRamp(SX(RAMP_X), gy);
  for (auto& o : ob) drawOb(o);
}

void drawSmoke() {
  for (auto& p : sm) if (p.t) {
    int16_t sx = SX(p.x), sy = SY(p.y);
    ab.fillCircle(sx, sy, p.r, WHITE); ab.drawCircle(sx, sy, p.r, BLACK);
  }
}

void banner(PGM_P s, int16_t bx, int16_t by) {          // small boxed call-out
  int16_t w = strlen_P(s) * 4 + 5;
  if (bx + w > 127) bx = 127 - w;
  frect(bx, by, w, 9, WHITE); ab.drawRect(bx, by, w, 9, BLACK);
  tinyStr(bx + 3, by + 2, s);
}

void printDist(int32_t d) {
  if (d >= 5280) {                                               // a mile or more: 1.38 MI, so it fits the score box
    int32_t h = d * 100 / 5280; pn(h / 100); pc('.'); if (h % 100 < 10) pc('0'); pn(h % 100); PS(" MI");
  } else { pn(d); PS(" FT"); }
}

// end of a run: GAME OVER or NEW RECORD, then distance and score, all centred
void centred(int16_t y, void (*f)()) { at(0, -50); f(); int16_t w = tx_ - 1; at(64 - w / 2, y); f(); }
void drawStats() {
  frect(4, 4, 120, 56, WHITE); ab.drawRect(4, 4, 120, 56, BLACK); ab.drawRect(6, 6, 116, 52, BLACK);
  if (newRec) FX::drawBitmap(10, 14, FX_NEWREC, 0, dbmMasked);
  else FX::drawBitmap(11, 14, FX_GAMEOVER, 0, dbmMasked);
  centred(36, [] { PS("DISTANCE "); printDist(dist); });
  centred(46, [] { PS("SCORE "); pn(score); });
}

// ---------------------------------------------------------------- logic
void say(PGM_P s, int16_t t) { msg = s; msgT = t; }

void launch(uint8_t g) {
  grade = g; gradeT = 55; feet = false;
  float q = 0;
  switch (g) {
    case 1: q = 1.00f; hitstop = 16; shake = 3; snd.tone(NOTE_C6, 60, NOTE_E6, 60, NOTE_G6, 140); break;
    case 2: q = 0.86f; shake = 2; snd.tone(NOTE_C6, 60, NOTE_E6, 100); break;
    case 3: q = 0.72f; snd.tone(NOTE_C5, 90); break;
    case 4: q = 0.25f + 0.055f * rpm; snd.tone(NOTE_C4, 120); break;
    case 5: q = 0.80f; snd.tone(NOTE_A4, 80); break;
    case 6: q = 0.65f; spinT = 50; break;
    case 7: q = 0.18f; spinT = 0; snd.tone(NOTE_C3, 200, NOTE_A2, 300); break;
  }
  target = VMAX * q * TSPD[tt] * (1 + (30 - psi) * 0.003f);   // soft tire: more rubber on the road, better launch grip
  st = S_ROLL;
  if (g == 1) for (uint8_t k = 0; k < 6; k++) puff(x - 10, 2 + k, -1.5f - random(10) * 0.1f, 0.2f + k * 0.1f);
}


void finish() {
  score = GP[grade] + dist / 2 + (uint32_t)caps * 100;
  newRec = score > best; if (newRec) { best = score; eeprom_update_dword((uint32_t*)112, best ^ 0x71BE7E57UL); }   // best score kept when switched off
  st = S_DONE;
}

void groundStuff() {        // things on the road the tire touches
  for (auto& o : ob) {
    if (x + R * 0.6f < o.x || x - R * 0.6f > o.x + o.w) continue;
    switch (o.type) {
      case O_BOOST:
        vx += 2.5f; if (vx > 15) vx = 15; o.type = O_NONE; say(PSTR("BOOST!"), 40); snd.tone(NOTE_C5, 40, NOTE_C6, 80);
        break;
      case O_KICK:
        { float kv = vx * o.h * 1.6f / o.w + 1; if (vy < kv) { vy = kv; y = 1; say(PSTR("KICKER!"), 40); snd.tone(NOTE_G5, 60); } }
        break;
      case O_STACK:
        vy = max(-vy, 5.0f + o.h * 0.9f); y = 1; shake = 2; say(PSTR("TIRE STACK!"), 40); snd.tone(NOTE_C4, 30, NOTE_G4, 30, NOTE_G5, 70);
        break;
      case O_CAR:
        vy = max(-vy, 7.0f); y = 1; vx += 0.5f; say(PSTR("ROOF HOP!"), 40); snd.tone(NOTE_E4, 40, NOTE_E5, 60);
        break;
      case O_BUMP:
        if (vy < 3) { vy = 3 + vx * 0.2f; y = 1; say(PSTR("SPEED BUMP!"), 40); snd.tone(NOTE_A3, 30, NOTE_A4, 40); }
        break;
      case O_TRAMP:
        vy = max(-vy, 7.0f); y = 1; o.h = 12; shake = 2;
        say(PSTR("BOING!"), 40); snd.tone(NOTE_C4, 40, NOTE_C5, 40, NOTE_C6, 60);
        break;
    }
  }
}

void update() {
  if (hitstop) { hitstop--; return; }
  if (gradeT) gradeT--;
  if (msgT) msgT--;
  switch (st) {
    case S_TITLE:
      if (ab.justPressed(B_BUTTON) || ab.justPressed(A_BUTTON)) st = S_SOUND;
      break;
    case S_SOUND:                                       // sound on or off (saved), then tire type
      if (ab.justPressed(UP_BUTTON) || ab.justPressed(DOWN_BUTTON)) { ab.audio.toggle(); ab.audio.saveOnOff(); snd.tone(NOTE_G5, 80); }
      if (ab.justPressed(B_BUTTON)) st = S_TYPE;
      if (ab.justPressed(A_BUTTON)) st = S_TITLE;
      break;
    case S_TYPE:
      if (ab.justPressed(UP_BUTTON)) { tt = (tt + 4) % 5; snd.tone(NOTE_E5, 30); }
      if (ab.justPressed(DOWN_BUTTON)) { tt = (tt + 1) % 5; snd.tone(NOTE_C5, 30); }
      spin += 0.04f;
      if (ab.justPressed(B_BUTTON)) { st = S_PSI; shownPsi = psi; }
      if (ab.justPressed(A_BUTTON)) st = S_SOUND;
      break;
    case S_PSI:
      if (ab.justPressed(UP_BUTTON) && psi < 45) { psi += 5; airT = 24; snd.tone(NOTE_C5, 30, NOTE_E5, 30); }
      if (ab.justPressed(DOWN_BUTTON) && psi > 15) { psi -= 5; airT = 24; snd.tone(1800, 200); }
      shownPsi += (psi - shownPsi) * 0.15f; if (airT) airT--;
      if (ab.justPressed(B_BUTTON)) st = S_RSEL;
      if (ab.justPressed(A_BUTTON)) st = S_TYPE;
      break;
    case S_RSEL:
      if (ab.justPressed(UP_BUTTON) && ang < 60) ang += 5;
      if (ab.justPressed(DOWN_BUTTON) && ang > 15) ang -= 5;
      if (ab.justPressed(B_BUTTON)) { reset(); lock = true; }
      if (ab.justPressed(A_BUTTON)) st = S_PSI;
      break;
    case S_SET:
      if (ab.justPressed(A_BUTTON)) { st = S_RSEL; break; }
      if (!ab.pressed(B_BUTTON)) lock = false;
      else if (!lock) { st = S_REV; feet = false; }
      break;
    case S_REV:
      if (ab.pressed(B_BUTTON)) {
        if (rpm < RPM_MAX) {
          rpm += REV_A + REV_C * rpm * rpm * rpm + (random(5) - 2) * 0.006f;
          if (rpm >= RPM_MAX) rpm = RPM_MAX;
        } else {
          rpm = RPM_MAX - 0.3f * (ab.frameCount & 4 ? 1 : 0);
          if (++limT > 150) { rpm = 0; launch(7); break; }
        }
        spin += rpm * 0.14f;
        if (rpm > 2.5f && random(10) < rpm) puff(x - 10, 1, -0.3f - random(10) * 0.06f, 0.15f + random(5) * 0.04f);
        shake = rpm > 6.0f ? (rpm > RED ? 2 : 1) : 0;
        if (limT && (ab.frameCount & 1)) snd.tone(160 + random(40), 20);
        else snd.tone((uint16_t)(50 + rpm * 75 + random(14)), 20);
      } else {
        shake = 0; snd.noTone();
        if (limT) launch(6);
        else if (rpm >= P_LO && rpm <= P_HI) launch(1);
        else if (rpm > P_HI) launch(5);
        else if (rpm >= 6.6f) launch(2);
        else if (rpm >= 5.5f) launch(3);
        else launch(4);
        if (grade == 1) skidEnd = START_X + 1;
      }
      break;
    case S_ROLL:
      if (rpm > 0) { rpm -= 0.25f; if (rpm < 0) rpm = 0; }
      if (shake && (ab.frameCount & 7) == 0) shake--;
      if (spinT) {
        spinT--; spin += 1.0f; spd += 0.02f;
        if (random(2)) puff(x - 10, 1, -0.6f, 0.2f);
        snd.tone(140 + random(60), 20);
      } else {
        spd += (target - spd) * (grade == 1 ? 0.10f : 0.05f);
        if (grade == 1 && x < START_X + 110) { skidEnd = x; if (random(2) == 0) puff(x - 10, 1, -1.2f, 0.3f); }
        if (grade != 7) snd.tone((uint16_t)(90 + spd * 50), 20);
      }
      x += spd; spin += spd / 13;
      if (x >= RAMP_X) { st = S_RAMP; phi = 0; }
      break;
    case S_RAMP: {
      float th = ang * PI / 180;
      phi += spd / RR; if (phi > th) phi = th;
      x = RAMP_X + RR * sin(phi); y = RR * (1 - cos(phi)); spin += spd / 13;
      snd.tone((uint16_t)(90 + spd * 50 + phi * 300), 20);
      if (phi >= th) {
        vx = spd * cos(th); vy = spd * sin(th); st = S_RUN; shake = 3; hitstop = 4;
        snd.tone(NOTE_G5, 40, NOTE_C6, 80); say(PSTR("LIFTOFF!"), 50);
      }
      break;
    }
    case S_RUN: {
      vy -= GRAV; vx *= 0.9993f; x += vx; y += vy; spin += vx / 13;   // a little air drag, so every run comes to an end
      if (shake && (ab.frameCount & 7) == 0) shake--;
      for (auto& o : ob) {
        if (o.type == O_CAP && fabs(x - (o.x + 4)) < 12 && fabs(y + R - o.h) < 13) {
          o.type = O_NONE; caps++; say(PSTR("+100 HUBCAP"), 40); snd.tone(NOTE_E6, 40, NOTE_A6, 60);
        }
        if (o.type == O_BLIMP && vy < 0 && fabs(x - (o.x + 27)) < 32 && y - o.h > -3 && y - o.h < 8) {
          vy = 8; say(PSTR("BLIMP BOUNCE!"), 40); snd.tone(NOTE_G3, 50, NOTE_G4, 80); shake = 1;
        }
        if (o.type == O_BIRD && fabs(x - (o.x + 8)) < 13 && fabs(y + R - o.h) < 13) {
          o.type = O_NONE; caps++; say(PSTR("+100 BIRDIE!"), 40); snd.tone(NOTE_A5, 30, NOTE_E6, 30, NOTE_A6, 50);
          for (uint8_t k = 0; k < 4; k++) puff(o.x + 8, o.h, (k - 1.5f) * 0.6f, 0.3f + (k & 1) * 0.4f);
        }
        if (o.type == O_RING && fabs(x - (o.x + 3)) < 10 && fabs(y + R - o.h) < 14) {
          o.type = O_NONE; vx += 2.0f; if (vx > 15) vx = 15; if (vy < 2) vy = 2; caps++;
          say(PSTR("RING BOOST!"), 40); snd.tone(NOTE_G5, 40, NOTE_D6, 40, NOTE_G6, 60); shake = 1;
        }
      }
      if (y <= 0 && vy <= 0) {
        y = 0; groundStuff(); if (st != S_RUN) break;
        float e = bounceE();
        if (vy < -1.2f) {                                   // landing / bounce
          landAmt = min(5.0f, -vy * 0.7f); landT = 8;
          vy = -vy * e; vx *= TLAND[tt] + (30 - psi) * 0.001f;   // soft tire soaks up landings shake = vy > 3 ? 2 : 1;
          snd.tone(NOTE_C3, 40); puff(x - 6, 0, -0.5f, 0.4f); puff(x + 6, 0, 0.5f, 0.4f);
        } else {                                            // rolling
          if (vy < 0) vy = 0;
          vx = vx * rollF() - 0.004f;
          if (vx < 0.08f) { vx = 0; finish(); }
        }
      }
      dist = max((int32_t)0, (int32_t)(x - (RAMP_X + rampW())));
      if (st == S_RUN && vx > 0.5f) snd.tone((uint16_t)(70 + vx * 30), 20);
      if (landT) landT--;
      break;
    }
    case S_DONE:
      if (ab.justPressed(B_BUTTON)) { reset(); lock = true; }
      if (ab.justPressed(A_BUTTON)) { st = S_TITLE; spd = 0; }   // inner: back to the title
      break;
  }
  // keep the course filled ahead
  for (auto& o : ob) { if (o.type == O_BIRD && st >= S_SET && st != S_DONE) o.x -= 0.6f; if (o.x + o.w < camx - 40) spawn(o); }
  for (auto& p : sm) if (p.t) { p.t--; p.x += p.vx; p.y += p.vy; if ((p.t & 7) == 0 && p.r < 5) p.r++; }
  float lead = st >= S_ROLL ? 30 : 40;
  float want = x - lead; if (want < 0) want = 0;
  float wy = y - (GROUND - 34); if (wy < 0) wy = 0;   // keep him in view when he flies high
  if (st >= S_ROLL) { camx = want; camy = wy; }        // locked on the tire once he's rolling
  else { camx += (want - camx) * 0.12f; camy += (wy - camy) * 0.35f; }
}


void boldC(int16_t cx, int16_t y, PGM_P s) {           // bold text centred on cx: each letter twice, 1 px apart, with room between letters
  int16_t x = cx - strlen_P(s) * 7 / 2;
  for (char c; (c = pgm_read_byte(s++)); x += 7) { at(x, y); pc(c); at(x + 1, y); pc(c); }
}
void hint(PGM_P s) { if ((ab.frameCount >> 5) & 1) { tw_ = true; boldC(64, 53, s); tw_ = false; } }   // white, on the road
void plist(int16_t x, int16_t y, PGM_P a, bool c) {     // lines separated by \0, ending with an empty one; c = centre on x
  for (; pgm_read_byte(a); a += strlen_P(a) + 1, y += 8) { at(c ? x - strlen_P(a) * 3 : x, y); pp(a); }
}
void upDown() { int8_t bob = (ab.frameCount >> 4) & 1; at(92, 13 - bob); pc('\x18'); at(92, 31 + bob); pc('\x19'); }
void road(int16_t y) { frect(0, y, 128, 64 - y, BLACK); for (int16_t x = 3; x < 128; x += 13) frect(x, 61, 7, 1, WHITE); }   // black road, white dashes
void frameUp(PGM_P t) { frect(0, 0, 128, 11, BLACK); tw_ = true; boldC(64, 2, t); tw_ = false; road(57); }   // setup screens: title bar on top, road below

void drawMenus() {
  if (st == S_TITLE) {
    FX::drawBitmap(1, 4, FX_TITLE, 0, dbmMasked);
    road(51);
    FX::drawBitmap(90, 6 - ((ab.frameCount >> 3) & 1), FX_HERO, (ab.frameCount & 127) < 6, dbmMasked);   // big tire guy, hops and blinks
    hint(PSTR("PRESS TO START"));
  } else if (st == S_SOUND) {
    bool on = ab.audio.enabled();
    uint8_t ph = (ab.frameCount >> 2) & 3;            // ON: bouncing to the music. OFF: asleep, z z z
    FX::drawBitmap(12, 13 - (on && (ph & 1) ? 4 : 0), FX_HERO, on ? (ph ? 0 : 2) : 3, dbmMasked);
    if (!on) boldC(54, 24 - ((ab.frameCount >> 4) & 3) * 2, PSTR("Z"));
    upDown();
    boldC(95, 22, on ? PSTR("ON") : PSTR("OFF"));
    boldC(95, 43, on ? PSTR("VROOM!") : PSTR("SHHH..."));
    frameUp(PSTR("SOUND"));
  } else if (st == S_TYPE) {
    drawTire(30, 57, 22, 22);
    upDown();
    plist(95, 22, (PGM_P)pgm_read_ptr(&TNAME[tt]), 1); plist(94, 40, (PGM_P)pgm_read_ptr(&TDA[tt]), 1);
    frameUp(PSTR("TIRE TYPE"));
  } else if (st == S_PSI) {
    at(84, 14); pc('\x18'); PS(" "); pn(psi); PS(" "); pc('\x19');
    at(92, 23); PS("PSI");
    { uint8_t q = psi < 25 ? 0 : psi > 35 ? 2 : 1;                 // centred under the PSI
      static const char L0[] PROGMEM = "+GRIP\0+SOFT LAND\0-LESS ROLL\0", L1[] PROGMEM = "BALANCED\0", L2[] PROGMEM = "+BOUNCY\0+ROLLS FAR\0-LESS GRIP\0";
      PGM_P a = q == 0 ? L0 : q == 1 ? L1 : L2;
      plist(95, 33, a, 1); }
    FX::drawBitmap(11, 11, FX_PSITIRE, tt * 31 + (uint8_t)(shownPsi - 15 + 0.5f), dbmNormal);   // squashed .. round, from FX
    if (airT) {                                         // air rushing in or out at the valve
      static const int8_t VX[7] = {49,48,48,47,47,46,46}, VY[7] = {35,33,32,29,28,26,23};   // valve position at each PSI step
      uint8_t q = (psi - 15) / 5; int16_t vx0 = VX[q], vy0 = VY[q], d = (24 - airT) / 3;
      for (uint8_t i = 0; i < 3; i++) ab.drawFastHLine(vx0 + 2 + (psi > shownPsi ? 6 - d : d) + i * 2, vy0 - 7 + i * 3, 3, BLACK);
    }
    frameUp(PSTR("TIRE PRESSURE"));
  } else {
    at(1, 14); pn(ang); ab.drawRect(tx_, 14, 3, 3, BLACK);       // degree sign
    plist(1, 23, ang < 35 ? PSTR("+KEEPS SPEED\0-LESS AIR\0") : ang > 45 ? PSTR("+BIG AIR\0-SLOWER\0") : PSTR("BALANCED\0"), 0);
    int16_t w = (int16_t)rampW(), x0 = 100 - w, gy = 57;
    drawRamp(x0, gy);
    int16_t lt = gy - rprof[w];
    float c = cos(ang * PI / 180), sn = sin(ang * PI / 180);   // dotted launch direction off the lip
    for (uint8_t k = 2; k < 14; k++) ab.drawPixel(x0 + w + 1 + (int16_t)(k * 3 * c), lt - 2 - (int16_t)(k * 3 * sn), BLACK);
    int8_t bob = (ab.frameCount >> 4) & 1;                    // arrows: the ramp tips up and down
    bool steep = lt < 28;                                    // arrows just behind the lip, clear of the flag and the launch line;
    int16_t ay = steep ? 29 : lt, axx = x0 + w + (steep ? 4 : -10);   // steep: no flag, so they sit right of the post
    at(axx, ay - 16 - bob); pc('\x18'); at(axx, ay - 7 + bob); pc('\x19');
    frameUp(PSTR("RAMP ANGLE"));
  }
}

void loop() {
  if (!ab.nextFrame()) return;
  ab.pollButtons();
  update();
  ab.fillScreen(WHITE); tw_ = false;
  if (st < S_SET) { drawMenus(); ab.invert(false); FX::display(); return; }
  int16_t ox = shake ? random(-shake, shake + 1) : 0, oy = shake ? random(-shake, shake + 1) : 0;
  float cx0 = camx, cy0 = camy; camx -= ox; camy += oy;
  drawWorld(); drawSmoke();
  float sp = st == S_RUN ? vx : spd;
  int16_t tx = SX(x), ty = SY(y) - R - (feet ? 3 : 0);
  // shape: soft tires sag a little on the ground, every landing squashes him, fast climbs stretch him
  float rx = R, ry = R;
  bool onGround = st != S_RUN || y < 1;
  if (onGround && psi < 30) { rx += (30 - psi) / 10.0f; ry -= (30 - psi) / 10.0f; }
  if (landT) { float q = landAmt * landT / 8; rx += q * 0.6f; ry -= q; }
  else if (st == S_RUN && fabs(vy) > 5) { rx -= 1; ry += 1; }
  int16_t tby = SY(y) - (feet ? 3 : 0);
  if (st == S_RUN && y < 1 && psi > 35 && vx > 1) tby += (ab.frameCount >> 1) & 1;   // high PSI wobble
  if ((st >= S_ROLL && st <= S_RUN) && sp > 4.0f) {
    for (uint8_t k = 0; k < 4; k++) {
      int16_t ly = ty - 9 + k * 6 + ((ab.frameCount >> 1) & 1);
      int16_t len = (int16_t)(sp * 3) + random(5);
      ab.drawFastHLine(tx - R - 3 - len, ly, len, BLACK);
    }
  }
  if (ty < -R) {                                    // above the screen: arrow + height
    int16_t ax = tx; if (ax < 3) ax = 3; if (ax > 124) ax = 124;
    ab.drawLine(ax, 0, ax - 3, 3, BLACK); ab.drawLine(ax, 0, ax + 3, 3, BLACK); ab.drawFastVLine(ax, 0, 6, BLACK);
    tinyNum(ax > 108 ? ax - 20 : ax + 5, 1, (uint16_t)y);   // keep the height on screen
  }
  if (st == S_RUN && y > 4) {                          // shadow shows where he'll come down
    int16_t gy = SY(0) - 1, w = max(3, 12 - (int16_t)(y / 12));
    for (int16_t i = -w; i <= w; i += 2) ab.drawPixel(tx + i, gy, BLACK);
  }
  drawTire(tx, tby, (int16_t)(rx + 0.5f), (int16_t)(ry + 0.5f));
  camx = cx0; camy = cy0;
  if (st <= S_ROLL) drawTach();
  if (st == S_SET && ((ab.frameCount >> 5) & 1)) boldC(32, 12, PSTR("HOLD"));
  if (st >= S_ROLL && st <= S_RUN) {
    float tot = st == S_RUN ? sqrt(vx * vx + vy * vy) : sp;
    frect(0, 0, 30, 7, WHITE);
    int16_t ex = tinyNum(1, 1, (uint16_t)(tot * MPH)) + 1;
    tinyStr(ex, 1, PSTR("MPH"));
  }
  int16_t bx = tx + R + 6, byy = tby - 2 * R - 2; if (byy < 8) byy = 8; if (byy > 40) byy = 40;
  if (st <= S_ROLL) { bx = 1; byy = 9; }                         // the rev counter fills the right side: call-outs go top left
  if (gradeT && grade) banner(nth(GN, grade), bx, byy);
  else if (msgT) banner(msg, bx, byy);
  ab.invert(hitstop > 11);
  if (st == S_DONE) drawStats();
  FX::display();
}
