#!/usr/bin/env bash
# Downloads the SDL2 and SDL2_image sources the Android build compiles, into android/app/jni/SDL and
# android/app/jni/SDL_image, and copies SDL's Java glue (org.libsdl.app) into the app. None of this is kept in
# git (see android/.gitignore): run it once after cloning, or again after changing a version below.
#
#   android/fetch_deps.sh
set -euo pipefail

SDL_VERSION=2.32.10
SDL_IMAGE_VERSION=2.8.12

ANDROID_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JNI_DIR="$ANDROID_DIR/app/jni"
JAVA_DIR="$ANDROID_DIR/app/src/main/java/org/libsdl/app"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

fetch() {  # fetch <github repo> <tag> <archive name> <target dir>
	local url="https://github.com/libsdl-org/$1/releases/download/$2/$3.tar.gz"
	echo "Downloading $url"
	curl -fsSL -o "$TMP/$3.tar.gz" "$url"
	tar xzf "$TMP/$3.tar.gz" -C "$TMP"
	rm -rf "$4"
	mv "$TMP/$3" "$4"
}

fetch SDL "release-$SDL_VERSION" "SDL2-$SDL_VERSION" "$JNI_DIR/SDL"
fetch SDL_image "release-$SDL_IMAGE_VERSION" "SDL2_image-$SDL_IMAGE_VERSION" "$JNI_DIR/SDL_image"

rm -rf "$JAVA_DIR"
mkdir -p "$JAVA_DIR"
cp "$JNI_DIR/SDL/android-project/app/src/main/java/org/libsdl/app/"*.java "$JAVA_DIR/"
echo "SDL $SDL_VERSION and SDL_image $SDL_IMAGE_VERSION ready."
