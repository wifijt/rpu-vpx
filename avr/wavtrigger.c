/* rpu-vpx: a WAV Trigger on the emulated Mega's Serial1 (see wavtrigger.h).
 *
 * The serial protocol is the WAV Trigger's (Robertsonics), as measured on a real one (firmware v1.34): packets
 * F0 AA len cmd data... 55, little-endian, gains in signed dB (-70 to +10). Its quirks are kept: the track
 * report (0x84) counts tracks from 0, the status list (0x83) from 1.
 *
 * Copyright (C) 2026 John Turner (wifijt). GPL-3.0.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wavtrigger.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#define VOICES     14
#define MAX_TRACK  4096

typedef struct {                /* a track's samples, loaded when first played: stereo int16 */
  int16_t *pcm;
  uint32_t frames, rate;
  int tried;
} wt_track;

typedef struct {
  int track;                    /* 0: free */
  double pos, step;             /* frames into the track; frames per output frame */
  float gain, target, delta;    /* linear; the fade moves gain by delta per output frame */
  uint32_t fadeLeft;            /* output frames left in the fade */
  int stopAtEnd, loop, paused, locked;
  unsigned long age;
} wt_voice;

static char dir[1024];
static char *paths[MAX_TRACK + 1];
static wt_track tracks[MAX_TRACK + 1];
static wt_voice voice[VOICES];
static unsigned long ages;
static float master = 1.f;
static int reporting, outRate = 44100;
static void (*sendByte)(uint8_t);
static uint8_t rx[64];
static int rxLen;

static float db(int g) {
  if (g < -70) g = -70;
  if (g > 10) g = 10;
  return powf(10.f, g / 20.f);
}

/*-- the folder: NNN_anything.wav is track NNN --*/
static void wt_add(const char *name) {
  int n = atoi(name);
  size_t l = strlen(name);
  if (n < 1 || n > MAX_TRACK || l < 5 || (strcmp(name + l - 4, ".wav") && strcmp(name + l - 4, ".WAV"))) return;
  if (name[0] < '0' || name[0] > '9' || paths[n]) return;
  paths[n] = (char *)malloc(strlen(dir) + l + 2);
  if (paths[n]) sprintf(paths[n], "%s/%s", dir, name);
}
static void wt_scan(void) {
#ifdef _WIN32
  char pattern[1100];
  WIN32_FIND_DATAA fd;
  HANDLE h;
  snprintf(pattern, sizeof pattern, "%s\\*.wav", dir);
  if ((h = FindFirstFileA(pattern, &fd)) == INVALID_HANDLE_VALUE) return;
  do wt_add(fd.cFileName); while (FindNextFileA(h, &fd));
  FindClose(h);
#else
  DIR *d = opendir(dir);
  struct dirent *e;
  if (!d) return;
  while ((e = readdir(d)) != NULL) wt_add(e->d_name);
  closedir(d);
#endif
}
static int wt_count(void) {
  int n, c = 0;
  for (n = 1; n <= MAX_TRACK; n++) c += paths[n] != NULL;
  return c;
}

/*-- a RIFF WAV, 8 or 16 bit PCM, mono or stereo, any rate: to stereo int16 --*/
static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static int wt_load(int n) {
  wt_track *t = &tracks[n];
  FILE *f;
  uint8_t h[12], c[8], fmt[16];
  int ch = 0, bits = 0, haveFmt = 0;
  if (t->pcm) return 1;
  if (t->tried || !paths[n]) return 0;
  t->tried = 1;
  if (!(f = fopen(paths[n], "rb"))) return 0;
  if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { fclose(f); return 0; }
  while (fread(c, 1, 8, f) == 8) {
    uint32_t len = le32(c + 4);
    if (!memcmp(c, "fmt ", 4) && len >= 16) {
      if (fread(fmt, 1, 16, f) != 16) break;
      if (le16(fmt) != 1) break;                                   /* PCM only */
      ch = le16(fmt + 2); t->rate = le32(fmt + 4); bits = le16(fmt + 14);
      haveFmt = (ch == 1 || ch == 2) && (bits == 8 || bits == 16) && t->rate;
      fseek(f, (long)(len - 16 + (len & 1)), SEEK_CUR);
    } else if (!memcmp(c, "data", 4) && haveFmt) {
      uint32_t bytesPer = (uint32_t)(ch * bits / 8), i;
      uint8_t *raw = (uint8_t *)malloc(len ? len : 1);
      if (!raw) break;
      len = (uint32_t)fread(raw, 1, len, f);
      t->frames = len / bytesPer;
      t->pcm = (int16_t *)malloc((size_t)t->frames * 2 * sizeof(int16_t) + 4);
      if (t->pcm)
        for (i = 0; i < t->frames; i++) {
          const uint8_t *s = raw + i * bytesPer;
          int l = bits == 16 ? (int16_t)le16(s) : (s[0] - 128) << 8;
          int r = ch == 1 ? l : bits == 16 ? (int16_t)le16(s + 2) : (s[1] - 128) << 8;
          t->pcm[2 * i] = (int16_t)l; t->pcm[2 * i + 1] = (int16_t)r;
        }
      free(raw);
      break;
    } else fseek(f, (long)(len + (len & 1)), SEEK_CUR);
  }
  fclose(f);
  return t->pcm != NULL;
}

/*-- replies --*/
static void send(const uint8_t *m, int n) {        /* m: cmd + data */
  int i;
  if (!sendByte) return;
  sendByte(0xF0); sendByte(0xAA); sendByte((uint8_t)(n + 4));
  for (i = 0; i < n; i++) sendByte(m[i]);
  sendByte(0x55);
}
static void report(int v, int playing) {
  uint8_t m[5];
  int t = voice[v].track - 1;                    /* the real WAV Trigger reports tracks from 0 */
  if (!reporting) return;
  m[0] = 0x84; m[1] = (uint8_t)t; m[2] = (uint8_t)(t >> 8); m[3] = (uint8_t)v; m[4] = (uint8_t)playing;
  send(m, 5);
}

/*-- voices --*/
static void stop_voice(int v) {
  if (!voice[v].track) return;
  report(v, 0);
  voice[v].track = 0;
}
static int new_voice(void) {                        /* a free voice, else the oldest unlocked */
  int v, best = -1;
  for (v = 0; v < VOICES; v++) if (!voice[v].track) return v;
  for (v = 0; v < VOICES; v++)
    if (!voice[v].locked && (best < 0 || voice[v].age < voice[best].age)) best = v;
  if (best >= 0) stop_voice(best);
  return best;
}
static void play(int track, int solo, int paused, int lock) {
  int v;
  if (track < 1 || track > MAX_TRACK || !wt_load(track)) return;
  if (solo) for (v = 0; v < VOICES; v++) stop_voice(v);
  if ((v = new_voice()) < 0) return;
  memset(&voice[v], 0, sizeof voice[v]);
  voice[v].track = track;
  voice[v].step = (double)tracks[track].rate / outRate;
  voice[v].gain = voice[v].target = 1.f;
  voice[v].paused = paused;
  voice[v].locked = lock;
  voice[v].age = ++ages;
  report(v, 1);
}
static void each_voice_of(int track, void (*f)(int v, int arg), int arg) {
  int v;
  for (v = 0; v < VOICES; v++) if (voice[v].track == track) f(v, arg);
}
static void v_stop(int v, int a) { (void)a; stop_voice(v); }
static void v_pause(int v, int a) { voice[v].paused = a; }
static void v_loop(int v, int a) { voice[v].loop = a; }
static void v_gain(int v, int g) { voice[v].gain = voice[v].target = db(g); voice[v].fadeLeft = 0; }
static int fadeGain, fadeMs, fadeStop;
static void v_fade(int v, int a) {
  (void)a;
  voice[v].target = db(fadeGain);
  voice[v].fadeLeft = (uint32_t)((long)fadeMs * outRate / 1000);
  if (!voice[v].fadeLeft) voice[v].fadeLeft = 1;
  voice[v].delta = (voice[v].target - voice[v].gain) / voice[v].fadeLeft;
  voice[v].stopAtEnd = fadeStop;
}

static void track_control(int code, int track, int lock) {
  switch (code) {
    case 0: play(track, 1, 0, lock); break;                         /* play solo */
    case 1: play(track, 0, 0, lock); break;                         /* play poly */
    case 2: each_voice_of(track, v_pause, 1); break;
    case 3: each_voice_of(track, v_pause, 0); break;
    case 4: each_voice_of(track, v_stop, 0); break;
    case 5: each_voice_of(track, v_loop, 1); break;
    case 6: each_voice_of(track, v_loop, 0); break;
    case 7: play(track, 0, 1, lock); break;                         /* load: paused at the start */
  }
}

/*-- a whole packet: m = cmd + data, n bytes --*/
static void command(const uint8_t *m, int n) {
  int v;
  switch (m[0]) {
    case 0x01: {                                                    /* get version */
      uint8_t r[21] = {0x81};
      memcpy(r + 1, "WAV Trigger v1.34   ", 20);
      send(r, 21);
      break;
    }
    case 0x02: {                                                    /* get system info */
      int c = wt_count();
      uint8_t r[4] = {0x82, VOICES, (uint8_t)c, (uint8_t)(c >> 8)};
      send(r, 4);
      break;
    }
    case 0x03: if (n >= 4) track_control(m[1], le16(m + 2), 0); break;
    case 0x0D: if (n >= 5) track_control(m[1], le16(m + 2), m[4]); break;   /* track control ex: + lock */
    case 0x04: for (v = 0; v < VOICES; v++) stop_voice(v); break;
    case 0x05: if (n >= 3) master = db((int16_t)le16(m + 1)); break;
    case 0x07: {                                                    /* status: tracks playing, from 1 */
      uint8_t r[1 + 2 * VOICES];
      int k = 1;
      r[0] = 0x83;
      for (v = 0; v < VOICES; v++)
        if (voice[v].track && !voice[v].paused) { r[k++] = (uint8_t)voice[v].track; r[k++] = (uint8_t)(voice[v].track >> 8); }
      send(r, k);
      break;
    }
    case 0x08: if (n >= 5) each_voice_of(le16(m + 1), v_gain, (int16_t)le16(m + 3)); break;
    case 0x0A:                                                      /* fade: track, gain, ms, stop */
      if (n >= 8) {
        fadeGain = (int16_t)le16(m + 3); fadeMs = le16(m + 5); fadeStop = m[7];
        each_voice_of(le16(m + 1), v_fade, 0);
      }
      break;
    case 0x0B: for (v = 0; v < VOICES; v++) voice[v].paused = 0; break;   /* resume all in sync */
    case 0x0E: if (n >= 2) reporting = m[1]; break;
    /* 0x09 amp power, 0x0C sample-rate offset, 0x0F trigger bank: nothing to do here */
  }
}

void wt_init(const char *sounds, void (*reply)(uint8_t b)) {
  snprintf(dir, sizeof dir, "%s", sounds);
  sendByte = reply;
  wt_scan();
  if (getenv("RPU_PLUGIN_LOG")) fprintf(stderr, "RPU: WAV Trigger: %d tracks in %s\n", wt_count(), dir);
}

/* framing: F0 AA len cmd data... 55, resyncing a byte at a time */
void wt_rx(uint8_t b) {
  if (rxLen == 0 && b != 0xF0) return;
  if (rxLen == 1 && b != 0xAA) { rxLen = b == 0xF0; rx[0] = 0xF0; return; }
  rx[rxLen++] = b;
  if (rxLen >= 3) {
    int len = rx[2];
    if (len < 5 || len > (int)sizeof rx) { rxLen = 0; return; }
    if (rxLen == len) {
      if (rx[len - 1] == 0x55) command(rx + 3, len - 4);
      rxLen = 0;
    }
  }
}

void wt_render(int16_t *out, int frames, int rate) {
  int i, v;
  if (rate > 0 && rate != outRate) {                              /* the host's rate: re-time the voices */
    for (v = 0; v < VOICES; v++) if (voice[v].track) voice[v].step = (double)tracks[voice[v].track].rate / rate;
    outRate = rate;
  }
  for (i = 0; i < frames; i++) {
    float l = 0, r = 0;
    for (v = 0; v < VOICES; v++) {
      wt_voice *p = &voice[v];
      wt_track *t;
      uint32_t f;
      if (!p->track || p->paused) continue;
      t = &tracks[p->track];
      f = (uint32_t)p->pos;
      if (f >= t->frames) {
        if (p->loop && t->frames) { p->pos = 0; f = 0; }
        else { stop_voice(v); continue; }
      }
      l += t->pcm[2 * f] * p->gain;
      r += t->pcm[2 * f + 1] * p->gain;
      p->pos += p->step;
      if (p->fadeLeft) {
        p->gain += p->delta;
        if (--p->fadeLeft == 0) { p->gain = p->target; if (p->stopAtEnd) stop_voice(v); }
      }
    }
    l *= master; r *= master;
    out[2 * i] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
    out[2 * i + 1] = (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
  }
}
