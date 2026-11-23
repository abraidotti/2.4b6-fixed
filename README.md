# 2.4b6-fixed

**A derivative of ROM 2.4b6 with its bugs fixed.** ROM 2.4b6 is Russ Taylor's MUD codebase (a text-based
multiplayer game), released on May 29, 1998. This repository starts from that release, unmodified, and adds fixes on
top of it, one commit per bug, so anyone starting a ROM-based MUD can see each change and decide whether to use it.

This isn't ROM, and it isn't released by ROM's authors. ROM's licence lets anyone release derivatives of ROM 2.4,
labelled as such, with all of its credits; only Russ Taylor releases ROM itself (see [Licence](#licence)).

**Status:** [`v1.0`](https://github.com/abraidotti/2.4b6-fixed/releases/tag/v1.0) fixes every stock bug in
[the triage](triage/README.md), and a few more found along the way. It builds on current compilers (see
[Building](#building)), and [FIXES.md](FIXES.md) lists every change made since the stock release, one commit each.

## What's here

- **The stock release:** the first commit, tagged [`stock-2.4b6`](https://github.com/abraidotti/2.4b6-fixed/tree/stock-2.4b6),
  is `Rom24b6.tar.gz` byte for byte. Its message records where the release came from and how it was checked.
- **Every change since:** [FIXES.md](FIXES.md), generated from the commits, and `git diff stock-2.4b6 main`. The
  stock files are never edited outside a fix's commit.
- **The bugs:** [triage/README.md](triage/README.md) takes the bugs found while porting QuickMUD (a ROM derivative)
  to Python, in [abraidotti/QuickMUD-Python](https://github.com/abraidotti/QuickMUD-Python), and checks each one
  against the stock code. The ones in stock are what this repo fixes.
- **Tools:** [tools/](tools/) holds the scripts that write the triage table and FIXES.md, the build check
  ([Building](#building)) and the scenarios ([Testing](#testing)). They're MIT ([tools/LICENSE](tools/LICENSE)),
  separate from the game's licences.

## Building

On Linux, with gcc or clang, `make` and libcrypt (`libcrypt-dev` on Debian and Ubuntu):

```sh
cd src && make            # or: make CC=clang
cd ../area && ../src/rom 4000
```

Then connect to port 4000 (for example `telnet 127.0.0.1 4000`). The game runs from `area/`, and saves players in
`player/`.

Stock ROM 2.4b6 doesn't build on today's compilers. The changes that make it build are the first commits after
`stock-2.4b6`, kept apart from the bug fixes, and none of them changes what the game does
([#2](https://github.com/abraidotti/2.4b6-fixed/issues/2)). `python tools/buildcheck/run.py` checks that with
Docker: it builds the code with gcc 4.9, gcc 15 and clang, plays the same scripted session as a new character on
each, and compares them with stock built the old way. Tested with gcc 4.9.4, gcc 15.3.0 and clang 19.1.7.

## Testing

`python tools/scenarios/run.py` plays scripted sessions through the game (players logging in, talking, fighting,
buying, casting) and compares what the game sends with the saved goldens in [tests/scenarios](tests/scenarios), with
a fixed seed and clock so every run is the same. 96 of them come from QuickMUD-Python. Each bug fix adds one that
shows its bug, and re-saves the goldens it changes, so its diff shows what the game now does differently. See
[tests/README.md](tests/README.md), and [#5](https://github.com/abraidotti/2.4b6-fixed/issues/5) for how it works.

## Lineage

Each generation is a derivative of the one before it and is bound by every earlier licence.

| Years | Codebase | Developers |
|---|---|---|
| 1990–1991 | **DikuMUD** (Gamma 0.0) | Sebastian Hammer (Quinn), Michael Seifert (Papi), Hans Henrik Stærfeldt (God), Tom Madsen (Stormbringer), Katja Nyboe (Superwoman), at DIKU, the Department of Computer Science, University of Copenhagen |
| 1992–1993 | **Merc 2.1** | Furey, Hatchet and Kahn: Michael Chastain, Michael Quan and Mitchell Tse |
| 1993–1998 | **ROM 2.4b6** (Rivers of Mud) | Russ Taylor (Alander), with the ROM consortium: Gabrielle Taylor and Brian Moore (Zump) |
| 2026– | **2.4b6-fixed** | [abraidotti](https://github.com/abraidotti) |

The stock release's own files tell its history: [README](README) (Diku and Merc), [README.rom](README.rom) and
[README.version](README.version), with the credits in [doc/rom.credits](doc/rom.credits).

## Licence

The code is under the licences it came with, unchanged, in [doc/](doc/): DikuMUD's
([doc/license.doc](doc/license.doc)), Merc's ([doc/license.txt](doc/license.txt)) and ROM's
([doc/rom.license](doc/rom.license)). Read them before you use this code. In short, ROM's licence requires that a
MUD running it:

- follows the Diku and Merc licences;
- says on its login screen that it's based on ROM 2.4 beta, copyright 1993-1996 Russ Taylor, and keeps the in-game
  `help rom` entry unaltered and readable by every player;
- isn't called ROM, Rivers of Mud, or a name built on them;
- keeps every README and credits file.

ROM's licence also asks you to email Russ Taylor before opening a ROM-based MUD. The address it gives
(rtaylor@efn.org) no longer works.

**The stock release in this repository.** ROM's licence says only Russ Taylor releases ROM itself. The first
commit is his release, unmodified and labelled as his, so that every fix can be read against it, as other sites
have long hosted it. ftp.game.org, the MUD archive that mirrored the ROM releases, said of them on its ROM page
([archived 16 January 2002](https://web.archive.org/web/20020116144842/http://ftp.game.org/cgi-bin/directory?/pub/mud/diku/merc/rom)):

> FTP.GAME.ORG has permission from Russ Taylor to mirror the ROM distributions, and make them publically
> available from our site.

That permission was ftp.game.org's, not this repository's. It's quoted here for what it shows: that Russ Taylor
let his releases be republished unmodified.

These fixes keep every credit and notice. A fix never changes a licence, a credit or a copyright line.
