package net.relifes.threeelements;

import android.app.Activity;
import android.os.Bundle;

/**
 * An invisible activity that closes itself at once. Starting it in a new task brings forward the other task of this
 * app that is still open - Google's sign-in screens, which the player left unfinished (see OnlineBoards.show). The
 * game itself is a singleInstance activity, so that sign-in lives in a task of its own and cannot be reached any
 * other way without asking for a permission.
 */
public class TaskResumeActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        finish();
    }
}
