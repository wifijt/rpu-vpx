#!/bin/bash
# rpu-vpx: build rpu_avr (the .hex plugin) with simavr. Mac: rpu_avr.dylib; Windows (MinGW): rpu_avr.dll.
#   avr/build.sh [path/to/simavr]   (default: a clone beside rpu-vpx; avr/SIMAVR_BASE is the tested commit)
set -e
[ -n "$1" ] && S=$(cd "$1" && pwd)/simavr                   # the caller's path, before the cd below
cd "$(dirname "$0")"
S=${S:-$(pwd)/../../simavr/simavr}
[ -d "$S/sim" ] || { echo "simavr not found at $S (git clone https://github.com/buserror/simavr)"; exit 1; }
SRCS=$(ls "$S"/sim/*.c | grep -v -e sim_elf.c -e sim_dwarf.c -e sim_gdb.c -e run_avr.c)
case "$(uname -s)" in
  Darwin) OUT=rpu_avr.dylib; FLAGS="-dynamiclib" ;;
  MINGW*|MSYS*|CYGWIN*) OUT=rpu_avr.dll; FLAGS="-shared -static" ;;
  *) OUT=rpu_avr.so; FLAGS="-shared -fPIC" ;;
esac
${CC:-cc} -O2 -w -fvisibility=hidden -include unistd.h -I. -I"$S/sim" -I"$S/cores" $FLAGS -o $OUT \
  rpu_avr.c wavtrigger.c stubs.c $SRCS "$S/cores/sim_mega2560.c"
echo "built avr/$OUT"
