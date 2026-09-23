# ProspectingUI

A World of Warcraft 3.3.5a addon that gives Prospecting its own profession window. It uses the
stock tradeskill frame art, so it looks like part of the default Blizzard UI.

## What it does

- Lists every ore you can prospect, grouped under Classic, Outland and Northrend headers.
- Colors each ore like a tradeskill recipe (orange, yellow, green or grey) based on your
  Jewelcrafting skill. Ores you can't prospect yet show in red.
- Shows `[n]` next to an ore for how many times you can prospect it with the stacks in your bags.
- The detail pane shows the Jewelcrafting rank the ore needs, the reagent count (have/5), and every
  gem it can give as an icon grid (uncommon first, then rare). Hover over a gem for its tooltip.
- The **Prospecting** button prospects the selected ore. It picks the smallest stack that has 5 or
  more, so broken stacks get used up first.
- Set how many times to prospect with the `<` and `>` arrows, or type a number in the box, the same
  as crafting. **Create All** fills in every stack you have.
  - **With the `mod-mass-prospecting` server module:** one click prospects the whole amount
    back-to-back, like Blizzard's Create All. Moving, or clicking again, stops it. Turn on Auto
    Loot so it doesn't wait on the loot window.
  - **Without it:** the game only lets an addon cast a spell on a real click or key press, so each
    click of **Prospecting** prospects one stack of 5 and the box counts down. To go faster, put
    `/click ProspectingFrameProspectButton` in a macro and bind that macro to a key.
- **Have Materials** hides ores you can't prospect right now.
- Keeps per-character counts of how much ore you've prospected, in total and for each ore.
- Shift-click an ore or gem to link it in chat.

## Reagent Bank

If ReagentBankUI is installed, its profession sidebar shows on the Prospecting window the same way
it does on the other profession windows. The sidebar has Withdraw Needed, Add to AH List, the
prepare count, and Auto-deposit leftovers, and the ore slot shows "+N bank". Each prospect needs 5
of the selected ore, so Withdraw x4 pulls enough for 4 prospects (20 ore, minus what you already
carry). The prepare count and the amount box stay in sync.

## Opening it

- `/prospect` or `/prospecting`
- Or bind a key under Key Bindings → Prospecting.

## Install

Copy the `ProspectingUI` folder into `World of Warcraft/Interface/AddOns/`.

## Limits

- The addon only changes what you see. The server still runs the normal Prospecting rules: you need
  Jewelcrafting, the Prospecting spell, and enough Jewelcrafting skill for each ore.
- The window closes when you enter combat and can't be opened during combat. It holds a secure
  button, and Blizzard doesn't allow addons to show or hide secure frames in combat.
- The gem grid shows what each ore can give on a stock server. Your server's
  `prospecting_loot_template` decides the actual drops and chances.
