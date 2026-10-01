package net.relifes.threeelements;

import android.os.Bundle;

import org.libsdl.app.SDLActivity;

/**
 * The game's Android entry point. SDLActivity (SDL's own Java glue, copied in by android/fetch_deps.sh) creates the
 * surface, forwards touch / keys / audio, and runs SDL_main() from libmain.so (the C++ game) on its own thread.
 * This subclass names the native libraries to load, in dependency order, and gives the C++ side (Online.cpp) its
 * four calls for the online leaderboards. What they do is in OnlineBoards, which exists twice: with Google Play
 * Games (src/online) when android/play-games.properties is there, and as an empty stand-in (src/offline) otherwise.
 */
public class ThreeElementsActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "SDL2_image", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        OnlineBoards.check(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        OnlineBoards.check(this);  // the player may have signed in or out while the game was in the background
    }

    // ---- called from C++ (Online.cpp) on SDL's thread; anything that touches Play Games runs on the UI thread

    public boolean onlineAvailable() {
        return OnlineBoards.available();
    }

    public boolean onlineSignedIn() {
        return OnlineBoards.signedIn();
    }

    public void onlineSubmit(final int board, final long score) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                OnlineBoards.submit(ThreeElementsActivity.this, board, score);
            }
        });
    }

    public void onlineShow() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                OnlineBoards.show(ThreeElementsActivity.this);
            }
        });
    }
}
