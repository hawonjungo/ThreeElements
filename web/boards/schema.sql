-- Injoker web leaderboards (GAMEPLAY_SPEC.md §33). One row per run sent; the boards show each name's best.
-- `who` is a salted hash of the sender's address, kept only to limit how often one player can send.
CREATE TABLE IF NOT EXISTS runs (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	board TEXT NOT NULL,           -- 'play' (points) or 'survival' (milliseconds)
	name TEXT NOT NULL,            -- typed by the player: 2 to 12 of A-Z 0-9 space - .
	score INTEGER NOT NULL,
	seconds INTEGER NOT NULL,      -- the run's length
	created_at INTEGER NOT NULL,   -- Unix seconds
	who TEXT NOT NULL,
	version TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS runs_board_time ON runs (board, created_at);
CREATE INDEX IF NOT EXISTS runs_who_time ON runs (who, created_at);
