#!/bin/bash
# Runs inside a container for tools/scenarios/run.py. Builds the game and the scenario driver from the tree at
# /in, then plays each scenario named in $SCENARIOS (tests/scenarios/NAME.scn) from a fresh copy of the area
# directory, with the fixture players (tests/player) and empty gods and log directories, writing /out/NAME.out
# (the driver's output), NAME.err and NAME.exit.
set -u
CF="-Wall -std=gnu89 -O -g"
mkdir -p /w && cp -r /in/src /in/area /in/tests /w/ && mkdir -p /w/player /w/gods /w/log
cd /w/src
rm -f ./*.o rom
{
  make rom && gcc -c $CF -Dmain=game_main -o comm_driver.o comm.c \
    && gcc -c $CF -I. -o scenario.o /in/tools/scenarios/scenario.c \
    && gcc -o scenario scenario.o comm_driver.o \
         $(ls ./*.o | grep -v -E '/(comm|comm_driver|scenario)\.o$') \
         -Wl,--wrap=random,--wrap=srandom -lcrypt
} > /out/build.log 2>&1
echo "build exit $?" >> /out/build.log
[ -x scenario ] || exit 1

for name in $SCENARIOS; do
  rm -rf /r && mkdir -p /r && cp -r /w/area /w/tests /r/ && mkdir -p /r/player /r/gods /r/log
  cp /w/tests/player/* /r/player/
  cd /r/area
  timeout 120 /w/src/scenario < "/r/tests/scenarios/$name.scn" > "/out/$name.out" 2> "/out/$name.err"
  echo $? > "/out/$name.exit"
done
