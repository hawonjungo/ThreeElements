#include "Audio.h"
#include "Draw.h"  // draw::Hash doubles as the noise source
#include "Define.h"
#include <cmath>
#include <vector>

namespace audio
{
	namespace
	{
		const int SAMPLE_RATE = 44100;
		const float MASTER_VOLUME = 0.35f;
		const int MAX_VOICES = 12;  // sounds playing at the same time; the oldest is replaced when all are busy

		enum class Wave { Sine, Square, Triangle, Noise };

		std::vector<float> g_clips[static_cast<int>(Sfx::Count)];
		struct Voice { const std::vector<float>* clip; size_t pos; };
		Voice g_voices[MAX_VOICES] = {};
		SDL_AudioDeviceID g_device = 0;
		bool g_muted = false;

		// Adds one tone to `clip`, starting `start` seconds in: frequency slides from f0 to f1, 5 ms attack, then an
		// exponential decay. The clip grows as needed, so tones can be layered and chained.
		void Tone(std::vector<float>& clip, float start, float length, float f0, float f1, Wave wave, float volume)
		{
			size_t first = static_cast<size_t>(start * SAMPLE_RATE);
			size_t count = static_cast<size_t>(length * SAMPLE_RATE);
			if (clip.size() < first + count)
				clip.resize(first + count, 0.0f);
			float phase = 0.0f;
			float lowpass = 0.0f;
			for (size_t i = 0; i < count; ++i)
			{
				float t = static_cast<float>(i) / SAMPLE_RATE;
				float u = t / length;
				float freq = f0 + (f1 - f0) * u;
				phase += freq / SAMPLE_RATE;
				phase -= std::floor(phase);
				float s;
				switch (wave)
				{
				case Wave::Sine:     s = std::sin(6.2831853f * phase); break;
				case Wave::Square:   s = phase < 0.5f ? 0.6f : -0.6f; break;
				case Wave::Triangle: s = phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase; break;
				default:  // noise, smoothed more for lower "frequencies" so f0/f1 still shape its colour
					lowpass += (draw::Hash(static_cast<int>(i), static_cast<int>(f0)) * 2.0f - 1.0f - lowpass) * (freq / 8000.0f);
					s = lowpass * 2.0f;
					break;
				}
				float attack = t < 0.005f ? t / 0.005f : 1.0f;
				float decay = std::exp(-4.0f * u);
				clip[first + i] += s * volume * attack * decay;
			}
		}

		void Build()
		{
			std::vector<float>* c = g_clips;
			// Q: bright, glassy ice chime (two sines an octave apart)
			Tone(c[(int)Sfx::OrbQuas], 0.0f, 0.18f, 1320, 1480, Wave::Sine, 0.5f);
			Tone(c[(int)Sfx::OrbQuas], 0.02f, 0.14f, 2640, 2960, Wave::Sine, 0.2f);
			// W: short electric zap (square with a fast downward sweep) plus crackle
			Tone(c[(int)Sfx::OrbWex], 0.0f, 0.12f, 900, 500, Wave::Square, 0.35f);
			Tone(c[(int)Sfx::OrbWex], 0.0f, 0.10f, 6000, 3000, Wave::Noise, 0.25f);
			// E: fire whoosh (low noise) with a warm low tone
			Tone(c[(int)Sfx::OrbExort], 0.0f, 0.20f, 1200, 500, Wave::Noise, 0.6f);
			Tone(c[(int)Sfx::OrbExort], 0.0f, 0.16f, 220, 170, Wave::Triangle, 0.35f);
			// R: rising arpeggio
			const float arp[4] = { 523.25f, 659.25f, 783.99f, 1046.5f };
			for (int i = 0; i < 4; ++i)
				Tone(c[(int)Sfx::Invoke], i * 0.045f, 0.12f, arp[i], arp[i], Wave::Triangle, 0.45f);
			// cast: airy whoosh
			Tone(c[(int)Sfx::Cast], 0.0f, 0.18f, 2500, 5000, Wave::Noise, 0.35f);
			// correct: two quick bright notes
			Tone(c[(int)Sfx::CastCorrect], 0.0f, 0.08f, 880, 880, Wave::Square, 0.3f);
			Tone(c[(int)Sfx::CastCorrect], 0.07f, 0.18f, 1318.5f, 1318.5f, Wave::Square, 0.3f);
			// wrong: low, dull buzz
			Tone(c[(int)Sfx::CastWrong], 0.0f, 0.22f, 180, 130, Wave::Square, 0.3f);
			// leak: thump and crunch
			Tone(c[(int)Sfx::Leak], 0.0f, 0.30f, 140, 50, Wave::Sine, 0.8f);
			Tone(c[(int)Sfx::Leak], 0.0f, 0.18f, 1500, 400, Wave::Noise, 0.5f);
			// game over: slow falling notes
			const float fall[4] = { 392.0f, 329.63f, 261.63f, 196.0f };
			for (int i = 0; i < 4; ++i)
				Tone(c[(int)Sfx::GameOver], i * 0.16f, 0.3f, fall[i], fall[i] * 0.98f, Wave::Triangle, 0.5f);
			// start: two rising notes
			Tone(c[(int)Sfx::Start], 0.0f, 0.1f, 523.25f, 523.25f, Wave::Triangle, 0.45f);
			Tone(c[(int)Sfx::Start], 0.08f, 0.18f, 783.99f, 783.99f, Wave::Triangle, 0.45f);
			// overlord warning: low horn calls (a triangle with a square an octave down and a fifth on top), one per tier;
			// the last call of tiers 2 and 3 is higher
			const float calls[3][3] = { { 146.83f, 0.0f, 0.0f }, { 146.83f, 174.61f, 0.0f }, { 146.83f, 174.61f, 220.0f } };
			for (int tier = 0; tier < 3; ++tier)
			{
				std::vector<float>& horn = c[(int)Sfx::Overlord1 + tier];
				for (int i = 0; i <= tier; ++i)
				{
					float f = calls[tier][i], at = i * 0.42f;
					Tone(horn, at, 0.55f, f * 0.97f, f, Wave::Triangle, 0.6f);
					Tone(horn, at, 0.5f, f * 0.5f, f * 0.5f, Wave::Square, 0.3f);
					Tone(horn, at + 0.02f, 0.45f, f * 1.5f, f * 1.5f, Wave::Triangle, 0.2f);
				}
			}
		}

		// Runs on the audio thread (native) or from Web Audio (web): sums the active voices into the buffer.
		void SDLCALL Mix(void*, Uint8* stream, int bytes)
		{
			Sint16* out = reinterpret_cast<Sint16*>(stream);
			int samples = bytes / static_cast<int>(sizeof(Sint16));
			for (int i = 0; i < samples; ++i)
			{
				float sum = 0.0f;
				for (Voice& v : g_voices)
				{
					if (v.clip == NULL)
						continue;
					sum += (*v.clip)[v.pos++];
					if (v.pos >= v.clip->size())
						v.clip = NULL;
				}
				sum *= MASTER_VOLUME;
				if (sum > 1.0f) sum = 1.0f;
				if (sum < -1.0f) sum = -1.0f;
				out[i] = static_cast<Sint16>(sum * 32767.0f);
			}
		}
	}

	bool Init()
	{
		Build();
		SDL_AudioSpec want = {};
		want.freq = SAMPLE_RATE;
		want.format = AUDIO_S16SYS;
		want.channels = 1;
		want.samples = 1024;
		want.callback = Mix;
		g_device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);  // exactly this format: SDL converts if it must
		if (g_device == 0)
		{
			printf("No audio device (%s): running without sound\n", SDL_GetError());
			return false;
		}
		SDL_PauseAudioDevice(g_device, 0);
		return true;
	}

	void Play(Sfx sfx)
	{
		if (g_device == 0 || g_muted)
			return;
		const std::vector<float>* clip = &g_clips[static_cast<int>(sfx)];
		SDL_LockAudioDevice(g_device);
		int slot = 0;
		size_t oldest = 0;
		for (int i = 0; i < MAX_VOICES; ++i)
		{
			if (g_voices[i].clip == NULL) { slot = i; break; }
			if (g_voices[i].pos > oldest) { oldest = g_voices[i].pos; slot = i; }  // all busy: the one furthest along
		}
		g_voices[slot].clip = clip;
		g_voices[slot].pos = 0;
		SDL_UnlockAudioDevice(g_device);
	}

	void SetMuted(bool muted)
	{
		g_muted = muted;
		if (muted && g_device != 0)
		{
			SDL_LockAudioDevice(g_device);  // silence what is already playing too
			for (Voice& v : g_voices)
				v.clip = NULL;
			SDL_UnlockAudioDevice(g_device);
		}
	}

	bool IsMuted() { return g_muted; }

	void Shutdown()
	{
		if (g_device != 0)
		{
			SDL_CloseAudioDevice(g_device);
			g_device = 0;
		}
	}
}
