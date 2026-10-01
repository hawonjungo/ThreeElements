package net.relifes.threeelements;

import android.app.Application;

/** Only here because Play Games has to be initialised before any activity starts (a no-op without Play Games). */
public class InjokerApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        OnlineBoards.init(this);
    }
}
