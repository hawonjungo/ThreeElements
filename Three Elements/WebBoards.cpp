#include "WebBoards.h"

#include <cstdio>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// The browser side. The address comes from the page (web/shell.html, filled in by web/build.sh); the name lives in
// localStorage; the last board fetched is kept in Module.injokerBoards until the game reads it.
EM_JS(int, js_boards_ready, (), {
	var url = (typeof window !== 'undefined' && window.INJOKER_BOARDS_URL) || '';
	return url.length > 0 && url.indexOf('%') < 0 ? 1 : 0;
});

EM_JS(void, js_boards_name, (char* out, int size), {
	var name = '';
	try { name = localStorage.getItem('threeElements_webName') || ''; } catch (e) {}
	stringToUTF8(name, out, size);
});

EM_JS(int, js_boards_ask_name, (), {
	var old = '';
	try { old = localStorage.getItem('threeElements_webName') || ''; } catch (e) {}
	var typed = window.prompt('Your name for the world ranking (2 to 12 letters or digits):', old);
	if (typed === null)
		return old.length > 0 ? 1 : 0;
	var name = typed.toUpperCase().replace(/[^A-Z0-9 \-.]/g, '').replace(/\s+/g, ' ').trim().slice(0, 12);
	if (name.replace(/[^A-Z0-9]/g, '').length < 2)
		return old.length > 0 ? 1 : 0;
	try { localStorage.setItem('threeElements_webName', name); } catch (e) {}
	return 1;
});

EM_JS(void, js_boards_submit, (int board, double score, double seconds), {
	var name = '';
	try { name = localStorage.getItem('threeElements_webName') || ''; } catch (e) {}
	if (!name)
		return;
	fetch(window.INJOKER_BOARDS_URL + '/score', {
		method: 'POST',
		headers: { 'content-type': 'application/json' },
		body: JSON.stringify({ board: board === 0 ? 'play' : 'survival', name: name, score: Math.round(score),
			seconds: seconds, version: 'web' })
	}).then(function (r) { return r.json(); })
	  .then(function (j) { if (j.error) console.log('[boards] not accepted: ' + j.error); })
	  .catch(function () { console.log('[boards] could not send the run'); });
});

EM_JS(void, js_boards_request, (int board, int period, int slot), {
	var all = Module.injokerBoards = Module.injokerBoards || [];
	var state = all[slot] = all[slot] || { id: 0 };
	var id = ++state.id;
	state.status = 1;
	state.entries = [];
	fetch(window.INJOKER_BOARDS_URL + '/top?board=' + (board === 0 ? 'play' : 'survival') + '&period=' + (period === 0 ? 'week' : 'all'))
		.then(function (r) { if (!r.ok) throw new Error('status ' + r.status); return r.json(); })
		.then(function (j) { if (state.id === id) { state.entries = j.entries || []; state.status = 2; } })
		.catch(function () { if (state.id === id) state.status = 3; });
});

EM_JS(int, js_boards_status, (int slot), {
	var s = Module.injokerBoards && Module.injokerBoards[slot];
	return s ? s.status : 0;
});

EM_JS(int, js_boards_count, (int slot), {
	var s = Module.injokerBoards && Module.injokerBoards[slot];
	return s && s.entries ? Math.min(s.entries.length, 10) : 0;
});

EM_JS(double, js_boards_entry, (int i, char* name, int size, int slot), {
	var e = Module.injokerBoards[slot].entries[i];
	stringToUTF8(String(e.name), name, size);
	return Number(e.score) || 0;
});
#endif

namespace webboards
{
#ifdef __EMSCRIPTEN__
	namespace
	{
		char g_name[16] = "";
	}

	bool Available() { return js_boards_ready() != 0; }

	const char* Name()
	{
		js_boards_name(g_name, sizeof(g_name));
		return g_name;
	}

	bool AskName() { return Available() && js_boards_ask_name() != 0; }

	void Submit(Board board, long long score, float seconds)
	{
		if (Available() && score > 0)
			js_boards_submit(board == Board::Play ? 0 : 1, static_cast<double>(score), static_cast<double>(seconds));
	}

	void Request(Board board, Period period, int slot)
	{
		if (Available())
			js_boards_request(board == Board::Play ? 0 : 1, period == Period::Week ? 0 : 1, slot);
	}

	Status GetStatus(int slot) { return static_cast<Status>(js_boards_status(slot)); }
	int Count(int slot) { return js_boards_count(slot); }

	Entry Get(int i, int slot)
	{
		Entry e = {};
		if (i >= 0 && i < Count(slot))
			e.score = static_cast<long long>(js_boards_entry(i, e.name, sizeof(e.name), slot));
		return e;
	}
#else
	bool Available() { return false; }
	const char* Name() { return ""; }
	bool AskName() { return false; }
	void Submit(Board, long long, float) {}
	void Request(Board, Period, int) {}
	Status GetStatus(int) { return Status::Idle; }
	int Count(int) { return 0; }
	Entry Get(int, int) { return Entry{}; }
#endif
}
