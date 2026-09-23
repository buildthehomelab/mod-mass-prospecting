/*
 * Mass prospecting: repeat Prospecting for a player until a count is reached.
 *
 * The 3.3.5 client only lets an addon cast a spell from a hardware event, so an
 * addon can't chain prospects. Blizzard's Create All gets around that for crafts by
 * repeating on the server; this does the same for Prospecting. `.massprospect start
 * <ore> <count>` queues a run, and every player update the run either casts the
 * next Prospecting (a normal, non-triggered cast with its cast bar and checks) or
 * waits for the last prospect's loot to be taken, since the ore is only destroyed
 * when that loot window is released.
 *
 * Replies meant for ProspectingUI are system messages starting with "MASSPROSPECT:":
 *   MASSPROSPECT:PONG
 *   MASSPROSPECT:START:<ore>:<count>
 *   MASSPROSPECT:DONE:<ore>:<prospected>
 *   MASSPROSPECT:STOP:<ore>:<prospected>:<reason>
 * where reason is one of cancelled, interrupted, no_ore, loot_timeout or
 * cast_failed_<SpellCastResult>.
 */

#include "Bag.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>

using namespace Acore::ChatCommands;

namespace
{
constexpr uint32 SPELL_PROSPECTING = 31252;
constexpr uint32 PROSPECT_STACK = 5;

bool g_enabled = true;
uint32 g_maxCount = 200;
uint32 g_delayMs = 250;
uint32 g_lootTimeoutMs = 30000;

enum class Phase
{
    WaitingToCast,  // timer counts down to the next cast
    Casting,        // our Prospecting cast is in progress
    WaitingForLoot  // cast finished, the loot window is still open
};

struct Run
{
    uint32 oreEntry = 0;
    uint32 remaining = 0;
    uint32 prospected = 0;
    Phase phase = Phase::WaitingToCast;
    uint32 timerMs = 0;
};

// Runs are touched from map threads (player updates, spell hooks) and from
// command handling. Recursive because casting can call back into our hooks.
std::recursive_mutex g_lock;
std::unordered_map<ObjectGuid::LowType, Run> g_runs;

void SendProtocol(Player* player, std::string const& message)
{
    ChatHandler(player->GetSession()).SendSysMessage("MASSPROSPECT:" + message);
}

void EndRun(Player* player, bool finished, std::string const& reason = "")
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);

    auto itr = g_runs.find(player->GetGUID().GetCounter());
    if (itr == g_runs.end())
        return;

    Run const run = itr->second;
    g_runs.erase(itr);

    if (finished)
        SendProtocol(player, "DONE:" + std::to_string(run.oreEntry) + ":" + std::to_string(run.prospected));
    else
        SendProtocol(player, "STOP:" + std::to_string(run.oreEntry) + ":" + std::to_string(run.prospected) + ":" + reason);
}

// Smallest stack of 5 or more, so broken stacks get used up first (same rule
// as ProspectingUI). Skips items being traded or still holding an open prospect's loot.
Item* FindProspectStack(Player* player, uint32 oreEntry)
{
    Item* best = nullptr;

    auto consider = [&](Item* item)
    {
        if (!item || item->GetEntry() != oreEntry || item->IsInTrade() || item->m_lootGenerated)
            return;

        if (item->GetCount() < PROSPECT_STACK)
            return;

        if (!best || item->GetCount() < best->GetCount())
            best = item;
    };

    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        consider(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));

    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        if (Bag* bag = player->GetBagByPos(bagSlot))
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                consider(bag->GetItemByPos(static_cast<uint8>(slot)));

    return best;
}

bool IsCastingProspecting(Player* player)
{
    Spell const* spell = player->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    return spell && spell->GetSpellInfo()->Id == SPELL_PROSPECTING;
}

void CastNextProspect(Player* player, Run& run)
{
    Item* ore = FindProspectStack(player, run.oreEntry);
    if (!ore)
    {
        EndRun(player, false, "no_ore");
        return;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_PROSPECTING);
    if (!spellInfo)
    {
        EndRun(player, false, "cast_failed_no_spell");
        return;
    }

    SpellCastTargets targets;
    targets.SetItemTarget(ore);

    // Set before casting: a zero cast time would reach OnSpellCast right away.
    run.phase = Phase::Casting;

    SpellCastResult result = player->CastSpell(targets, spellInfo, nullptr, TRIGGERED_NONE);
    if (result != SPELL_CAST_OK)
        EndRun(player, false, "cast_failed_" + std::to_string(uint32(result)));
}
}

class MassProspectingWorldScript : public WorldScript
{
public:
    MassProspectingWorldScript() : WorldScript("MassProspectingWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD
    })
    {
    }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        g_enabled = sConfigMgr->GetOption<bool>("MassProspecting.Enable", true);
        g_maxCount = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("MassProspecting.MaxCount", 200));
        g_delayMs = sConfigMgr->GetOption<uint32>("MassProspecting.DelayMs", 250);
        g_lootTimeoutMs = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("MassProspecting.LootTimeoutSec", 30)) * IN_MILLISECONDS;
    }
};

class MassProspectingPlayerScript : public PlayerScript
{
public:
    MassProspectingPlayerScript() : PlayerScript("MassProspectingPlayerScript", {
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_LOGOUT
    })
    {
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        std::lock_guard<std::recursive_mutex> guard(g_lock);

        auto itr = g_runs.find(player->GetGUID().GetCounter());
        if (itr == g_runs.end())
            return;

        Run& run = itr->second;

        switch (run.phase)
        {
            case Phase::Casting:
                // OnSpellCast moves a finished cast on; if our cast is gone
                // without that, it was interrupted (moved, pushed back, failed).
                if (!IsCastingProspecting(player))
                    EndRun(player, false, "interrupted");
                break;

            case Phase::WaitingForLoot:
                if (player->GetLootGUID().IsEmpty())
                {
                    if (run.remaining == 0)
                    {
                        EndRun(player, true);
                        return;
                    }

                    run.phase = Phase::WaitingToCast;
                    run.timerMs = g_delayMs;
                }
                else if ((run.timerMs += diff) >= g_lootTimeoutMs)
                {
                    EndRun(player, false, "loot_timeout");
                }
                break;

            case Phase::WaitingToCast:
                if (run.timerMs > diff)
                {
                    run.timerMs -= diff;
                    break;
                }

                run.timerMs = 0;

                // Let whatever the player is casting finish first.
                if (player->IsNonMeleeSpellCast(false) || !player->GetLootGUID().IsEmpty())
                    break;

                CastNextProspect(player, run);
                break;
        }
    }

    void OnPlayerLogout(Player* player) override
    {
        std::lock_guard<std::recursive_mutex> guard(g_lock);
        g_runs.erase(player->GetGUID().GetCounter());
    }
};

class MassProspectingSpellScript : public AllSpellScript
{
public:
    MassProspectingSpellScript() : AllSpellScript("MassProspectingSpellScript", {
        ALLSPELLHOOK_ON_CAST
    })
    {
    }

    // Called once a cast has gone off, before its loot window is released.
    void OnSpellCast(Spell* /*spell*/, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        if (spellInfo->Id != SPELL_PROSPECTING || !caster || !caster->IsPlayer())
            return;

        std::lock_guard<std::recursive_mutex> guard(g_lock);

        auto itr = g_runs.find(caster->GetGUID().GetCounter());
        if (itr == g_runs.end() || itr->second.phase != Phase::Casting)
            return;

        Run& run = itr->second;
        run.prospected++;
        run.remaining--;
        run.phase = Phase::WaitingForLoot;
        run.timerMs = 0;
    }
};

class MassProspectingCommandScript : public CommandScript
{
public:
    MassProspectingCommandScript() : CommandScript("MassProspectingCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable massProspectTable =
        {
            { "ping",  HandlePing,  SEC_PLAYER, Console::No },
            { "start", HandleStart, SEC_PLAYER, Console::No },
            { "stop",  HandleStop,  SEC_PLAYER, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "massprospect", massProspectTable },
        };

        return commandTable;
    }

    static bool HandlePing(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player || !g_enabled)
            return false;

        SendProtocol(player, "PONG");
        return true;
    }

    static bool HandleStart(ChatHandler* handler, uint32 oreEntry, uint32 count)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (!g_enabled)
        {
            handler->SendSysMessage("Mass prospecting is disabled on this server.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!player->HasSpell(SPELL_PROSPECTING))
        {
            handler->SendSysMessage("You don't know Prospecting.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(oreEntry);
        if (!proto || !proto->HasFlag(ITEM_FLAG_IS_PROSPECTABLE))
        {
            handler->SendSysMessage("That item can't be prospected.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        count = std::min(std::max<uint32>(count, 1), g_maxCount);

        std::lock_guard<std::recursive_mutex> guard(g_lock);

        // A new run replaces whatever was running.
        g_runs.erase(player->GetGUID().GetCounter());

        Run run;
        run.oreEntry = oreEntry;
        run.remaining = count;
        run.phase = Phase::WaitingToCast;
        run.timerMs = 0;
        g_runs[player->GetGUID().GetCounter()] = run;

        SendProtocol(player, "START:" + std::to_string(oreEntry) + ":" + std::to_string(count));
        return true;
    }

    static bool HandleStop(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        std::lock_guard<std::recursive_mutex> guard(g_lock);

        auto itr = g_runs.find(player->GetGUID().GetCounter());
        if (itr == g_runs.end())
            return true;

        if (itr->second.phase == Phase::Casting && IsCastingProspecting(player))
            player->InterruptSpell(CURRENT_GENERIC_SPELL, false);

        EndRun(player, false, "cancelled");
        return true;
    }
};

void AddMassProspectingScripts()
{
    new MassProspectingWorldScript();
    new MassProspectingPlayerScript();
    new MassProspectingSpellScript();
    new MassProspectingCommandScript();
}
