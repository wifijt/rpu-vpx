/**************************************************************************
 * rpu-pinmame: an RPU game as a plugin for PinMAME's RPU driver.
 *
 * Copyright (C) 2026 John Turner (wifijt). GPL-3.0: see LICENSE.
 *
 * rpu-sim's sim_core.cpp stands in for RPU.cpp and the Arduino core (it is #included here, with
 * RPU_SIM_PLUGIN defined, so its state is ours to read). This file adds:
 *   - the plugin entry points (rpu_plugin.h): time, switches in; lamps, coils, displays out,
 *   - the machine's own sound board: the game's RPU sound numbers, queued for the driver,
 *   - the WAV Trigger: the game's WAV files, mixed for PinMAME's sound output.
 * The physics (troughs, drop targets) is the table's: none of rpu-sim's is configured.
 **************************************************************************/
#include "sim_core.cpp"
#include "rpu_plugin.h"

#include <cmath>
#include <dirent.h>

// ------------------------------------------------------------------ coils
static unsigned long PlugSolUntil[32];         // a fired coil counts as on for a pulse's length
static uint32_t PlugContinuous;                // RPU_SetContinuousSolenoid
static const unsigned long PULSE_MS = 30;

static void PluginContinuous(byte solNum, bool on) {
  if (solNum >= 32) return;
  if (on) PlugContinuous |= 1u << solNum; else PlugContinuous &= ~(1u << solNum);
}

// ------------------------------------------------------------------ the machine's sound board
struct PlugSound { unsigned short n; unsigned long when; };
static std::deque<PlugSound> PlugSounds;
static std::mutex PlugSoundMu;

static void PluginSound(unsigned short soundNumber, byte, unsigned long when) {
  std::lock_guard<std::mutex> g(PlugSoundMu);
  if (PlugSounds.size() < 64) PlugSounds.push_back({soundNumber, when});
}

// ------------------------------------------------------------------ the WAV Trigger
// Every file plays at 44.1 kHz whatever its header says, as on the WAV Trigger (games were tuned by ear
// on it). 14 voices, track gain in dB, fades, loops, as the game asks through Serial1.
struct PlugWav { std::vector<int16_t> pcm; int channels = 1; bool loaded = false, bad = false; std::string path; };
struct PlugVoice { int track = 0; size_t pos = 0; uint32_t frac = 0; bool loop = false;
                   float gainDb = 0, fadeTo = 0, fadeStep = 0; int fadeLeft = 0; bool stopAtFadeEnd = false; };
static std::map<int, PlugWav> PlugTracks;
static PlugVoice PlugVoices[14];
static std::mutex PlugAudioMu;

static bool LoadWav(PlugWav &w) {
  if (w.loaded || w.bad) return w.loaded;
  FILE *f = fopen(w.path.c_str(), "rb");
  if (!f) { w.bad = true; return false; }
  std::vector<uint8_t> b;
  uint8_t buf[65536];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) b.insert(b.end(), buf, buf + n);
  fclose(f);
  if (b.size() < 12 || memcmp(&b[0], "RIFF", 4) || memcmp(&b[8], "WAVE", 4)) { w.bad = true; return false; }
  int bits = 0;
  for (size_t p = 12; p + 8 <= b.size();) {
    uint32_t len = b[p + 4] | b[p + 5] << 8 | b[p + 6] << 16 | (uint32_t)b[p + 7] << 24;
    if (!memcmp(&b[p], "fmt ", 4) && p + 24 <= b.size()) {
      w.channels = b[p + 10] | b[p + 11] << 8;
      bits = b[p + 22] | b[p + 23] << 8;
    } else if (!memcmp(&b[p], "data", 4)) {
      if (bits != 16 || w.channels < 1 || w.channels > 2) break;
      size_t end = std::min(b.size(), (size_t)(p + 8 + len));
      w.pcm.resize((end - (p + 8)) / 2);
      memcpy(w.pcm.data(), &b[p + 8], w.pcm.size() * 2);
      w.loaded = true;
      return true;
    }
    p += 8 + len + (len & 1);
  }
  w.bad = true;
  return false;
}

static void ScanSounds(const char *dir) {
  DIR *d = dir ? opendir(dir) : NULL;
  struct dirent *e;
  while (d && (e = readdir(d))) {
    const char *n = e->d_name;
    size_t l = strlen(n);
    if (!isdigit((unsigned char)n[0]) || l < 5 || strcasecmp(n + l - 4, ".wav")) continue;
    int track = atoi(n);
    PlugWav w;
    w.path = std::string(dir) + "/" + n;
    PlugTracks[track] = w;
    // the game's track reports (sim_core) end when the file does
    FILE *f = fopen(w.path.c_str(), "rb");
    if (f) {
      fseek(f, 0, SEEK_END);
      long size = ftell(f);
      fclose(f);
      WavTrackLength[track] = (unsigned long)(size / (44100.0 * 2 * 2) * 1000);   // roughly: stereo 16-bit
    }
  }
  if (d) closedir(d);
}

static void VoiceStopTrack(int track) { for (auto &v : PlugVoices) if (v.track == track) v.track = 0; }

static void VoicePlay(int track, bool solo) {
  if (solo) for (auto &v : PlugVoices) v.track = 0;
  VoiceStopTrack(track);
  auto it = PlugTracks.find(track);
  if (it == PlugTracks.end() || !LoadWav(it->second)) return;
  for (auto &v : PlugVoices) {
    if (v.track) continue;
    v = PlugVoice();
    v.track = track;
    return;
  }
}

// the WAV Trigger commands sim_core decoded (live mode), as small JSON arrays
static void AudioEvent(const std::string &e) {
  char op[16] = "";
  int a = 0, b = 0, c = 0, d = 0;
  if (sscanf(e.c_str(), "[\"%15[a-z]\",%d,%d,%d,%d]", op, &a, &b, &c, &d) < 1) return;
  std::lock_guard<std::mutex> g(PlugAudioMu);
  std::string o = op;
  if (o == "solo") VoicePlay(a, true);
  else if (o == "play") VoicePlay(a, false);
  else if (o == "stop") VoiceStopTrack(a);
  else if (o == "stopall") for (auto &v : PlugVoices) v.track = 0;
  else if (o == "loop") { for (auto &v : PlugVoices) if (v.track == a) v.loop = b != 0; }
  else if (o == "gain") { for (auto &v : PlugVoices) if (v.track == a) { v.gainDb = (float)b; v.fadeLeft = 0; } }
  else if (o == "fade") {
    for (auto &v : PlugVoices) if (v.track == a) {
      int frames = std::max(1, c * 441 / 10);            // ms at 44.1 kHz
      v.fadeTo = (float)b; v.fadeStep = (v.fadeTo - v.gainDb) / frames; v.fadeLeft = frames; v.stopAtFadeEnd = d != 0;
    }
  }
}

static void RenderAudio(int16_t *out, int frames, int rate) {
  std::vector<int32_t> acc(frames * 2, 0);
  {
    std::lock_guard<std::mutex> g(PlugAudioMu);
    uint32_t step = (uint32_t)(((uint64_t)44100 << 16) / (rate > 0 ? rate : 44100));
    for (auto &v : PlugVoices) {
      if (!v.track) continue;
      PlugWav &w = PlugTracks[v.track];
      size_t len = w.pcm.size() / w.channels;
      for (int i = 0; i < frames && v.track; i++) {
        if (v.pos >= len) {
          if (v.loop && len) v.pos = 0; else { v.track = 0; break; }
        }
        float gain = powf(10.0f, v.gainDb / 20.0f);
        int l = w.pcm[v.pos * w.channels], r = w.channels == 2 ? w.pcm[v.pos * 2 + 1] : l;
        acc[i * 2] += (int32_t)(l * gain);
        acc[i * 2 + 1] += (int32_t)(r * gain);
        v.frac += step;
        v.pos += v.frac >> 16;
        v.frac &= 0xFFFF;
        if (v.fadeLeft > 0) {
          v.gainDb += v.fadeStep;
          if (--v.fadeLeft == 0) { v.gainDb = v.fadeTo; if (v.stopAtFadeEnd) v.track = 0; }
        }
      }
    }
  }
  for (int i = 0; i < frames * 2; i++) out[i] = (int16_t)std::max(-32768, std::min(32767, acc[i]));
}

// ------------------------------------------------------------------ the entry points
static bool PlugEepromDirty;
static unsigned long PlugEepromWrites;

static void PlugStart(const char *sound_dir, const uint8_t *eeprom, int size) {
  memset(SimEEPROMData, 0xFF, sizeof(SimEEPROMData));
  if (eeprom && size > 0) memcpy(SimEEPROMData, eeprom, std::min(size, (int)sizeof(SimEEPROMData)));
  LiveMode = true;                          // sim_core hands us each coil fired and each WAV command
  SimEcho = getenv("RPU_PLUGIN_LOG") != NULL;
  ScanSounds(sound_dir);
  Boot();
  PlugEepromWrites = SimEEPROMWrites;
}

static void PlugRunMs(unsigned ms) {
  for (unsigned i = 0; i < ms; i++) {
    SimNow += 1;
    LoopPass += 1;
    WavTick();
    loop();
    for (int sol : LiveFired) if (sol >= 0 && sol < 32) PlugSolUntil[sol] = SimNow + PULSE_MS;
    LiveFired.clear();
    for (const std::string &e : LiveAudio) AudioEvent(e);
    LiveAudio.clear();
  }
  if (SimLog.size() > 2000) SimLog.erase(SimLog.begin(), SimLog.end() - 200);
  if (SimEEPROMWrites != PlugEepromWrites) { PlugEepromWrites = SimEEPROMWrites; PlugEepromDirty = true; }
}

static void PlugSetSwitch(int sw, int closed) {
  if (closed) { if (!SwitchState[sw]) SimCloseSwitch(sw); }
  else if (SwitchState[sw]) SimOpenSwitch(sw, true);
}

static int PlugLamp(int n) {
  if (n < 0 || n >= RPU_MAX_LAMPS) return 0;
  if (UseOverlayOn && OverlayOn[n]) return 1;
  if (UseOverlayOff && OverlayOff[n]) return 0;
  if (!LampOn[n]) return 0;
  return LampFlash[n] > 0 ? (int)((SimNow / LampFlash[n]) % 2 == 0) : 1;
}

static uint32_t PlugSolenoids() {
  uint32_t m = PlugContinuous;
  for (int s = 0; s < 32; s++) if (PlugSolUntil[s] > SimNow) m |= 1u << s;
  return m;
}

static int PlugFlippers() { return !FlippersDisabled; }

static int PlugDigit(int display, int pos) {
  if (display < 0 || display > 3 || pos < 0 || pos >= RPU_OS_NUM_DIGITS) return -1;
  if (!(DisplayEnable[display] & (1 << pos))) return -1;
  return DisplayDigits[display][pos];
}

static int PlugDigits() { return RPU_OS_NUM_DIGITS; }
static int PlugCredits() { return CreditsShown; }
static int PlugBall() { return BallInPlayShown; }

static int PlugSoundCommand() {
  std::lock_guard<std::mutex> g(PlugSoundMu);
  for (auto it = PlugSounds.begin(); it != PlugSounds.end(); ++it) {
    if (it->when && it->when > SimNow) continue;
    int n = it->n;
    PlugSounds.erase(it);
    return n;
  }
  return -1;
}

static const uint8_t *PlugEeprom(int *size, int *dirty) {
  if (size) *size = SIM_EEPROM_SIZE;
  if (dirty) { *dirty = PlugEepromDirty; PlugEepromDirty = false; }
  return SimEEPROMData;
}

#ifndef RPU_PLUGIN_GAME
#define RPU_PLUGIN_GAME "rpu"
#endif

static const rpu_plugin_api PlugApi = {
  RPU_PLUGIN_VERSION, RPU_PLUGIN_GAME,
  PlugStart, PlugRunMs, PlugSetSwitch, PlugLamp, PlugSolenoids, PlugFlippers,
  PlugDigit, PlugDigits, PlugCredits, PlugBall, PlugSoundCommand, RenderAudio, PlugEeprom,
};

#ifdef _WIN32
#define RPU_PLUGIN_EXPORT __declspec(dllexport)
#else
#define RPU_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif
extern "C" RPU_PLUGIN_EXPORT const rpu_plugin_api *rpu_plugin_get(void) { return &PlugApi; }
