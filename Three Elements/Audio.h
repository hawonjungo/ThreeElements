#pragma once
#ifndef AUDIO_H_
#define AUDIO_H_

// Sound effects (presentation only). Every sound is synthesised in code at start-up as a short chiptune-style clip,
// so there are no sound files, no SDL_mixer and no licensing question; the clips are mixed by an SDL audio
// callback, several at once. If no audio device can be opened the game simply runs silent.
namespace audio
{
	enum class Sfx
	{
		OrbQuas,      // Q: ice chime
		OrbWex,       // W: electric buzz
		OrbExort,     // E: fire crackle
		Invoke,       // R with 3 orbs
		Cast,         // a cast that is not judged at once (Tornado launch, or no enemy to judge against)
		CastCorrect,
		CastWrong,
		Leak,         // an enemy reached the player
		GameOver,
		Start,        // a new session
		Immortal1,    // "IMMORTAL INCOMING": one, two or three horn calls by tier (spec §28 O-3)
		Immortal2,
		Immortal3,
		Count
	};

	bool Init();              // after SDL_Init(SDL_INIT_AUDIO); false = silent
	void Play(Sfx sfx);       // no-op while muted or without a device
	void SetMuted(bool muted);
	bool IsMuted();
	void Shutdown();
}

#endif
