#pragma once
#ifndef WEBBOARDS_H_
#define WEBBOARDS_H_

// Web leaderboards (GAMEPLAY_SPEC.md §33): the web version's world ranking, kept by a small server of our own
// (web/boards: a Cloudflare Worker with a D1 database). The Android version has Google Play Games instead (§29).
// Presentation only, like Online.h: the Practice layer does not know about it. On PC and Android every call does
// nothing and Available() is false; on the web too until the build is given the server's address (web/deploy.conf).
//
// The player types a name (2 to 12 of A-Z 0-9 space - .); it is kept in the browser and sent with each run.
namespace webboards
{
	enum class Board { Play, Survival };   // PLAY by points, SURVIVAL by milliseconds
	enum class Period { Week, All };       // the week starts on Monday 00:00 UTC
	enum class Status { Idle, Loading, Ready, Failed };
	const int TOP = 10;

	struct Entry
	{
		char name[16];
		long long score;
	};

	bool Available();
	const char* Name();         // "" until the player has typed one
	bool AskName();             // the browser's own text box; true when a name is set afterwards
	void Submit(Board board, long long score, float seconds);  // only with a name; nothing comes back
	// Two boards can be on their way at once: the WORLD RANKING screen's (slot 0) and the menu's top 3 (slot 1).
	enum Slot { SCREEN = 0, PREVIEW = 1 };
	void Request(Board board, Period period, int slot = SCREEN);  // the board is fetched; see GetStatus()
	Status GetStatus(int slot = SCREEN);
	int Count(int slot = SCREEN);   // entries of the last board fetched into the slot (0..TOP)
	Entry Get(int i, int slot = SCREEN);
}

#endif // WEBBOARDS_H_
