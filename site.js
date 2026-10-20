// Injoker website: the small bits of behaviour every page shares (menu, Android button, trailer, screenshots,
// leaderboards). Plain script, no libraries. web/build.sh fills in the %...% values from web/deploy.conf.
(function () {
  'use strict';

  // Google Play: empty until the app is public; then the store address, and every Android button becomes a link.
  var ANDROID_URL = '';
  var BOARDS_URL = 'https://injoker-boards.injoker.workers.dev';
  var CF_ANALYTICS_TOKEN = '';
  var YOUTUBE_ID = 'kW-dQLGD_e0';

  var filled = function (value) { return value && value.indexOf('%') < 0; };
  var $ = function (sel, root) { return (root || document).querySelector(sel); };
  var $$ = function (sel, root) { return Array.prototype.slice.call((root || document).querySelectorAll(sel)); };

  // A home-screen install made before the game moved to /play/ still opens "/": send it straight to the game.
  if (location.pathname === '/' && window.matchMedia('(display-mode: fullscreen), (display-mode: standalone)').matches) {
    location.replace('/play/');
    return;
  }

  // ------------------------------------------------------------ mobile menu
  var toggle = $('.menu-toggle'), nav = $('#site-nav');
  if (toggle && nav) {
    toggle.addEventListener('click', function () {
      var open = nav.classList.toggle('open');
      toggle.setAttribute('aria-expanded', open ? 'true' : 'false');
    });
    $$('a', nav).forEach(function (a) { a.addEventListener('click', function () { nav.classList.remove('open'); }); });
  }

  // ------------------------------------------------------------ Android buttons and the platform hint
  $$('[data-android]').forEach(function (el) {
    if (!filled(ANDROID_URL)) return;
    var link = document.createElement('a');
    link.className = el.className.replace('is-soon', '').trim();
    link.href = ANDROID_URL;
    link.target = '_blank';
    link.rel = 'noopener';
    link.innerHTML = el.innerHTML.replace(/<span class="soon-tag">[^<]*<\/span>/, '').replace('Coming soon to', 'Get it on');
    el.parentNode.replaceChild(link, el);
  });
  var ua = navigator.userAgent;
  var isAndroid = /Android/i.test(ua);
  var isIOS = /iPhone|iPad|iPod/i.test(ua) || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);
  var note = $('#platform-note');
  if (note) {
    if (isIOS)
      note.textContent = 'On iPhone and iPad: play in Safari, then Share → Add to Home Screen for full screen.';
    else if (isAndroid && !filled(ANDROID_URL))
      note.textContent = 'The Android app is coming soon. Until then, Injoker plays right here in your browser.';
    else if (!isAndroid)
      note.textContent = 'Plays in your browser with a keyboard. No download, no account.';
  }
  // on an Android phone with the app out, the store button goes first
  if (isAndroid && filled(ANDROID_URL)) {
    $$('.cta-row').forEach(function (row) {
      var store = $('[href="' + ANDROID_URL + '"]', row);
      if (store) row.insertBefore(store, row.firstChild);
    });
  }

  // ------------------------------------------------------------ trailer: YouTube only loads when asked for
  var trailer = $('#trailer-player');
  if (trailer) {
    trailer.addEventListener('click', function () {
      var frame = document.createElement('iframe');
      frame.src = 'https://www.youtube-nocookie.com/embed/' + YOUTUBE_ID + '?autoplay=1&rel=0';
      frame.title = 'Injoker trailer';
      frame.allow = 'autoplay; encrypted-media; picture-in-picture; fullscreen';
      frame.allowFullscreen = true;
      trailer.innerHTML = '';
      trailer.appendChild(frame);
      trailer.style.cursor = 'default';
    }, { once: true });
  }

  // ------------------------------------------------------------ screenshots
  var box = $('#lightbox');
  if (box && box.showModal) {
    var big = $('img', box);
    $$('[data-shot]').forEach(function (btn) {
      btn.addEventListener('click', function () {
        big.src = btn.getAttribute('data-shot');
        big.alt = $('img', btn).alt;
        box.showModal();
      });
    });
    box.addEventListener('click', function () { box.close(); });
  }

  // ------------------------------------------------------------ leaderboards (the web version's world ranking)
  var pad = function (n) { return (n < 10 ? '0' : '') + n; };
  var formatValue = function (board, score) {
    if (board === 'play') return score.toLocaleString('en-US');
    var s = Math.floor(score / 1000);
    return pad(Math.floor(s / 60)) + ':' + pad(s % 60);
  };
  var formatDate = function (at) {
    return new Date(at * 1000).toLocaleDateString('en-GB', { day: 'numeric', month: 'short', year: 'numeric' });
  };
  var fetchBoard = function (board, period) {
    if (!filled(BOARDS_URL)) return Promise.reject(new Error('no server'));
    return fetch(BOARDS_URL + '/top?board=' + board + '&period=' + period)
      .then(function (r) { return r.ok ? r.json() : Promise.reject(new Error(r.status)); })
      .then(function (data) { return data.entries || []; });
  };
  var renderBoard = function (table, board, entries, limit) {
    var body = $('tbody', table);
    body.innerHTML = '';
    entries.slice(0, limit).forEach(function (e, i) {
      var tr = document.createElement('tr');
      [['rank', '#' + (i + 1)], ['who', e.name], ['val', formatValue(board, e.score)], ['when', formatDate(e.at)]]
        .forEach(function (cell) {
          var td = document.createElement('td');
          td.className = cell[0];
          td.textContent = cell[1];
          tr.appendChild(td);
        });
      body.appendChild(tr);
    });
  };

  // home page: the five best PLAY scores of the week, or of all time while the week is still empty
  var teaser = $('#top-board');
  if (teaser) {
    var label = $('#top-board-period'), empty = $('#top-board-empty');
    fetchBoard('play', 'week').then(function (week) {
      if (week.length > 0) return { entries: week, period: 'This week' };
      return fetchBoard('play', 'all').then(function (all) { return { entries: all, period: 'All time' }; });
    }).then(function (res) {
      label.textContent = res.period;
      if (res.entries.length === 0) { empty.hidden = false; return; }
      renderBoard(teaser, 'play', res.entries, 5);
      teaser.hidden = false;
    }).catch(function () {
      empty.textContent = 'The ranking could not be loaded right now.';
      empty.hidden = false;
    });
  }

  // leaderboard page: two boards, two periods
  var full = $('#full-board');
  if (full) {
    var state = { board: 'play', period: 'week' };
    var fullEmpty = $('#full-board-empty'), head = $('#full-board-value');
    var load = function () {
      $$('[data-board]').forEach(function (b) { b.setAttribute('aria-pressed', b.getAttribute('data-board') === state.board ? 'true' : 'false'); });
      $$('[data-period]').forEach(function (b) { b.setAttribute('aria-pressed', b.getAttribute('data-period') === state.period ? 'true' : 'false'); });
      head.textContent = state.board === 'play' ? 'Score' : 'Time';
      fullEmpty.hidden = true;
      fullEmpty.textContent = state.period === 'week' ? 'No runs this week yet. The week starts on Monday (UTC).' : 'No runs yet.';
      var asked = state.board + state.period;
      fetchBoard(state.board, state.period).then(function (entries) {
        if (asked !== state.board + state.period) return;
        renderBoard(full, state.board, entries, 10);
        full.hidden = entries.length === 0;
        fullEmpty.hidden = entries.length > 0;
      }).catch(function () {
        full.hidden = true;
        fullEmpty.textContent = 'The ranking could not be loaded right now. Please try again later.';
        fullEmpty.hidden = false;
      });
    };
    $$('[data-board]').forEach(function (b) { b.addEventListener('click', function () { state.board = b.getAttribute('data-board'); load(); }); });
    $$('[data-period]').forEach(function (b) { b.addEventListener('click', function () { state.period = b.getAttribute('data-period'); load(); }); });
    load();
  }

  // ------------------------------------------------------------ Cloudflare Web Analytics (no cookies), when set up
  if (filled(CF_ANALYTICS_TOKEN)) {
    var beacon = document.createElement('script');
    beacon.defer = true;
    beacon.src = 'https://static.cloudflareinsights.com/beacon.min.js';
    beacon.setAttribute('data-cf-beacon', JSON.stringify({ token: CF_ANALYTICS_TOKEN }));
    document.body.appendChild(beacon);
  }
})();
