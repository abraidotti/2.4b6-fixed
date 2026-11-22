#!/bin/bash
# Runs inside a container for tools/buildcheck/run.py. Copies the tree mounted at /in, builds it with
# `make -f $MAKEFILE CC=$CC`, boots it on port 4000, plays a scripted session as a new character, and writes the
# results to /out/$TAG: compiler.txt, build.log, boot.log, session.txt, player.txt, result.txt.
set -u
OUT=/out/$TAG
mkdir -p "$OUT" /w
cp -r /in/src /in/area /w/ && mkdir -p /w/player /w/gods /w/log
cd /w/src
rm -f ./*.o rom
"$CC" --version 2>&1 | head -1 > "$OUT/compiler.txt"
make -f "$MAKEFILE" CC="$CC" > "$OUT/build.log" 2>&1
echo "make exit $?" >> "$OUT/build.log"
if [ ! -x rom ]; then echo "no rom binary" > "$OUT/result.txt"; exit 1; fi

cd /w/area
(timeout 60 ../src/rom 4000 > "$OUT/boot.log" 2>&1 &)
for _ in $(seq 1 80); do grep -q "ready to rock" "$OUT/boot.log" 2>/dev/null && break; sleep 0.25; done
if ! grep -q "ready to rock" "$OUT/boot.log"; then echo "never ready" > "$OUT/result.txt"; exit 1; fi

# The session: one line every 0.7 seconds, and everything the game sends is kept.
{
  exec 3<>/dev/tcp/127.0.0.1/4000
  (timeout 40 cat <&3 > "$OUT/session.txt") &
  for line in Checker y secret12 secret12 human m warrior n n sword "" look score "help rom" who inventory \
              equipment quit; do
    sleep 0.7
    printf '%s\r\n' "$line" >&3
  done
  sleep 2
  exec 3>&-
} 2> "$OUT/session.err"
sleep 1
pkill -f "src/rom 4000"
sleep 1
cp /w/player/Checker "$OUT/player.txt" 2>/dev/null
echo "ok" > "$OUT/result.txt"
