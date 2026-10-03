// The rules of the web leaderboards (GAMEPLAY_SPEC.md §33), kept apart from the Worker so that they can be tested
// with plain Node (node web/boards/test.mjs). No Cloudflare, no database here.

export const BOARDS = ['play', 'survival'];
export const PERIODS = ['week', 'all'];
export const NAME_MIN = 2;
export const NAME_MAX = 12;
export const TOP = 10;
export const SUBMITS_PER_HOUR = 30;   // per player address (stored only as a salted hash)

// Words a name may not contain (after removing spaces and punctuation). Short on purpose: names are also
// checked by eye, and a long list blocks innocent names.
const BLOCKED = ['FUCK', 'SHIT', 'CUNT', 'DICK', 'NIGG', 'FAGG', 'WHORE', 'RAPE', 'HITLER', 'NAZI',
	'DITME', 'DCM', 'DMM', 'CAILON', 'DUMA', 'DIME'];

// The pixel font of the game draws A-Z, 0-9, space and - . ' : a name uses those only, in capitals.
export function cleanName(raw) {
	if (typeof raw !== 'string')
		return null;
	const name = raw.toUpperCase().replace(/[^A-Z0-9 \-.]/g, '').replace(/\s+/g, ' ').trim();
	if (name.length < NAME_MIN || name.length > NAME_MAX)
		return null;
	const squashed = name.replace(/[^A-Z0-9]/g, '');
	if (squashed.length < NAME_MIN || BLOCKED.some((w) => squashed.includes(w)))
		return null;
	return name;
}

// Basic plausibility (owner 2026-10-03: the basic level of protection). PLAY: points; SURVIVAL: milliseconds.
// `seconds` is the run's length as the game measured it.
export function checkScore(board, score, seconds) {
	if (!BOARDS.includes(board) || !Number.isInteger(score) || typeof seconds !== 'number' || !isFinite(seconds))
		return 'bad request';
	if (seconds < 1 || seconds > 36000)
		return 'bad length';
	if (board === 'play') {
		// the fastest scoring there is: an enemy about every half second, 1 to 10 points, an Immortal 50, all of
		// it doubled by a rune; 8 points a second over a whole run is far above what a person can do
		if (score < 1 || score > 100000 || score > seconds * 8 + 200)
			return 'implausible score';
	} else {
		if (score < 1000 || score > 36000000 || Math.abs(score - seconds * 1000) > 3000)
			return 'implausible time';
	}
	return null;
}

// Monday 00:00 UTC of the week of `nowSeconds`, as Unix seconds: the weekly board starts there.
export function weekStart(nowSeconds) {
	const day = Math.floor(nowSeconds / 86400);           // days since 1970-01-01, a Thursday
	const weekday = (day + 3) % 7;                        // 0 = Monday
	return (day - weekday) * 86400;
}
