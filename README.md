# 2.4b6-fixed

**A derivative of ROM 2.4b6 with its bugs fixed.** ROM 2.4b6 is Russ Taylor's MUD codebase (a text-based
multiplayer game), released on May 29, 1998. This repository starts from that release, unmodified, and adds fixes on
top of it, one commit per bug, so anyone starting a ROM-based MUD can see each change and decide whether to use it.

This isn't ROM, and it isn't released by ROM's authors. ROM's licence lets anyone release derivatives of ROM 2.4,
labelled as such, with all of its credits; only Russ Taylor releases ROM itself (see [Licence](#licence)).

**Status:** the stock release is at the `stock-2.4b6` tag, and [the triage](triage/README.md) lists the bugs to fix.
No fixes yet. The code doesn't build on a current compiler yet: that comes first.

## What's here

- **The stock release:** the first commit, tagged [`stock-2.4b6`](https://github.com/abraidotti/2.4b6-fixed/tree/stock-2.4b6),
  is `Rom24b6.tar.gz` byte for byte. Its message records where the release came from and how it was checked.
- **Every change since:** `git diff stock-2.4b6 main`. The stock files are never edited outside a fix's commit.
- **The bugs:** [triage/README.md](triage/README.md) takes the bugs found while porting QuickMUD (a ROM derivative)
  to Python, in [abraidotti/QuickMUD-Python](https://github.com/abraidotti/QuickMUD-Python), and checks each one
  against the stock code. The ones in stock are what this repo fixes.
- **Tools:** [tools/](tools/) holds the scripts that keep the triage table in step with its data. They're MIT
  ([tools/LICENSE](tools/LICENSE)), separate from the game's licences.

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

These fixes keep every credit and notice. A fix never changes a licence, a credit or a copyright line.
