#pragma once
#ifndef ONLINE_H_
#define ONLINE_H_

// Online leaderboards (GAMEPLAY_SPEC.md §29): Google Play Games on Android, nothing anywhere else. Presentation
// only, like Audio: the Practice layer does not know about it, and every call is safe on every platform (on PC and
// the web, and in an Android build made without Play Games, they do nothing and Available() is false).
//
// The player's name and avatar belong to Google's own leaderboard screen; the game never reads or stores them.
namespace online
{
	enum class Board { Play, Survival };  // PLAY by score; SURVIVAL by survival time

	bool Available();   // this build can show the global boards (asked once, then remembered)
	bool SignedIn();    // the player is signed in to Play Games right now
	// PLAY: points. SURVIVAL: milliseconds. Not signed in: kept until the player signs in during this session.
	void Submit(Board board, long long score);
	void ShowBoards();  // Google's leaderboard screen; asks the player to sign in first when needed
}

#endif // ONLINE_H_
