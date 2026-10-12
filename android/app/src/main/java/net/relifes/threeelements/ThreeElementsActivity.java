package net.relifes.threeelements;

import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;
import com.google.android.play.core.review.ReviewInfo;
import com.google.android.play.core.review.ReviewManager;
import com.google.android.play.core.review.ReviewManagerFactory;

import org.libsdl.app.SDLActivity;

/**
 * The game's Android entry point. SDLActivity (SDL's own Java glue, copied in by android/fetch_deps.sh) creates the
 * surface, forwards touch / keys / audio, and runs SDL_main() from libmain.so (the C++ game) on its own thread.
 * This subclass names the native libraries to load, in dependency order, and gives the C++ side (Online.cpp) its
 * four calls for the online leaderboards. What they do is in OnlineBoards, which exists twice: with Google Play
 * Games (src/online) when android/play-games.properties is there, and as an empty stand-in (src/offline) otherwise.
 * Since 1.9.16 also the calls of Social.cpp: rate (the Play page), Google's in-app review card, and share.
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

    // ---- called from C++ (Social.cpp), 1.9.16

    // RATE in SETTINGS: the game's page in the Play Store app, or in the browser when there is no Play Store
    public void socialRate() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                String id = getPackageName();
                try {
                    startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("market://details?id=" + id)));
                } catch (ActivityNotFoundException e) {
                    try {
                        startActivity(new Intent(Intent.ACTION_VIEW,
                                Uri.parse("https://play.google.com/store/apps/details?id=" + id)));
                    } catch (ActivityNotFoundException ignored) {
                    }
                }
            }
        });
    }

    // Google Play's in-app review card over the game. Google limits how often it shows, and shows nothing for an
    // app not installed from Play: the game never knows whether it appeared, or what the player did.
    public void socialReview() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                final ReviewManager manager = ReviewManagerFactory.create(ThreeElementsActivity.this);
                manager.requestReviewFlow().addOnCompleteListener(new OnCompleteListener<ReviewInfo>() {
                    @Override
                    public void onComplete(Task<ReviewInfo> task) {
                        if (task.isSuccessful() && !isFinishing())
                            manager.launchReviewFlow(ThreeElementsActivity.this, task.getResult());
                    }
                });
            }
        });
    }

    // SHARE: the system's share sheet with the text (it holds the links)
    public void socialShare(final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Intent send = new Intent(Intent.ACTION_SEND);
                send.setType("text/plain");
                send.putExtra(Intent.EXTRA_TEXT, text);
                try {
                    startActivity(Intent.createChooser(send, "Share Injoker"));
                } catch (ActivityNotFoundException ignored) {
                }
            }
        });
    }
}
