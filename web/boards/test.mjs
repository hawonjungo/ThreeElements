// node web/boards/test.mjs  - checks the rules of the web leaderboards. Exit code 0 = all passed.
import { cleanName, checkScore, weekStart } from './rules.js';

let checks = 0, failed = 0;
function check(ok, what) {
	++checks;
	if (!ok) { ++failed; console.log('FAIL: ' + what); }
}

// names
check(cleanName('jungo') === 'JUNGO', 'capitals');
check(cleanName('  Ju  Ngo ') === 'JU NGO', 'spaces squeezed');
check(cleanName('a') === null, 'too short');
check(cleanName('ABCDEFGHIJKLM') === null, 'too long');
check(cleanName('Invoker_99!') === 'INVOKER99', 'other characters dropped');
check(cleanName('f.u.c.k') === null, 'blocked word through dots');
check(cleanName('--') === null, 'no letters');
check(cleanName(42) === null, 'not a string');
check(cleanName('Mage-7') === 'MAGE-7', 'dash kept');

// scores
check(checkScore('play', 85, 300) === null, 'a normal PLAY run');
check(checkScore('play', 5000, 300) !== null, 'too many points for the time');
check(checkScore('play', 0, 300) !== null, 'no points');
check(checkScore('play', 12.5, 300) !== null, 'not a whole number');
check(checkScore('survival', 125000, 125.2) === null, 'survival time matches');
check(checkScore('survival', 125000, 60) !== null, 'survival time does not match the run');
check(checkScore('arena', 10, 10) !== null, 'unknown board');
check(checkScore('play', 10, 0.5) !== null, 'too short a run');

// the week starts on Monday 00:00 UTC
const thu = Date.UTC(2026, 9, 1, 15, 0, 0) / 1000;   // Thursday 2026-10-01
const mon = Date.UTC(2026, 8, 28, 0, 0, 0) / 1000;   // Monday 2026-09-28
check(weekStart(thu) === mon, 'week of a Thursday');
check(weekStart(mon) === mon, 'a Monday is its own week');
check(weekStart(mon - 1) === mon - 7 * 86400, 'Sunday night belongs to the week before');

console.log(`${checks} checks, ${failed} failed`);
process.exit(failed === 0 ? 0 : 1);
