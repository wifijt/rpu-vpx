// rpu-pinmame: run an RPU game in libpinmame with no table: boot, balls in the trough, a coin, Start.
// Prints the displays, the sound board's commands and the coils. Usage: pmtest [game] (rpu_scrpn).
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include "libpinmame.h"

static int sounds = 0;

static void PINMAMECALLBACK OnState(int state, void *) { printf("state %d\n", state); }
static void PINMAMECALLBACK OnDisplayAvailable(int, int, PinmameDisplayLayout *, void *) {}
// the System 6 layout's displays, in order: P1, P2, P3, P4 (6 digits), then the two 2-digit ones
static uint16_t disp[6][8];
static void PINMAMECALLBACK OnDisplayUpdated(int index, void *data, PinmameDisplayLayout *l, void *) {
  if (!data || index < 0 || index >= 6) return;
  memcpy(disp[index], data, sizeof(uint16_t) * (l->length < 8 ? l->length : 8));
}
// the mixed output (the sound board and the WAV channel): the loudest sample since the last show()
static int peak = 0, channels = 1;
static int PINMAMECALLBACK OnAudioAvailable(PinmameAudioInfo *a, void *) { channels = a->channels; return a->samplesPerFrame; }
static int PINMAMECALLBACK OnAudioUpdated(void *buf, int samples, void *) {
  const int16_t *b = (const int16_t *)buf;
  for (int i = 0; i < samples * channels; i++) { int v = b[i] < 0 ? -b[i] : b[i]; if (v > peak) peak = v; }
  return samples;
}
static void PINMAMECALLBACK OnMechAvailable(int, PinmameMechInfo *, void *) {}
static void PINMAMECALLBACK OnMechUpdated(int, PinmameMechInfo *, void *) {}
static void PINMAMECALLBACK OnSolenoid(PinmameSolenoidState *s, void *) {
  if (s->state) printf("  coil %d on\n", s->solNo);
}
static void PINMAMECALLBACK OnConsole(void *, int, void *) {}
static int PINMAMECALLBACK IsKey(PINMAME_KEYCODE, void *) { return 0; }
static void PINMAMECALLBACK OnLog(PINMAME_LOG_LEVEL lvl, const char *fmt, va_list ap, void *) {
  if (lvl >= PINMAME_LOG_LEVEL_INFO) { vprintf(fmt, ap); }
}
static void PINMAMECALLBACK OnSound(int board, int cmd, void *) { if (sounds++ < 20) printf("  sound board %d: command 0x%02x\n", board, cmd); }

// a 7-segment pattern back to its digit (blank: ' ')
static char digit(uint16_t s) {
  static const uint8_t seg[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
  for (int d = 0; d < 10; d++) if ((s & 0x7f) == seg[d]) return '0' + d;
  return s ? '?' : ' ';
}
static void show(const char *what) {
  char p1[7], p2[7], cr[3], bl[3];
  for (int i = 0; i < 6; i++) { p1[i] = digit(disp[0][i]); p2[i] = digit(disp[1][i]); }
  p1[6] = p2[6] = 0;
  cr[0] = digit(disp[4][0]); cr[1] = digit(disp[4][1]); cr[2] = 0;     // layout entry 4: segments 14-15
  bl[0] = digit(disp[5][0]); bl[1] = digit(disp[5][1]); bl[2] = 0;     // entry 5: segments 6-7
  printf("%-8s P1 [%s] P2 [%s]  credits [%s] ball [%s]  audio peak %d\n", what, p1, p2, cr, bl, peak);
  peak = 0;
}
static void wait(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
static void press(int sw) { PinmameSetSwitch(sw, 1); wait(150); PinmameSetSwitch(sw, 0); }

int main(int argc, char **argv) {
  const char *game = argc > 1 ? argv[1] : "rpu_scorpion2022";
  PinmameConfig config = {PINMAME_AUDIO_FORMAT_INT16, 44100, "", &OnState, &OnDisplayAvailable, &OnDisplayUpdated,
                          &OnAudioAvailable, &OnAudioUpdated, &OnMechAvailable, &OnMechUpdated, &OnSolenoid, &OnConsole,
                          &IsKey, &OnLog, &OnSound};
  snprintf((char *)config.vpmPath, PINMAME_MAX_PATH, "%s/.pinmame/", getenv("HOME"));
  PinmameSetConfig(&config);
  PinmameSetHandleKeyboard(0);
  PinmameSetHandleMechanics(0);
  if (PinmameRun(game) != PINMAME_STATUS_OK) { printf("%s didn't start\n", game); return 1; }
  wait(4000);
  show("attract");
  if (!strcmp(game, "rpu_startrek2022")) {                     // Bally Star Trek: one ball, in the outhole
    PinmameSetSwitch(8, 1); wait(500);                      // the outhole (RPU SW_OUTHOLE + 1)
    press(10); wait(1500); show("coin");                    // coin chute 1 (RPU SW_COIN_1 + 1)
    press(6); wait(2500);                                   // credit / Start (RPU SW_CREDIT_RESET + 1)
    PinmameSetSwitch(8, 0);                                 // kicked to the shooter lane
    wait(1500); show("start");
    press(5); wait(500);                                    // the shooter lane (RPU 4)
    for (int sw = 38; sw <= 40; sw++) { press(sw); wait(300); }   // the bumpers (RPU 37-39)
    press(36); wait(300); press(37); wait(300);             // the slings
    for (int sw = 1; sw <= 4; sw++) { PinmameSetSwitch(sw, 1); wait(400); }   // the drop targets down
    wait(2000); show("playing");
    PinmameStop(); wait(500); return 0;
  }
  PinmameSetSwitch(9, 1); PinmameSetSwitch(10, 1);       // two balls in the trough (RPU's SW_BALL_1/2 + 1)
  wait(500);
  press(4); wait(1500); show("coin");                       // coin chute 1 (RPU SW_COIN_1 + 1)
  press(3); wait(2500);                                     // credit / Start (RPU SW_CREDIT_RESET + 1)
  PinmameSetSwitch(10, 0);                                  // the ball served: its trough switch opens, as the table's would
  wait(1500); show("start");
  for (int i = 0; i < 6; i++) { press(32); wait(200); }     // the spinner (RPU 31)
  press(23); wait(300); press(24); wait(300); press(22); wait(300);   // the pops (RPU 22, 23, 21)
  for (int sw = 25; sw <= 27; sw++) { PinmameSetSwitch(sw, 1); wait(400); }   // the left 3-bank down
  wait(2000); show("playing");
  printf("sound board commands: %d\n", sounds);
  PinmameStop();
  wait(500);
  return 0;
}
