/* rpu-pinmame: load a game plugin on its own and play a little: boot, a coin, Start, a drain. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include "../plugin/rpu_plugin.h"

static void show(const rpu_plugin_api *g, const char *what) {
  printf("%-10s P1 [", what);
  for (int p = 0; p < g->display_digits(); p++) { int d = g->display_digit(0, p); putchar(d < 0 ? ' ' : '0' + d); }
  printf("]  credits %d  ball %d  coils %08x  flippers %d  lamps on:", g->credits(), g->ball_in_play(), g->solenoids(),
         g->flippers_enabled());
  int n = 0;
  for (int l = 0; l < 64; l++) if (g->lamp(l)) { printf(" %d", l); n++; }
  printf(" (%d)\n", n);
}

int main(int argc, char **argv) {
  void *h = dlopen(argc > 1 ? argv[1] : "build/rpu_scorpion2022.dylib", RTLD_NOW);
  if (!h) { printf("dlopen: %s\n", dlerror()); return 1; }
  rpu_plugin_get_fn get = (rpu_plugin_get_fn)dlsym(h, "rpu_plugin_get");
  const rpu_plugin_api *g = get();
  printf("plugin %s v%d\n", g->game, g->version);
  g->start(argc > 2 ? argv[2] : NULL, NULL, 0);
  int sounds = 0;
  #define RUN(ms) for (int i = 0; i < (ms); i++) { g->run_ms(1); int s; while ((s = g->sound_command()) >= 0) { if (sounds++ < 12) printf("  sound command 0x%04x at %d ms\n", s, i); } }
  RUN(3000); show(g, "attract");
  g->set_switch(3, 1); RUN(100); g->set_switch(3, 0); RUN(1000); show(g, "coin");
  g->set_switch(2, 1); RUN(100); g->set_switch(2, 0); RUN(3000); show(g, "start");
  int16_t buf[2 * 1024]; g->render_audio(buf, 1024, 48000);
  printf("sound commands: %d total\n", sounds);
  return 0;
}
