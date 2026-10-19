package com.bilko.openxwvr;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;

import org.libsdl.app.SDLActivity;

/**
 * OpenXW VR launcher. The engine runs entirely in libmain.so (SDL_main); this
 * class only supplies the library list, the game-data location and the
 * storage permission the original game files need.
 */
public class OpenXWActivity extends SDLActivity {
    private static final String TAG = "OpenXW";

    /** Original X-Wing installations copied from the PC: the 1994
     *  Collector's CD-ROM and the 1998 Windows release. */
    public static final String GAME_DATA_DIR = "/sdcard/OpenXW";
    public static final String XW94_DATA_DIR = GAME_DATA_DIR + "/XW94";
    public static final String XW98_DATA_DIR = GAME_DATA_DIR + "/XW98";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && !Environment.isExternalStorageManager()) {
            Log.w(TAG, "all-files access missing; asking the user to grant it");
            try {
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName()));
                startActivity(intent);
            } catch (Exception e) {
                Log.e(TAG, "could not open the all-files access settings", e);
            }
        }
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }

    @Override
    protected String[] getArguments() {
        return new String[] { "--xw94-data", XW94_DATA_DIR, "--xw98-data", XW98_DATA_DIR };
    }
}
