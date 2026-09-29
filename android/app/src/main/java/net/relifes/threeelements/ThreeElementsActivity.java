package net.relifes.threeelements;

import org.libsdl.app.SDLActivity;

/**
 * The game's Android entry point. SDLActivity (SDL's own Java glue, copied in by android/fetch_deps.sh) creates the
 * surface, forwards touch / keys / audio, and runs SDL_main() from libmain.so (the C++ game) on its own thread.
 * This subclass only names the native libraries to load, in dependency order.
 */
public class ThreeElementsActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "SDL2_image", "main" };
    }
}
