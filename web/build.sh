#!/usr/bin/env bash
# Builds the website and the web version of the game (Emscripten), and optionally publishes them to GitHub Pages.
#
# Site layout (2026-10-20): the home page, /guide/, /leaderboard/, /policy/, /terms/, /support/ at the root (sources in
# web/site and web/<page>, shared header/footer from web/site/header.html and footer.html); the game in /play/.
#
#   web/build.sh             build into $WEB_BUILD_DIR
#   web/build.sh --deploy    build, then commit the output to the gh-pages branch of the site repository
#                            (web/deploy.conf), push it, and wait until GitHub Pages has published it
#
# Run from Git Bash (Windows) or any bash. Settings (environment variables, defaults in brackets):
#   EMSDK_DIR      Emscripten SDK folder           [/d/Dev/tools/emsdk]
#   WEB_BUILD_DIR  the one output folder           [/d/Dev/web-build]
#   SITE_URL       published site, for the check   [https://injoker.relifes.net]
set -euo pipefail

EMSDK_DIR="${EMSDK_DIR:-/d/Dev/tools/emsdk}"
WEB_BUILD_DIR="${WEB_BUILD_DIR:-/d/Dev/web-build}"
SITE_URL="${SITE_URL:-https://injoker.relifes.net}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Where the site is published (owner 2026-10-03, choice B): a public repository that holds only the built game, so
# the source repository, with the original art files, can be private. web/deploy.conf sets PAGES_REPO (owner/name).
PAGES_REPO="hawonjungo/ThreeElements"
PAGES_BRANCH="gh-pages"
# shellcheck disable=SC1091
[ -f "$REPO_ROOT/web/deploy.conf" ] && source "$REPO_ROOT/web/deploy.conf"
BOARDS_URL="${BOARDS_URL:-}"
EMAILJS_SERVICE="${EMAILJS_SERVICE:-}"; EMAILJS_TEMPLATE="${EMAILJS_TEMPLATE:-}"; EMAILJS_PUBLIC_KEY="${EMAILJS_PUBLIC_KEY:-}"
CF_ANALYTICS_TOKEN="${CF_ANALYTICS_TOKEN:-}"; CF_ANALYTICS_TOKEN="${CF_ANALYTICS_TOKEN%$'\r'}"
EMAILJS_SERVICE="${EMAILJS_SERVICE%$'\r'}"; EMAILJS_TEMPLATE="${EMAILJS_TEMPLATE%$'\r'}"; EMAILJS_PUBLIC_KEY="${EMAILJS_PUBLIC_KEY%$'\r'}"
PAGES_REPO="${PAGES_REPO%$'\r'}"; PAGES_BRANCH="${PAGES_BRANCH%$'\r'}"; BOARDS_URL="${BOARDS_URL%$'\r'}"  # Windows line ends
REPO_API="https://api.github.com/repos/$PAGES_REPO"
SRC_DIR="$REPO_ROOT/Three Elements"

# ---------------------------------------------------------------- build
if ! command -v em++ > /dev/null 2>&1; then
	# shellcheck disable=SC1091
	source "$EMSDK_DIR/emsdk_env.sh" > /dev/null 2>&1
fi
mkdir -p "$WEB_BUILD_DIR"
# start from an empty output folder, so files the site no longer has (the game used to sit at the root) are not
# published again
case "$WEB_BUILD_DIR" in /|"") echo "WEB_BUILD_DIR is not set"; exit 1;; esac
rm -rf "${WEB_BUILD_DIR:?}"/*
mkdir -p "$WEB_BUILD_DIR/play"

echo "Building into $WEB_BUILD_DIR ..."
# the page gets the address of the web leaderboards (spec §33; empty = no world ranking in this build)
SHELL_FILE="$(mktemp -d)/shell.html"
sed "s|%INJOKER_BOARDS_URL%|${BOARDS_URL}|" "$REPO_ROOT/web/shell.html" > "$SHELL_FILE"
cd "$SRC_DIR"
# The browser drives the frame loop (emscripten_set_main_loop_arg in GameManager::LoopGame), so no ASYNCIFY.
# Assets are preloaded into index.data; paths stay the same relative "assets/..." as on desktop.
em++ -O2 -std=c++14 \
	-sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS='["png","bmp"]' \
	-sALLOW_MEMORY_GROWTH=1 \
	--preload-file assets \
	--shell-file "$SHELL_FILE" \
	main.cpp GameManager.cpp MainPlayer.cpp BaseObject.cpp Enemy.cpp Keyboard.cpp Skill.cpp PixelText.cpp \
	ImpTimer.cpp Draw.cpp SkillVfx.cpp Audio.cpp Online.cpp WebBoards.cpp Social.cpp TouchLayout.cpp Core/Invoker.cpp Practice/Practice.cpp Practice/Tutorial.cpp Practice/Boss.cpp Practice/Items.cpp \
	-o "$WEB_BUILD_DIR/play/index.html"
cp "$REPO_ROOT/web/manifest.webmanifest" "$WEB_BUILD_DIR/play/"
cp "$REPO_ROOT/web/privacy.html" "$WEB_BUILD_DIR/"
mkdir -p "$WEB_BUILD_DIR/icons"
cp "$REPO_ROOT/web/icons/"*.png "$WEB_BUILD_DIR/icons/"

# ---------------------------------------------------------------- website
# Every page gets the shared header and footer in place of its <!--#header--> / <!--#footer--> line, and the values
# from web/deploy.conf in place of the %...% marks (the support page's EmailJS ids, the leaderboard server, the
# Cloudflare Web Analytics token; an empty value switches that feature off).
SITE="$REPO_ROOT/web/site"
fill() {
	sed -e "/<!--#header-->/{r $SITE/header.html" -e "d}" -e "/<!--#footer-->/{r $SITE/footer.html" -e "d}" \
		-e "s|%EMAILJS_SERVICE%|${EMAILJS_SERVICE}|g" -e "s|%EMAILJS_TEMPLATE%|${EMAILJS_TEMPLATE}|g" \
		-e "s|%EMAILJS_PUBLIC_KEY%|${EMAILJS_PUBLIC_KEY}|g" -e "s|%INJOKER_BOARDS_URL%|${BOARDS_URL}|g" \
		-e "s|%CF_ANALYTICS_TOKEN%|${CF_ANALYTICS_TOKEN}|g" "$1" > "$2"
}
cp -r "$SITE/media" "$SITE/fonts" "$SITE/site.css" "$SITE/robots.txt" "$SITE/sitemap.xml" "$WEB_BUILD_DIR/"
fill "$SITE/site.js" "$WEB_BUILD_DIR/site.js"
fill "$SITE/index.html" "$WEB_BUILD_DIR/index.html"
for page in guide leaderboard; do
	mkdir -p "$WEB_BUILD_DIR/$page"
	fill "$SITE/$page/index.html" "$WEB_BUILD_DIR/$page/index.html"
done
for page in policy terms support; do
	mkdir -p "$WEB_BUILD_DIR/$page"
	fill "$REPO_ROOT/web/$page/index.html" "$WEB_BUILD_DIR/$page/index.html"
done
if grep -rl -e '<!--#header-->' -e '<!--#footer-->' "$WEB_BUILD_DIR" --include=*.html; then
	echo "A page still has a header/footer mark (see above)"; exit 1
fi
echo "Build OK: $(ls "$WEB_BUILD_DIR" | tr '\n' ' ')"

[ "${1:-}" = "--deploy" ] || exit 0

# ---------------------------------------------------------------- deploy
# The site branch holds only the published files (plus CNAME, .nojekyll, README.md, which are kept as they are).
# It is cloned into a temporary folder, so the source repository's branches and working tree are never touched.
TMP="$(mktemp -d)"
WT="$TMP/site"
trap 'rm -rf "$TMP"' EXIT
SITE_GIT="https://github.com/$PAGES_REPO.git"
if git clone -q --depth 1 --branch "$PAGES_BRANCH" "$SITE_GIT" "$WT" 2> /dev/null; then
	:
else
	# a new, empty site repository: start its branch with the custom domain and no Jekyll
	git clone -q "$SITE_GIT" "$WT"
	git -C "$WT" checkout -q --orphan "$PAGES_BRANCH"
	echo "${SITE_URL#https://}" > "$WT/CNAME"
	touch "$WT/.nojekyll"
	echo "Injoker - the published web build (https://injoker.relifes.net). The source is private." > "$WT/README.md"
fi

# the branch mirrors the build folder: everything but CNAME, .nojekyll and README.md is replaced
find "$WT" -mindepth 1 -maxdepth 1 ! -name .git ! -name CNAME ! -name .nojekyll ! -name README.md -exec rm -rf {} +
cp -r "$WEB_BUILD_DIR"/. "$WT/"
SRC_SHA="$(git rev-parse --short HEAD)"
git -C "$WT" add -A
if git -C "$WT" diff --cached --quiet; then
	# Nothing changed, but still push a commit: it also re-triggers a Pages build that GitHub skipped.
	git -C "$WT" commit -q --allow-empty -m "Redeploy from $SRC_SHA (no file changes)"
else
	git -C "$WT" commit -q -m "Rebuild from $SRC_SHA"
fi
DEPLOY_SHA="$(git -C "$WT" rev-parse HEAD)"
git -C "$WT" push -q origin "$PAGES_BRANCH"
echo "Pushed $PAGES_REPO $PAGES_BRANCH ${DEPLOY_SHA:0:7}; waiting for GitHub Pages ..."

# GitHub sometimes creates no Pages run for a push; wait for the run of exactly this commit.
for _ in $(seq 1 30); do
	STATE="$(curl -sS "$REPO_API/actions/runs?per_page=5&branch=$PAGES_BRANCH" | python -c "
import json, sys
runs = json.load(sys.stdin).get('workflow_runs', [])
run = next((r for r in runs if r['head_sha'] == '$DEPLOY_SHA'), None)
print('none' if run is None else '%s/%s' % (run['status'], run['conclusion']))" || echo error)"
	if [ "$STATE" = "completed/failure" ]; then
		echo "GitHub Pages build FAILED for ${DEPLOY_SHA:0:7}; see the repository's Actions tab."
		exit 1
	fi
	if [ "$STATE" = "completed/success" ]; then
		LIVE="$(curl -sS "$SITE_URL/play/index.wasm?nocache=$RANDOM" | md5sum | cut -d' ' -f1)"
		LOCAL="$(md5sum < "$WEB_BUILD_DIR/play/index.wasm" | cut -d' ' -f1)"
		LIVE_HOME="$(curl -sS "$SITE_URL/?nocache=$RANDOM" | md5sum | cut -d' ' -f1)"
		LOCAL_HOME="$(md5sum < "$WEB_BUILD_DIR/index.html" | cut -d' ' -f1)"
		if [ "$LIVE" = "$LOCAL" ] && [ "$LIVE_HOME" = "$LOCAL_HOME" ]; then
			echo "Published and verified: $SITE_URL (browsers may cache up to 10 min; hard-refresh)"
		else
			echo "Pages build succeeded, but the CDN still serves an older home page or game; check again in a minute."
		fi
		exit 0
	fi
	sleep 10
done
echo "No successful Pages run for ${DEPLOY_SHA:0:7} after 5 minutes."
echo "If none was created, run 'web/build.sh --deploy' again (it pushes a fresh commit)."
exit 1
