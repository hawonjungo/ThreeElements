// Injoker web leaderboards (GAMEPLAY_SPEC.md §33): a Cloudflare Worker with a D1 database. The Android version has
// its own boards on Google Play Games (§29); these are for the web version only.
//
//   GET  /top?board=play|survival&period=week|all    -> { entries: [ { name, score, at } ... ] }   (10 best names)
//   POST /score  { board, name, score, seconds, version }  -> { ok: true } or { error }
//
// PLAY scores are points, SURVIVAL scores milliseconds. The rules (names, plausibility, the week) are in rules.js.
import { BOARDS, PERIODS, TOP, SUBMITS_PER_HOUR, cleanName, checkScore, weekStart } from './rules.js';

function json(body, status, origin) {
	// a 204 (the answer to the browser's CORS preflight) must have no body at all
	return new Response(status === 204 ? null : JSON.stringify(body), {
		status,
		headers: {
			'content-type': 'application/json',
			'access-control-allow-origin': origin,
			'access-control-allow-methods': 'GET, POST, OPTIONS',
			'access-control-allow-headers': 'content-type',
			'cache-control': 'no-store',
		},
	});
}

async function hashAddress(address, salt) {
	const data = new TextEncoder().encode(salt + '|' + address);
	const digest = await crypto.subtle.digest('SHA-256', data);
	return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, '0')).join('').slice(0, 32);
}

export default {
	async fetch(request, env) {
		const allowed = (env.ALLOWED_ORIGINS || '').split(',').map((s) => s.trim()).filter(Boolean);
		const origin = request.headers.get('origin') || '';
		const allowOrigin = allowed.includes(origin) ? origin : (allowed[0] || '*');
		if (request.method === 'OPTIONS')
			return json({}, 204, allowOrigin);
		const url = new URL(request.url);
		const now = Math.floor(Date.now() / 1000);

		if (request.method === 'GET' && url.pathname === '/top') {
			const board = url.searchParams.get('board');
			const period = url.searchParams.get('period') || 'all';
			if (!BOARDS.includes(board) || !PERIODS.includes(period))
				return json({ error: 'bad request' }, 400, allowOrigin);
			const since = period === 'week' ? weekStart(now) : 0;
			const rows = await env.DB.prepare(
				'SELECT name, MAX(score) AS score, MIN(created_at) AS at FROM runs WHERE board = ?1 AND created_at >= ?2 ' +
				'GROUP BY name ORDER BY score DESC, at ASC LIMIT ?3').bind(board, since, TOP).all();
			return json({ entries: rows.results || [] }, 200, allowOrigin);
		}

		if (request.method === 'POST' && url.pathname === '/score') {
			if (allowed.length > 0 && !allowed.includes(origin))
				return json({ error: 'origin' }, 403, allowOrigin);
			let body;
			try { body = await request.json(); } catch (e) { return json({ error: 'bad request' }, 400, allowOrigin); }
			const name = cleanName(body.name);
			if (name === null)
				return json({ error: 'bad name' }, 400, allowOrigin);
			const problem = checkScore(body.board, body.score, body.seconds);
			if (problem !== null)
				return json({ error: problem }, 400, allowOrigin);
			const who = await hashAddress(request.headers.get('cf-connecting-ip') || 'unknown', env.SALT || 'injoker');
			const recent = await env.DB.prepare('SELECT COUNT(*) AS n FROM runs WHERE who = ?1 AND created_at >= ?2')
				.bind(who, now - 3600).first();
			if (recent && recent.n >= SUBMITS_PER_HOUR)
				return json({ error: 'too many' }, 429, allowOrigin);
			await env.DB.prepare('INSERT INTO runs (board, name, score, seconds, created_at, who, version) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)')
				.bind(body.board, name, body.score, Math.round(body.seconds), now, who, String(body.version || '').slice(0, 16)).run();
			return json({ ok: true }, 200, allowOrigin);
		}

		return json({ error: 'not found' }, 404, allowOrigin);
	},
};
