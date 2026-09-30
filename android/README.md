# Injoker — Android (native SDL2)

App id `net.relifes.injoker` (permanent once on Google Play). Launcher icon: `art/make_injoker.py`.

The same C++ game as the desktop and web builds, compiled for Android with the NDK. SDL's own Java glue
(`SDLActivity`) runs it; `ThreeElementsActivity` only names the native libraries.

## One-time setup

1. **Android Studio** with the Android SDK (platform 35), plus from *SDK Manager → SDK Tools*: **NDK (Side by side)**
   and **CMake**. The build picks the newest installed NDK and CMake by itself (`app/build.gradle`); if they are
   missing, the Android Gradle Plugin downloads them on the first build.
2. `android/local.properties` pointing at the SDK (Android Studio writes it when you open `android/`; by hand:
   `sdk.dir=C\:\\Users\\<you>\\AppData\\Local\\Android\\Sdk`). Not in git.
3. SDL sources: from Git Bash, `android/fetch_deps.sh` (downloads SDL2 2.32.10 and SDL2_image 2.8.12 into
   `app/jni/`, copies SDL's Java files into `app/src/main/java/org/libsdl/`). Not in git; run again after
   changing the versions in the script.

## Build and install

From Git Bash in `android/` (Java: the JDK that ships with Android Studio):

```bash
export JAVA_HOME="/c/Program Files/Android/Android Studio/jbr"
./gradlew assembleDebug          # -> app/build/outputs/apk/debug/app-debug.apk
./gradlew installDebug           # installs on the phone connected with USB debugging on
```

Or open the `android/` folder in Android Studio and press Run.

On the phone: *Settings → About phone → tap Build number 7 times*, then *Developer options → USB debugging* on,
connect the cable and accept the prompt. `adb devices` (in the SDK's `platform-tools`) should list it.

## Checking other screen shapes (emulator)

The APK also contains `x86_64`, so it runs in the Android Studio emulator (AVD `Medium_Phone_API_36.0`, 1080×2400).
Other shapes without other phones: `adb shell wm size 1080x1920` (16:9), `adb shell wm size 1536x2048` (4:3 tablet),
then relaunch the app; `adb shell wm size reset` afterwards. `adb exec-out screencap -p > shot.png` takes a
screenshot, `adb shell input tap X Y` taps.

Two traps found this way (2026-09-30), both invisible on desktop / web where the window is exactly 928 × 544:
- `SDL_RenderSetViewport(renderer, NULL)` resets the viewport to the whole screen and **drops SDL's letterbox
  offset**: the game jumped to the left edge. Save the viewport with `SDL_RenderGetViewport` and restore that.
- With a logical size set, SDL **already** rewrites `SDL_FINGER*` positions to 0..1 of the letterboxed game area.
  Multiply by 928 / 544 and nothing else; converting through the viewport again moved every touch.

## What is Android-specific in the C++ code

All in `GameManager.cpp`, behind `#ifdef __ANDROID__` where it differs:

- Landscape only (`SDL_HINT_ORIENTATIONS`, and `sensorLandscape` in the manifest), fullscreen window.
- The game draws into a 928 × 544 logical area (`SDL_RenderSetLogicalSize`, used on every platform), letterboxed on
  the phone; `TouchToGame` maps finger positions through that letterbox.
- The on-screen Q/W/E/R/D/F buttons are shown from the start.
- The Back button acts as Esc (Ready → quit, otherwise back to Ready).
- Save files (records, top 10, sound setting) go to the app's private folder (`SDL_GetPrefPath`), and the records
  reached so far are written when the app goes to the background.
- Images are packed into the APK under `assets/` (copied from `Three Elements/assets` at build time), so the code
  opens them with the same relative paths as everywhere else.
