# Mass Prospecting

A Prospecting profession window for WoW 3.3.5a, plus an AzerothCore module that lets it prospect an ore over and over from one click,
the way Blizzard's **Create All** repeats a craft.

This repo has two parts:

- **`ProspectingUI/`**: a client addon that gives Prospecting its own window in the default tradeskill
  style. It has ores grouped by expansion, difficulty colors, a gem grid for each ore, an amount box
  and Create All, and ReagentBankUI integration. See [ProspectingUI/README.md](ProspectingUI/README.md).
- **The server module** (everything else) repeats the Prospecting cast on the server, so Create All
  doesn't need a click per prospect.

The addon works on its own. The module is what makes Create All continuous.

The 3.3.5a client only lets an addon cast a spell from a real click or key press, so an addon
can't chain prospects by itself. Crafts get around this because the server repeats them, and this
module does the same for Prospecting. It's built to be driven by the **ProspectingUI** addon: with this
module installed, ProspectingUI's **Create All** (or **Prospecting** with an amount above 1) prospects the
whole amount from one click.

## How it works

- Each prospect is a normal Prospecting cast started by the server. It shows a cast bar, runs the usual
  skill and stack checks, and moving interrupts it.
- The ore from a prospect is only used up when its loot window closes, so each run waits for the
  loot before starting the next cast. With **Auto Loot** on this is instant. Without it, you have
  to take the loot each time, and a run stops if the loot sits there for
  `MassProspecting.LootTimeoutSec`.
- It uses the smallest stack of 5 or more first, the same as ProspectingUI.
- A run stops when:
  - it reaches the amount
  - you run out of stacks of 5
  - you move or the cast is interrupted
  - you click again (`.massprospect stop`)
  - you log out

## Commands

| Command | What it does |
| --- | --- |
| `.massprospect start <ore entry> <count>` | Prospect that ore `count` times. |
| `.massprospect stop` | Stop the current run. |
| `.massprospect ping` | Replies `MASSPROSPECT:PONG`; ProspectingUI uses it to find the module. |

Replies for the addon are system messages starting with `MASSPROSPECT:`. ProspectingUI hides them from
chat.

## Requirements

- AzerothCore wotlk (master). No other module is needed and no SQL is run.
- WoW 3.3.5a (12340) client. The module works without the addon, but only **ProspectingUI** gives it a
  window and a Create All button.
- Optional: ReagentBankUI, which ProspectingUI integrates with if it is installed.

## Install

```bash
cd /path/to/azerothcore-wotlk/modules
cp -r /path/to/mod-mass-prospecting .
cd /path/to/build
cmake ../ -DCMAKE_INSTALL_PREFIX=/path/to/server
make -j$(nproc) install
```

Copy `conf/mod_mass_prospecting.conf.dist` to `etc/modules/mod_mass_prospecting.conf`. No SQL is needed.

For the addon, copy the `ProspectingUI` folder into `World of Warcraft/Interface/AddOns/`.

## Configuration

| Setting | Default | Meaning |
| --- | --- | --- |
| `MassProspecting.Enable` | `1` | Master switch. |
| `MassProspecting.MaxCount` | `200` | Most prospects one run can ask for. |
| `MassProspecting.DelayMs` | `250` | Pause between taking a prospect's loot and the next cast. |
| `MassProspecting.LootTimeoutSec` | `30` | How long a run waits for loot to be taken before it stops. |

## Troubleshooting

- **A run stops after one prospect.** Without Auto Loot the server waits for you to take the loot of each
  prospect before the next cast. Turn on Auto Loot, or take the loot before `MassProspecting.LootTimeoutSec`
  runs out.
- **Create All only does one at a time.** The module is not running. ProspectingUI looks for it with
  `.massprospect ping`, so check that the worldserver was rebuilt with the module and that
  `MassProspecting.Enable` is `1`.
- **A run stops early.** Moving, an interrupted cast, having no stack of 5 left, or clicking again
  (`.massprospect stop`) all end a run. A single run is limited by `MassProspecting.MaxCount`.

## Credits

Author: [buildthehomelab](https://github.com/buildthehomelab)

## License

GNU AGPL v3. See [LICENSE](LICENSE).
