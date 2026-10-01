package net.relifes.threeelements;

import android.app.Activity;
import android.content.Context;

/**
 * The stand-in used when the build has no Google Play Games (no android/play-games.properties): no online
 * leaderboards, no library, no internet permission. The real one is in src/online.
 */
final class OnlineBoards {
    private OnlineBoards() {}

    static void init(Context app) {}

    static void check(Activity activity) {}

    static boolean available() {
        return false;
    }

    static boolean signedIn() {
        return false;
    }

    static void submit(Activity activity, int board, long score) {}

    static void show(Activity activity) {}
}
