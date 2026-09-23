# gh-pages

Auto-generated. This branch holds only the static Web/WebAssembly build of
**Three Elements** (Emscripten output), published via GitHub Pages.

- Source: `main` repository, branch `phase-2-invoker-core`, commit `bfad03a`
  ("fix: raise/enlarge touch cluster, request real fullscreen on mobile web").
- Built with Emscripten 6.0.10 (`-sASYNCIFY`, SDL2 + SDL2_image ports),
  custom shell (`web/shell.html`) with a landscape-mobile fullscreen layout;
  the first tap on the canvas on a phone also requests real browser
  fullscreen (not supported by iOS Safari, a browser limitation).
- Keyboard (Q/W/E/R/D/F, Enter, Esc) and touch (six on-screen buttons,
  bottom-left, keyboard-style layout) both work; Top-10 high scores are
  saved to the browser's localStorage.
- Do not edit files here by hand — they are overwritten by the next build.
  Rebuild from the source tree and re-push this branch instead.
