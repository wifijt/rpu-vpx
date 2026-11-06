#!/bin/bash
# rpu-vpx: put an RPU game's .hex into VPX (macOS). Copies it into ~/.pinmame/rpu with its settings and WAVs,
# and makes a copy of a table that runs it.
#   tools/add-game.sh <sketch>.ino.hex <name> <machine> [rev] [--sounds <folder>] [--table <table>.vpx]
#   <name>     your game's name in PinMAME: rpu_<name> (letters, digits, _)
#   <machine>  PinMAME's name for the machine: scrpn_l1 (System 6), startrek or startreb (Bally -35), ...
#   [rev]      your build's RPU_OS_HARDWARE_REV; the default is 102 on System 6, 4 on Bally
#   --sounds   your WAV Trigger files (001_name.wav, ...): copied to ~/.pinmame/rpu/<name>/
#   --table    a VPX table for that machine: copied beside it as "<table> (rpu_<name>).vpx", set to run your game
# RPU_PLUGIN_DIR overrides ~/.pinmame/rpu. VPX: VPinballX_BGFX.app in ~/Applications or /Applications,
# or set VPX to its binary.
set -e
usage() { sed -n 2,11p "$0" | sed 's/^# \{0,1\}//'; exit 1; }
[ $# -ge 3 ] || usage
HEX=$1 NAME=$2 MACHINE=$3; shift 3
REV= SOUNDS= TABLE=
while [ $# -gt 0 ]; do
  case $1 in
    --sounds) SOUNDS=${2:?--sounds needs a folder}; shift 2 ;;
    --table) TABLE=${2:?--table needs a .vpx}; shift 2 ;;
    [0-9]*) REV=$1; shift ;;
    *) usage ;;
  esac
done
[ -f "$HEX" ] || { echo "no file $HEX"; exit 1; }
case $HEX in *with_bootloader*) echo "use <sketch>.ino.hex, not the with_bootloader one"; exit 1 ;; esac
[[ $NAME =~ ^[A-Za-z0-9_]+$ ]] || { echo "the name takes letters, digits and _ only"; exit 1; }
DIR=${RPU_PLUGIN_DIR:-$HOME/.pinmame/rpu}
mkdir -p "$DIR"
[ -f "$DIR/rpu_avr.dylib" ] || echo "note: $DIR/rpu_avr.dylib isn't there yet (avr/build.sh builds it)"

cp "$HEX" "$DIR/rpu_$NAME.hex"
{ echo "rom $MACHINE"; [ -n "$REV" ] && echo "rev $REV"; } > "$DIR/rpu_$NAME.txt"
echo "game:   $DIR/rpu_$NAME.hex, rpu_$NAME.txt (rom $MACHINE${REV:+, rev $REV})"
if [ -n "$SOUNDS" ]; then
  [ -d "$SOUNDS" ] || { echo "no folder $SOUNDS"; exit 1; }
  mkdir -p "$DIR/$NAME"
  cp "$SOUNDS"/*.wav "$DIR/$NAME/"
  echo "sounds: $(ls "$DIR/$NAME"/*.wav | wc -l | tr -d ' ') WAVs in $DIR/$NAME"
fi

if [ -n "$TABLE" ]; then
  [ -f "$TABLE" ] || { echo "no table $TABLE"; exit 1; }
  BASE=${TABLE%.vpx}
  NEW="$BASE (rpu_$NAME)"
  cp "$TABLE" "$NEW.vpx"
  # the backglass: the table's own name, else the folder's only one
  shopt -s nullglob
  B2S=("$BASE".[Dd]irect[Bb]2[Ss])
  if [ ${#B2S[@]} -eq 0 ]; then
    B2S=()
    for f in "$(dirname "$TABLE")"/*.[Dd]irect[Bb]2[Ss]; do [[ $f == *"(rpu_"* ]] || B2S+=("$f"); done
  fi
  shopt -u nullglob
  [ ${#B2S[@]} -eq 1 ] && cp "${B2S[0]}" "$NEW.directB2S" && echo "backglass: $NEW.directB2S"
  if [ -f "$BASE.vbs" ]; then cp "$BASE.vbs" "$NEW.vbs"
  else                                                  # the script inside the table: VPX exports it
    VPXBIN=$VPX
    if [ -z "$VPXBIN" ]; then                           # ~/Applications, else /Applications
      for a in "$HOME/Applications" /Applications; do
        [ -x "$a/VPinballX_BGFX.app/Contents/MacOS/VPinballX_BGFX" ] && { VPXBIN=$a/VPinballX_BGFX.app/Contents/MacOS/VPinballX_BGFX; break; }
      done
    fi
    [ -x "$VPXBIN" ] || { echo "VPX not found in ~/Applications or /Applications, to export the table's script: set VPX to its binary"; exit 1; }
    "$VPXBIN" -ExtractVBS "$NEW.vpx" > /dev/null 2>&1 || true
    [ -f "$NEW.vbs" ] || { echo "VPX didn't export the script of $TABLE"; exit 1; }
  fi
  # the first cGameName = "..." is the table's game; line endings kept
  perl -0pi -e 's/(cGameName\s*=\s*")[^"]*"/${1}rpu_'"$NAME"'"/i or die "no cGameName in the script\n"' "$NEW.vbs"
  echo "table:  $NEW.vpx (cGameName rpu_$NAME)"
fi
