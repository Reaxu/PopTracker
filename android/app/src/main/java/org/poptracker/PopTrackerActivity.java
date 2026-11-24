package org.poptracker;

import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.text.InputType;
import android.util.Log;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.CountDownLatch;

/**
 * PopTracker's activity. SDLActivity runs the native app (libmain.so, SDL_main in src/main.cpp);
 * this class adds what the native side can't do on its own:
 *  - extracting bundled assets (fonts, icons, CA certificates) to internal storage,
 *  - text input, file open and file save dialogs (called from src/uilib/dlg_android.cpp over JNI),
 *  - "Open with PopTracker" for pack .zip and state .json files.
 *
 * Dialog methods are called on the SDL thread and block until the user answered.
 */
public class PopTrackerActivity extends SDLActivity {
    private static final String TAG = "PopTracker";
    private static final int REQUEST_OPEN = 4101;
    private static final int REQUEST_SAVE = 4102;

    private CountDownLatch mPickLatch;
    private Uri mPickResult;
    private final Map<String, Uri> mPendingSaves = new HashMap<>();
    private static volatile String sPendingOpenFile;

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL2",
            "SDL2_image",
            "SDL2_ttf",
            "main"
        };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        extractAssets();

        // SDLActivity would pass intent.getData().getPath() to the app, which is useless for content:// URIs.
        // We copy the file ourselves and hand it to the app once it's running.
        Intent intent = getIntent();
        if (intent != null && Intent.ACTION_VIEW.equals(intent.getAction()) && intent.getData() != null) {
            sPendingOpenFile = copyToCache(intent.getData(), "open");
            setIntent(new Intent(Intent.ACTION_MAIN));
        }

        super.onCreate(savedInstanceState);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        if (intent != null && Intent.ACTION_VIEW.equals(intent.getAction()) && intent.getData() != null) {
            final Uri uri = intent.getData();
            new Thread(() -> {
                String path = copyToCache(uri, "open");
                if (path != null) {
                    // the app is running: same as dropping a file on the desktop window
                    SDLActivity.onNativeDropFile(path);
                }
            }).start();
        }
    }

    // ---------------------------------------------------------------------------------------------
    // assets

    /** Copy APK assets/assets/** to filesDir/assets/ when the app was installed or updated. */
    private void extractAssets() {
        File filesDir = getFilesDir();
        File marker = new File(filesDir, "assets.version");
        String version;
        try {
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), 0);
            version = info.lastUpdateTime + "";
        } catch (Exception e) {
            version = "unknown";
        }
        if (marker.exists() && version.equals(readSmallFile(marker)) && new File(filesDir, "assets").isDirectory())
            return;
        Log.i(TAG, "Extracting assets ...");
        try {
            copyAssetDir(getAssets(), "assets", new File(filesDir, "assets"));
            writeSmallFile(marker, version);
        } catch (IOException e) {
            Log.e(TAG, "Could not extract assets", e);
        }
    }

    private static void copyAssetDir(AssetManager am, String path, File dest) throws IOException {
        String[] children = am.list(path);
        if (children == null || children.length == 0) {
            // a file
            try (InputStream in = am.open(path); OutputStream out = new FileOutputStream(dest)) {
                copy(in, out);
            }
            return;
        }
        if (!dest.isDirectory() && !dest.mkdirs())
            throw new IOException("Could not create " + dest);
        for (String child : children)
            copyAssetDir(am, path + "/" + child, new File(dest, child));
    }

    // ---------------------------------------------------------------------------------------------
    // dialogs, called from native code (dlg_android.cpp) on the SDL thread

    /** Text input. Returns null if cancelled. */
    public String inputBox(final String title, final String message, final String dflt, final boolean password) {
        final String[] result = { null };
        final CountDownLatch latch = new CountDownLatch(1);
        runOnUiThread(() -> {
            final EditText input = new EditText(this);
            input.setSingleLine(true);
            input.setText(dflt);
            input.setSelectAllOnFocus(true);
            if (password)
                input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
            else
                input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
            LinearLayout layout = new LinearLayout(this);
            layout.setOrientation(LinearLayout.VERTICAL);
            int pad = (int) (20 * getResources().getDisplayMetrics().density);
            layout.setPadding(pad, pad / 2, pad, 0);
            TextView label = new TextView(this);
            label.setText(message);
            layout.addView(label);
            layout.addView(input);

            AlertDialog dialog = new AlertDialog.Builder(this)
                    .setTitle(title)
                    .setView(layout)
                    .setPositiveButton(android.R.string.ok, (d, w) -> result[0] = input.getText().toString())
                    .setNegativeButton(android.R.string.cancel, null)
                    .create();
            dialog.setOnDismissListener(d -> latch.countDown());
            dialog.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
            dialog.show();
            input.requestFocus();
        });
        awaitQuietly(latch);
        return result[0];
    }

    /**
     * Let the user pick a document, copy it to the cache dir and return that path (null if cancelled).
     * mimeTypes is a comma separated hint; we show all files since .json/.zip MIME types are unreliable.
     */
    public String openFile(final String title, final String mimeTypes, final String unused, final boolean multi) {
        Uri uri = pick(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            startActivityForResult(intent, REQUEST_OPEN);
        });
        if (uri == null)
            return null;
        return copyToCache(uri, "import");
    }

    /**
     * Let the user pick where to save. Returns a temporary file path for the app to write to;
     * saveFileDone() copies it to the picked document. Returns null if cancelled.
     */
    public String saveFile(final String title, final String suggestedName, final String mimeType, final boolean unused) {
        Uri uri = pick(() -> {
            Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType(mimeType == null || mimeType.isEmpty() ? "application/octet-stream" : mimeType);
            intent.putExtra(Intent.EXTRA_TITLE, suggestedName == null || suggestedName.isEmpty() ? "state.json" : suggestedName);
            startActivityForResult(intent, REQUEST_SAVE);
        });
        if (uri == null)
            return null;
        File dir = new File(getCacheDir(), "export");
        dir.mkdirs();
        String name = sanitizeFileName(queryDisplayName(uri, suggestedName));
        File tmp = new File(dir, name);
        synchronized (mPendingSaves) {
            mPendingSaves.put(tmp.getAbsolutePath(), uri);
        }
        return tmp.getAbsolutePath();
    }

    /** Copy a file written after saveFile() to its destination. */
    public boolean saveFileDone(final String path, final boolean success) {
        Uri uri;
        synchronized (mPendingSaves) {
            uri = mPendingSaves.remove(path);
        }
        File tmp = new File(path);
        if (uri == null)
            return success;
        boolean ok = false;
        ContentResolver cr = getContentResolver();
        if (success) {
            try (InputStream in = new FileInputStream(tmp); OutputStream out = cr.openOutputStream(uri, "wt")) {
                if (out != null) {
                    copy(in, out);
                    ok = true;
                }
            } catch (IOException | SecurityException e) {
                Log.e(TAG, "Could not save " + uri, e);
            }
        }
        if (!ok) {
            try {
                DocumentsContract.deleteDocument(cr, uri); // don't leave an empty file behind
            } catch (Exception ignored) {
            }
        }
        tmp.delete();
        return ok;
    }

    /** File the app was opened with ("Open with PopTracker") before it was running. Called once by native code. */
    public String takePendingOpenFile(String a, String b, String c, boolean d) {
        String path = sPendingOpenFile;
        sPendingOpenFile = null;
        return path;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_OPEN || requestCode == REQUEST_SAVE) {
            mPickResult = (resultCode == RESULT_OK && data != null) ? data.getData() : null;
            CountDownLatch latch = mPickLatch;
            if (latch != null)
                latch.countDown();
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    private Uri pick(final Runnable startPicker) {
        mPickResult = null;
        mPickLatch = new CountDownLatch(1);
        runOnUiThread(() -> {
            try {
                startPicker.run();
            } catch (Exception e) {
                Log.e(TAG, "Could not open file picker", e);
                mPickLatch.countDown();
            }
        });
        awaitQuietly(mPickLatch);
        mPickLatch = null;
        return mPickResult;
    }

    // ---------------------------------------------------------------------------------------------
    // helpers

    private String copyToCache(Uri uri, String subdir) {
        try {
            File dir = new File(getCacheDir(), subdir);
            dir.mkdirs();
            String name = sanitizeFileName(queryDisplayName(uri, "file"));
            File dest = new File(dir, name);
            try (InputStream in = getContentResolver().openInputStream(uri);
                 OutputStream out = new FileOutputStream(dest)) {
                if (in == null)
                    return null;
                copy(in, out);
            }
            return dest.getAbsolutePath();
        } catch (Exception e) {
            Log.e(TAG, "Could not read " + uri, e);
            return null;
        }
    }

    private String queryDisplayName(Uri uri, String fallback) {
        if ("file".equals(uri.getScheme()) && uri.getLastPathSegment() != null)
            return uri.getLastPathSegment();
        try (Cursor c = getContentResolver().query(uri, new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (c != null && c.moveToFirst()) {
                String name = c.getString(0);
                if (name != null && !name.isEmpty())
                    return name;
            }
        } catch (Exception ignored) {
        }
        return fallback;
    }

    private static String sanitizeFileName(String name) {
        String s = name.replaceAll("[/\\\\:*?\"<>|\\x00-\\x1f]", "_");
        return s.isEmpty() || s.startsWith(".") ? "file" + s : s;
    }

    private static void copy(InputStream in, OutputStream out) throws IOException {
        byte[] buf = new byte[64 * 1024];
        int n;
        while ((n = in.read(buf)) > 0)
            out.write(buf, 0, n);
    }

    private static void awaitQuietly(CountDownLatch latch) {
        while (true) {
            try {
                latch.await();
                return;
            } catch (InterruptedException ignored) {
            }
        }
    }

    private static String readSmallFile(File f) {
        try (InputStream in = new FileInputStream(f)) {
            byte[] buf = new byte[256];
            int n = in.read(buf);
            return n > 0 ? new String(buf, 0, n, "UTF-8") : "";
        } catch (IOException e) {
            return "";
        }
    }

    private static void writeSmallFile(File f, String s) throws IOException {
        try (OutputStream out = new FileOutputStream(f)) {
            out.write(s.getBytes("UTF-8"));
        }
    }
}
