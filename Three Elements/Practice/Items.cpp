#include "Items.h"

namespace practice
{
	namespace
	{
		// GAMEPLAY_SPEC.md §27 I-2. INITIAL TUNING VALUES (prices are high on purpose: gold is earned slowly).
		const ItemDefinition kItems[ITEM_COUNT] =
		{
			{ ItemId::Blink, ItemKind::Active, 2, {
				{ "BLINK DAGGER", "THE ENEMY WALKS BACK 3 S", 40.0f, 1500, 3.0f },
				{ "SWIFT BLINK", "THE ENEMY WALKS BACK 4 S", 30.0f, 4000, 4.0f } } },
			{ ItemId::Refresher, ItemKind::Active, 2, {
				{ "REFRESHER ORB", "THE CHAIN LOSES 2 SKILLS", 90.0f, 3000, 2.0f },
				{ "REFRESHER ORB II", "THE CHAIN LOSES 2 SKILLS", 60.0f, 7000, 2.0f } } },
			{ ItemId::Euls, ItemKind::Active, 2, {
				{ "EUL'S SCEPTER", "THE ENEMY STANDS STILL 2 S", 25.0f, 1200, 2.0f },
				{ "WIND WAKER", "STILL 3 S, PUSHED BACK", 20.0f, 3500, 3.0f } } },
			{ ItemId::Bkb, ItemKind::Active, 2, {
				{ "BLACK KING BAR", "NO LIFE LOST FOR 5 S", 120.0f, 2500, 5.0f },
				{ "BKB II", "NO LIFE LOST FOR 7 S", 90.0f, 6000, 7.0f } } },
			{ ItemId::Midas, ItemKind::Passive, 2, {
				{ "HAND OF MIDAS", "+50% GOLD", 0.0f, 2000, 0.5f },
				{ "MIDAS II", "+100% GOLD", 0.0f, 6000, 1.0f } } },
			{ ItemId::Octarine, ItemKind::Passive, 2, {
				{ "OCTARINE CORE", "ITEM COOLDOWNS -25%", 0.0f, 3500, 0.25f },
				{ "OCTARINE II", "ITEM COOLDOWNS -40%", 0.0f, 8000, 0.40f } } },
			{ ItemId::Aghanim, ItemKind::Passive, 2, {
				{ "AGHANIM'S SCEPTER", "CHOOSE 1 OF 2 RUNES", 0.0f, 4000, 2.0f },
				{ "AGHANIM'S BLESSING", "CHOOSE 1 OF 3 RUNES", 0.0f, 9000, 3.0f } } },
			{ ItemId::Salve, ItemKind::Consumable, 1, {
				{ "HEALING SALVE", "+1 LIFE", 0.0f, 60, 1.0f }, { "", "", 0.0f, 0, 0.0f } } },
			{ ItemId::Cheese, ItemKind::Consumable, 1, {
				{ "CHEESE", "+2 LIVES", 0.0f, 300, 2.0f }, { "", "", 0.0f, 0, 0.0f } } },
			{ ItemId::Smoke, ItemKind::Consumable, 1, {
				{ "SMOKE OF DECEIT", "ENEMIES SLOWED 8 S", 0.0f, 80, 8.0f }, { "", "", 0.0f, 0, 0.0f } } },
			{ ItemId::GreaterSmoke, ItemKind::Consumable, 1, {
				{ "GREATER SMOKE", "ENEMIES SLOWED 12 S", 0.0f, 200, 12.0f }, { "", "", 0.0f, 0, 0.0f } } },
		};
	}

	const ItemDefinition& GetItemDefinition(ItemId id) { return kItems[static_cast<int>(id)]; }

	const ItemDefinition& GetItemDefinition(int index)
	{
		if (index < 0 || index >= ITEM_COUNT)
			index = 0;
		return kItems[index];
	}

	Inventory EmptyInventory()
	{
		Inventory inv;
		for (int i = 0; i < ITEM_COUNT; ++i)
			inv.level[i] = inv.count[i] = 0;
		for (int s = 0; s < ITEM_SLOTS; ++s)
			inv.slot[s] = ITEM_NONE;
		for (int m = 0; m < MATERIAL_COUNT; ++m)
			inv.material[m] = 0;
		return inv;
	}

	const char* MaterialName(Material material)
	{
		switch (material)
		{
		case Material::PointBooster: return "POINT BOOSTER";
		case Material::MysticStaff:  return "MYSTIC STAFF";
		default:                     return "SACRED RELIC";
		}
	}

	Material MaterialOfBoss(int boss)
	{
		return boss <= 2 ? Material::PointBooster : boss <= 6 ? Material::MysticStaff : Material::SacredRelic;
	}

	// O-11: level 1 of Aghanim's Scepter and every level-2 upgrade need a material; consumables never do.
	int RequiredMaterial(const Inventory& inv, ItemId id)
	{
		int level = inv.level[static_cast<int>(id)];  // the level owned now; the purchase gives level + 1
		return level < GetItemDefinition(id).levels ? MaterialForLevel(id, level + 1) : MATERIAL_NONE;
	}

	int MaterialForLevel(ItemId id, int level)
	{
		const ItemDefinition& def = GetItemDefinition(id);
		if (def.kind == ItemKind::Consumable || level < 1 || level > def.levels)
			return MATERIAL_NONE;
		if (level == 1)
			return id == ItemId::Aghanim ? static_cast<int>(Material::PointBooster) : MATERIAL_NONE;
		switch (id)
		{
		case ItemId::Blink:
		case ItemId::Euls:      return static_cast<int>(Material::PointBooster);
		case ItemId::Bkb:
		case ItemId::Midas:
		case ItemId::Octarine:  return static_cast<int>(Material::MysticStaff);
		default:                return static_cast<int>(Material::SacredRelic);  // Refresher II, Aghanim's Blessing
		}
	}

	bool CanBuy(const Inventory& inv, ItemId id, int gold)
	{
		int price = NextPrice(inv, id);
		int material = RequiredMaterial(inv, id);
		return price > 0 && gold >= price && (material == MATERIAL_NONE || inv.material[material] > 0);
	}

	void AddMaterial(Inventory& inv, Material material)
	{
		int m = static_cast<int>(material);
		if (inv.material[m] < MATERIAL_MAX)
			++inv.material[m];
	}

	bool Owns(const Inventory& inv, ItemId id)
	{
		int i = static_cast<int>(id);
		return GetItemDefinition(id).kind == ItemKind::Consumable ? inv.count[i] > 0 : inv.level[i] > 0;
	}

	int CurrentLevel(const Inventory& inv, ItemId id)
	{
		int i = static_cast<int>(id);
		return GetItemDefinition(id).kind == ItemKind::Consumable ? (inv.count[i] > 0 ? 1 : 0) : inv.level[i];
	}

	int NextPrice(const Inventory& inv, ItemId id)
	{
		const ItemDefinition& def = GetItemDefinition(id);
		int i = static_cast<int>(id);
		if (def.kind == ItemKind::Consumable)
			return inv.count[i] < ITEM_MAX_STACK ? def.level[0].price : 0;
		return inv.level[i] < def.levels ? def.level[inv.level[i]].price : 0;
	}

	int SlotOf(const Inventory& inv, ItemId id)
	{
		for (int s = 0; s < ITEM_SLOTS; ++s)
			if (inv.slot[s] == static_cast<int>(id))
				return s;
		return ITEM_NONE;
	}

	bool IsEquipped(const Inventory& inv, ItemId id) { return SlotOf(inv, id) != ITEM_NONE; }

	bool Equip(Inventory& inv, ItemId id)
	{
		if (!Owns(inv, id))
			return false;
		if (IsEquipped(inv, id))
			return true;
		for (int s = 0; s < ITEM_SLOTS; ++s)
		{
			if (inv.slot[s] == ITEM_NONE)
			{
				inv.slot[s] = static_cast<int>(id);
				return true;
			}
		}
		return false;  // all six slots are taken
	}

	void Unequip(Inventory& inv, ItemId id)
	{
		int s = SlotOf(inv, id);
		if (s != ITEM_NONE)
			inv.slot[s] = ITEM_NONE;
	}

	bool Buy(Inventory& inv, ItemId id, int& gold)
	{
		if (!CanBuy(inv, id, gold))
			return false;
		int material = RequiredMaterial(inv, id);
		if (material != MATERIAL_NONE)
			--inv.material[material];  // one material is used up by the purchase
		gold -= NextPrice(inv, id);
		int i = static_cast<int>(id);
		if (GetItemDefinition(id).kind == ItemKind::Consumable)
			++inv.count[i];
		else
			++inv.level[i];
		Equip(inv, id);  // a new item goes straight into a free slot (if there is one)
		return true;
	}

	int EquippedLevel(const Inventory& inv, ItemId id)
	{
		return IsEquipped(inv, id) ? CurrentLevel(inv, id) : 0;
	}
}
