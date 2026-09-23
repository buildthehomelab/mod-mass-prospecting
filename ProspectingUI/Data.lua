---
-- ProspectingUI: ore -> gem table for 3.3.5a.
-- The client has no API for "is this item prospectable" or "what does it give",
-- so the prospecting_loot_template knowledge lives here. `skill` is the
-- Jewelcrafting rank the server checks before letting you prospect that ore.
-- `yields` lists uncommon gems first, then rare (and epic for Titanium).

local _, private = ...;

-- Shared gem lists
local TBC_GEMS = {
	23077, 21929, 23112, 23079, 23117, 23107,  -- Blood Garnet, Flame Spessarite, Golden Draenite, Deep Peridot, Azure Moonstone, Shadow Draenite
	23436, 23439, 23440, 23437, 23438, 23441,  -- Living Ruby, Noble Topaz, Dawnstone, Talasite, Star of Elune, Nightseye
};

local WOTLK_GEMS = {
	36917, 36923, 36932, 36929, 36926, 36920,  -- Bloodstone, Chalcedony, Dark Jade, Huge Citrine, Shadow Crystal, Sun Crystal
	36918, 36921, 36933, 36930, 36924, 36927,  -- Scarlet Ruby, Autumn's Glow, Forest Emerald, Monarch Topaz, Sky Sapphire, Twilight Opal
};

local function With(list, ...)
	local copy = {};
	for _, id in ipairs(list) do copy[#copy + 1] = id; end
	for i = 1, select("#", ...) do copy[#copy + 1] = select(i, ...); end
	return copy;
end

-- Groups are shown as headers in the list, in this order.
private.groups = {
	{
		name = "Classic Ores",
		ores = {
			{ id = 2770,  name = "Copper Ore",  skill = 20,  yields = { 774, 818, 1210 } },
			{ id = 2771,  name = "Tin Ore",     skill = 50,  yields = { 1210, 1206, 1705, 1529, 3864 } },
			{ id = 2772,  name = "Iron Ore",    skill = 125, yields = { 1529, 3864, 1705, 7909, 7910 } },
			{ id = 3858,  name = "Mithril Ore", skill = 175, yields = { 3864, 7909, 7910, 12361, 12799, 12364, 12800 } },
			{ id = 10620, name = "Thorium Ore", skill = 250, yields = { 7910, 12361, 12799, 12364, 12800 } },
		},
	},
	{
		name = "Outland Ores",
		ores = {
			{ id = 23424, name = "Fel Iron Ore",   skill = 275, yields = TBC_GEMS },
			{ id = 23425, name = "Adamantite Ore", skill = 325, yields = With(TBC_GEMS, 24243) },  -- + Adamantite Powder
		},
	},
	{
		name = "Northrend Ores",
		ores = {
			{ id = 36909, name = "Cobalt Ore",   skill = 350, yields = WOTLK_GEMS },
			{ id = 36912, name = "Saronite Ore", skill = 400, yields = WOTLK_GEMS },
			-- + Cardinal Ruby, King's Amber, Eye of Zul, Ametrine, Majestic Zircon, Dreadstone, Titanium Powder
			{ id = 36910, name = "Titanium Ore", skill = 450, yields = With(WOTLK_GEMS, 36919, 36922, 36934, 36931, 36925, 36928, 46849) },
		},
	},
};

-- ore id -> its group, for quick lookups from bag scans.
private.oreGroup = {};
for _, group in ipairs(private.groups) do
	for _, ore in ipairs(group.ores) do
		private.oreGroup[ore.id] = group;
	end
end
