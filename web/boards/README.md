# Web leaderboards (spec §33)

The WORLD RANKING of the web version: a Cloudflare Worker (`worker.js`) with a D1 database (`schema.sql`). Two
boards, PLAY (points) and SURVIVAL (milliseconds), each shown for **this week** (from Monday 00:00 UTC) and **all
time**, ten best names. Players type their own name (2 to 12 of A-Z 0-9 space - .). The Android version keeps its
Google Play Games boards (§29); these are separate.

Protection is basic (owner 2026-10-03): names are cleaned and filtered (`rules.js`), a score that is too high for the
length of its run is refused, one address may send at most 30 runs an hour, and only the published site may send.
Anyone who reads the page's code can still send a fake score that looks plausible; a stronger check (the server
replaying the run from its seed and keys) can be added later without changing the boards.

## Test the rules

    node web/boards/test.mjs

## Run it locally (no Cloudflare account needed)

    cd web/boards
    npx wrangler d1 execute injoker-boards --local --file schema.sql
    npx wrangler dev --local --port 8787
    # in another shell: a web build that talks to it
    BOARDS_URL=http://localhost:8787 WEB_BUILD_DIR=<a temporary folder> bash web/build.sh
    cd <that folder> && python -m http.server 8000      # open http://localhost:8000

## Put it online (once)

1. Create a free Cloudflare account (the owner).
2. `cd web/boards && npx wrangler login` - a browser page asks to allow access: the owner clicks Allow.
3. `npx wrangler d1 create injoker-boards` - copy the `database_id` it prints into `wrangler.toml`.
4. `npx wrangler d1 execute injoker-boards --remote --file schema.sql`
5. `npx wrangler secret put SALT` - type any long random text (it makes the stored address hashes unguessable).
6. `npx wrangler deploy` - it prints the address, `https://injoker-boards.<account>.workers.dev`.
7. Put that address in `web/deploy.conf` (`BOARDS_URL`), then `web/build.sh --deploy`. The LEADERBOARD screen of
   the web version now has a WORLD RANKING button.

## Data

Each run sent: board, the typed name, score, the run's length, the time it was sent, and a salted hash of the sender's
address (only to limit how often one player can send). No account, no e-mail, no device id. To remove a name:
`npx wrangler d1 execute injoker-boards --remote --command "DELETE FROM runs WHERE name = 'THE NAME'"`.
