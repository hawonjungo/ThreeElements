#pragma once
#ifndef ITEMS_H_
#define ITEMS_H_

// Shop and items (GAMEPLAY_SPEC.md §27, update 1.4): the item table, the player's inventory, buying, upgrading and
// equipping. Pure data and rules like the rest of the Practice layer (no SDL, no files); the item effects during a
// PLAY run live in PracticeSession, saving the inventory in the presentation.

namespace practice
{
	enum class ItemId
	{
		Blink, Refresher, Euls, Bkb, Midas, Octarine, Aghanim,  // permanent, two levels
		Salve, Cheese, Smoke, GreaterSmoke                        // consumables
	};
	const int ITEM_COUNT = 11;
	const int ITEM_SLOTS = 6;           // 2 rows of 3, like the Dota 2 inventory (I-3)
	const int ITEM_MAX_STACK = 9;       // consumables owned at most, per item
	const int ITEM_NONE = -1;           // an empty slot

	enum class ItemKind { Active, Passive, Consumable };

	struct ItemLevel
	{
		const char* name;
		const char* effect;     // one short line for the shop (English capitals)
		float cooldown;         // s; 0 for passives and consumables
		int price;              // gold: the level itself for permanent items, one unit for consumables
		float value;            // strength: seconds, lives, skills removed, gold bonus, cooldown cut, rune choices
	};

	struct ItemDefinition
	{
		ItemId id;
		ItemKind kind;
		int levels;             // 2 for permanent items, 1 for consumables
		ItemLevel level[2];
	};

	const ItemDefinition& GetItemDefinition(ItemId id);
	const ItemDefinition& GetItemDefinition(int index);  // 0 <= index < ITEM_COUNT

	struct Inventory
	{
		int level[ITEM_COUNT];  // permanent items: 0 = not owned, 1, 2
		int count[ITEM_COUNT];  // consumables: units owned
		int slot[ITEM_SLOTS];   // the item in each slot (as an int), or ITEM_NONE
	};

	Inventory EmptyInventory();
	bool Owns(const Inventory& inv, ItemId id);             // level >= 1, or at least one unit
	int CurrentLevel(const Inventory& inv, ItemId id);      // 1 for an owned consumable
	// Price of the next purchase (level 1, the upgrade, or one more unit); 0 when there is nothing left to buy.
	int NextPrice(const Inventory& inv, ItemId id);
	// Spends the price from `gold` and gives the item (a new item is equipped in the first free slot). false = not
	// enough gold or nothing to buy; nothing changes then.
	bool Buy(Inventory& inv, ItemId id, int& gold);
	int SlotOf(const Inventory& inv, ItemId id);            // its slot, or ITEM_NONE
	bool Equip(Inventory& inv, ItemId id);                  // into the first free slot; false if not owned / full
	void Unequip(Inventory& inv, ItemId id);
	bool IsEquipped(const Inventory& inv, ItemId id);
	// The equipped level of a passive item (0 when it is not equipped): passives only work from a slot (I-3).
	int EquippedLevel(const Inventory& inv, ItemId id);
}

#endif // ITEMS_H_
