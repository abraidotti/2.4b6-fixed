# Scenarios

Each scenario is a script of what players type, and its golden is what the game sent back, step by step. They're
played through the game by `python tools/scenarios/run.py` (Docker), which fails if anything differs from a golden.
The driver and its format are described in [tools/scenarios/scenario.c](../tools/scenarios/scenario.c), and the
choices behind it in [#5](https://github.com/abraidotti/2.4b6-fixed/issues/5).

- `scenarios/NAME.scn` and `NAME.golden`: the 96 scenarios from QuickMUD-Python, and one for each bug fixed
  since (its comment names the issue). A golden's first line is `2.4b6-fixed`; the rest is the
  driver's output.
- `player/`: 35 characters the scenarios log in as (Decimus, Smokegod, Rambler and the rest), copied into a fresh
  `player/` before each scenario.
- `files/`: files a scenario puts in place before the game boots (`@place`): `bans`, a ban list. A scenario
  can also delete a file before boot (`@remove`), such as `music.txt`.

A bug fix re-saves the goldens it changes (`run.py --save --only NAME`), so its PR shows what the game does
differently.

## Where they came from

The scenarios and the 35 characters are QuickMUD-Python's (`game/tests/data/scenarios/` and
`tools/fixtures/player/`, after its rename, #117), where they compared QuickMUD's C with its Python port. Their
comments are QuickMUD-Python's, as written: an issue number in them (`#49`) is one of
[QuickMUD-Python's issues](https://github.com/abraidotti/QuickMUD-Python/issues), and a remark about the Python game
or QuickMUD is about that repo. The characters' player files are in QuickMUD's format; stock reads them, logging
"Fread_char: no match" for the fields it doesn't have (OLC's `Sec`, for one) and skipping them.

The goldens here were saved from this repo's code, built from stock. QuickMUD-Python had 122 scenarios. The 26 that
need something stock hasn't got were left out:

| Left out | Why |
|---|---|
| `aedit`, `medit`, `oedit`, `redit`, `hedit`, `asave-commands`, `asave-fixture` | OLC, QuickMUD's area editor |
| `mobprog-checks`, `mobprog-errors`, `mobprog-fight`, `mobprog-wander`, `mobcmds-acts`, `mobcmds-room` | mob programs |
| `notes-badkeys`, `notes-badtext`, `notes-read`, `notes-write`, `note-long` | QuickMUD's boards (stock has `note.c`) |
| `colour`, `copyover` | colour and copyover |
| `old-format` | QuickMUD's old-format conversions; its fixture's bad `P` reset, which QuickMUD logs, ends stock's boot |
| `fight-commands` | `surrender` |
| `leftover-commands` | `scan`, which stock has the code for but no command |
| `memory-dump` | the OLC editors, for `dump`'s counts |
| `toggles` | `autoall` and `telnetga` |
| `wiz-switch` | `qmconfig` |

The last five are mostly stock commands, and stock-only versions of them could come back later.

Of the 96 kept, every one has the same number of `Huh?` replies, logins and closed connections as its QuickMUD-Python
golden: none leans on a command stock lacks.
