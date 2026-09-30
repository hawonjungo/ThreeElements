#!/usr/bin/env bash
# Builds the web version (Emscripten) and optionally publishes it to GitHub Pages.
#
#   web/build.sh             build into $WEB_BUILD_DIR
#   web/build.sh --deploy    build, then commit the output to the gh-pages branch, push it,
#                            and wait until GitHub Pages has published it
#
# Run from Git Bash (Windows) or any bash. Settings (environment variables, defaults in brackets):
#   EMSDK_DIR      Emscripten SDK folder           [/d/Dev/tools/emsdk]
#   WEB_BUILD_DIR  the one output folder           [/d/Dev/web-build]
#   SITE_URL       published site, for the check   [https://injoker.relifes.net]
set -euo pipefail

EMSDK_DIR="${EMSDK_DIR:-/d/Dev/tools/emsdk}"
WEB_BUILD_DIR="${WEB_BUILD_DIR:-/d/Dev/web-build}"
SITE_URL="${SITE_URL:-https://injoker.relifes.net}"
REPO_API="https://api.github.com/repos/hawonjungo/ThreeElements"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="$REPO_ROOT/Three Elements"

# ---------------------------------------------------------------- build
if ! command -v em++ > /dev/null 2>&1; then
	# shellcheck disable=SC1091
	source "$EMSDK_DIR/emsdk_env.sh" > /dev/null 2>&1
fi
mkdir -p "$WEB_BUILD_DIR"

echo "Building into $WEB_BUILD_DIR ..."
cd "$SRC_DIR"
# The browser drives the frame loop (emscripten_set_main_loop_arg in GameManager::LoopGame), so no ASYNCIFY.
# Assets are preloaded into index.data; paths stay the same relative "assets/..." as on desktop.
em++ -O2 -std=c++14 \
	-sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS='["png","bmp"]' \
	-sALLOW_MEMORY_GROWTH=1 \
	--preload-file assets \
	--shell-file "$REPO_ROOT/web/shell.html" \
	main.cpp GameManager.cpp MainPlayer.cpp BaseObject.cpp Enemy.cpp Keyboard.cpp Skill.cpp PixelText.cpp \
	ImpTimer.cpp Draw.cpp SkillVfx.cpp Audio.cpp Core/Invoker.cpp Practice/Practice.cpp Practice/Tutorial.cpp Practice/Boss.cpp \
	-o "$WEB_BUILD_DIR/index.html"
cp "$REPO_ROOT/web/manifest.webmanifest" "$REPO_ROOT/web/privacy.html" "$WEB_BUILD_DIR/"
mkdir -p "$WEB_BUILD_DIR/icons"
cp "$REPO_ROOT/web/icons/"*.png "$WEB_BUILD_DIR/icons/"
# static pages: privacy policy, terms, support (served as /policy/, /terms/, /support/)
for page in policy terms support; do
	mkdir -p "$WEB_BUILD_DIR/$page"
	cp "$REPO_ROOT/web/$page/index.html" "$WEB_BUILD_DIR/$page/"
done
echo "Build OK: $(ls "$WEB_BUILD_DIR" | tr '\n' ' ')"

[ "${1:-}" = "--deploy" ] || exit 0

# ---------------------------------------------------------------- deploy
# gh-pages holds only the published files (plus CNAME, .nojekyll, README.md, which are kept as they are).
# A temporary worktree is used so the current branch and working tree are never touched.
cd "$REPO_ROOT"
WT="$(mktemp -d)/gh-pages"
git fetch -q origin gh-pages
git worktree add -q "$WT" gh-pages
trap 'git -C "$REPO_ROOT" worktree remove --force "$WT" 2> /dev/null || true' EXIT
git -C "$WT" merge -q --ff-only origin/gh-pages

cp "$WEB_BUILD_DIR"/{index.html,index.js,index.wasm,index.data,manifest.webmanifest,privacy.html} "$WT/"
mkdir -p "$WT/icons"
cp "$WEB_BUILD_DIR/icons/"*.png "$WT/icons/"
for page in policy terms support; do
	mkdir -p "$WT/$page"
	cp "$WEB_BUILD_DIR/$page/index.html" "$WT/$page/"
done
SRC_SHA="$(git rev-parse --short HEAD)"
git -C "$WT" add -A
if git -C "$WT" diff --cached --quiet; then
	# Nothing changed, but still push a commit: it also re-triggers a Pages build that GitHub skipped.
	git -C "$WT" commit -q --allow-empty -m "Redeploy from $SRC_SHA (no file changes)"
else
	git -C "$WT" commit -q -m "Rebuild from $SRC_SHA"
fi
DEPLOY_SHA="$(git -C "$WT" rev-parse HEAD)"
git -C "$WT" push -q origin gh-pages
echo "Pushed gh-pages ${DEPLOY_SHA:0:7}; waiting for GitHub Pages ..."

# GitHub sometimes creates no Pages run for a push; wait for the run of exactly this commit.
for _ in $(seq 1 30); do
	STATE="$(curl -sS "$REPO_API/actions/runs?per_page=5&branch=gh-pages" | python -c "
import json, sys
runs = json.load(sys.stdin).get('workflow_runs', [])
run = next((r for r in runs if r['head_sha'] == '$DEPLOY_SHA'), None)
print('none' if run is None else '%s/%s' % (run['status'], run['conclusion']))" || echo error)"
	if [ "$STATE" = "completed/failure" ]; then
		echo "GitHub Pages build FAILED for ${DEPLOY_SHA:0:7}; see the repository's Actions tab."
		exit 1
	fi
	if [ "$STATE" = "completed/success" ]; then
		LIVE="$(curl -sS "$SITE_URL/index.wasm?nocache=$RANDOM" | md5sum | cut -d' ' -f1)"
		LOCAL="$(md5sum < "$WEB_BUILD_DIR/index.wasm" | cut -d' ' -f1)"
		if [ "$LIVE" = "$LOCAL" ]; then
			echo "Published and verified: $SITE_URL (browsers may cache up to 10 min; hard-refresh)"
		else
			echo "Pages build succeeded, but the CDN still serves an older index.wasm; check again in a minute."
		fi
		exit 0
	fi
	sleep 10
done
echo "No successful Pages run for ${DEPLOY_SHA:0:7} after 5 minutes."
echo "If none was created, run 'web/build.sh --deploy' again (it pushes a fresh commit)."
exit 1
