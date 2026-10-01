package net.relifes.threeelements;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.widget.Toast;

import com.google.android.gms.games.AuthenticationResult;
import com.google.android.gms.games.PlayGames;
import com.google.android.gms.games.PlayGamesSdk;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.android.gms.tasks.Task;

/**
 * Online leaderboards with Google Play Games Services v2 (GAMEPLAY_SPEC.md §29). Used when
 * android/play-games.properties exists; src/offline holds the empty stand-in.
 *
 * Signing in is never required: Play Games signs a player with a profile in by itself when the game starts, and
 * the game only asks when the player opens the global boards. The player's name and avatar are shown by Google's
 * own leaderboard screen; nothing of them is read or kept here. All methods run on the UI thread.
 */
final class OnlineBoards {
    private static final int BOARDS = 2;              // 0 = PLAY (score), 1 = SURVIVAL (milliseconds)
    private static final int REQUEST_BOARDS = 9004;   // any number: the result is not used

    private static volatile boolean signedIn = false;
    private static boolean signInAsked = false;       // show() started Google's sign-in and it has not answered yet
    // the best result of this session that could not be sent yet (not signed in): sent as soon as the player signs in
    private static final long[] waiting = new long[BOARDS];

    private OnlineBoards() {}

    static void init(Context app) {
        PlayGamesSdk.initialize(app);
    }

    static boolean available() {
        return true;
    }

    static boolean signedIn() {
        return signedIn;
    }

    /** Asks Play Games whether the player is signed in (it signs in by itself at start-up when it can). */
    static void check(final Activity activity) {
        PlayGames.getGamesSignInClient(activity).isAuthenticated()
            .addOnCompleteListener(new OnCompleteListener<AuthenticationResult>() {
                @Override
                public void onComplete(Task<AuthenticationResult> task) {
                    signedIn = task.isSuccessful() && task.getResult().isAuthenticated();
                    if (signedIn)
                        sendWaiting(activity);
                }
            });
    }

    static void submit(Activity activity, int board, long score) {
        if (board < 0 || board >= BOARDS || score <= 0)
            return;
        if (!signedIn) {
            if (score > waiting[board])
                waiting[board] = score;
            return;
        }
        // Play Games keeps the score and sends it later by itself when the phone is offline
        PlayGames.getLeaderboardsClient(activity).submitScore(boardId(activity, board), score);
    }

    /** Google's leaderboard screen (both boards; day / week / all time); signs in first when needed. */
    static void show(final Activity activity) {
        if (signedIn) {
            open(activity);
            return;
        }
        if (signInAsked) {
            // The player left Google's sign-in unfinished (Home, then back to the game). The game is a singleInstance
            // activity (SDL's template), so that sign-in is still open in a task of its own, and Play Games ignores a
            // new request while it waits: bring that task back, where the player can finish it or go Back out of it.
            Intent back = new Intent(activity, TaskResumeActivity.class);
            back.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity.startActivity(back);
            return;
        }
        signInAsked = true;
        PlayGames.getGamesSignInClient(activity).signIn()
            .addOnCompleteListener(new OnCompleteListener<AuthenticationResult>() {
                @Override
                public void onComplete(Task<AuthenticationResult> task) {
                    signInAsked = false;
                    signedIn = task.isSuccessful() && task.getResult().isAuthenticated();
                    if (signedIn) {
                        sendWaiting(activity);
                        open(activity);
                    } else {
                        Toast.makeText(activity, "Could not sign in to Google Play Games", Toast.LENGTH_SHORT).show();
                    }
                }
            });
    }

    private static void open(final Activity activity) {
        PlayGames.getLeaderboardsClient(activity).getAllLeaderboardsIntent()
            .addOnSuccessListener(new OnSuccessListener<Intent>() {
                @Override
                public void onSuccess(Intent intent) {
                    activity.startActivityForResult(intent, REQUEST_BOARDS);
                }
            })
            .addOnFailureListener(new OnFailureListener() {
                @Override
                public void onFailure(Exception e) {
                    Toast.makeText(activity, "The leaderboards are not available right now", Toast.LENGTH_SHORT).show();
                }
            });
    }

    private static void sendWaiting(Activity activity) {
        for (int board = 0; board < BOARDS; ++board) {
            if (waiting[board] > 0) {
                PlayGames.getLeaderboardsClient(activity).submitScore(boardId(activity, board), waiting[board]);
                waiting[board] = 0;
            }
        }
    }

    private static String boardId(Activity activity, int board) {
        return activity.getString(board == 0 ? R.string.leaderboard_play : R.string.leaderboard_survival);
    }
}
