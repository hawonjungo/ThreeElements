# Three Elements — Android (native SDL2)

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
