// Copyright 2020-2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Simple main activity for Android.
 * @author Rylie Pavlik <rylie.pavlik@collabora.com>
 * @author Korcan Hussein <korcan.hussein@collabora.com>
 */

package org.freedesktop.monado.android_common;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentTransaction;
import dagger.hilt.android.AndroidEntryPoint;
import javax.inject.Inject;
import org.freedesktop.monado.auxiliary.NameAndLogoProvider;
import org.freedesktop.monado.auxiliary.UiProvider;

@AndroidEntryPoint
public class AboutActivity extends AppCompatActivity {

    @Inject NoticeFragmentProvider noticeFragmentProvider;

    @Inject UiProvider uiProvider;

    @Inject NameAndLogoProvider nameAndLogoProvider;

    private boolean isInProcessBuild() {
        try {
            getClassLoader().loadClass("org/freedesktop/monado/ipc/Client");
            return false;
        } catch (ClassNotFoundException e) {
            // ok, we're in-process.
        }
        return true;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestPermissions();

        setContentView(R.layout.activity_about);

        // Default to dark mode universally?
        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES);

        // Make our Monado link clickable
        ((TextView) findViewById(R.id.textPowered))
                .setMovementMethod(LinkMovementMethod.getInstance());

        // Branding from the branding provider
        ((TextView) findViewById(R.id.textName))
                .setText(nameAndLogoProvider.getLocalizedRuntimeName());
        ((ImageView) findViewById(R.id.imageView))
                .setImageDrawable(nameAndLogoProvider.getLogoDrawable());

        boolean isInProcess = isInProcessBuild();
        if (!isInProcess) {
            ShutdownProcess.Companion.setupRuntimeShutdownButton(this);
        }

        // Start doing fragments
        FragmentTransaction fragmentTransaction = getSupportFragmentManager().beginTransaction();

        @VrModeStatus.Status
        int status =
                VrModeStatus.detectStatus(
                        this, getApplicationContext().getApplicationInfo().packageName);

        VrModeStatus statusFrag = VrModeStatus.newInstance(status);
        fragmentTransaction.add(R.id.statusFrame, statusFrag, null);

        if (!isInProcess) {
            findViewById(R.id.drawOverOtherAppsFrame).setVisibility(View.VISIBLE);
            DisplayOverOtherAppsStatusFragment drawOverFragment =
                    new DisplayOverOtherAppsStatusFragment();
            fragmentTransaction.replace(R.id.drawOverOtherAppsFrame, drawOverFragment, null);
        }

        if (noticeFragmentProvider != null) {
            Fragment noticeFragment = noticeFragmentProvider.makeNoticeFragment();
            fragmentTransaction.add(R.id.aboutFrame, noticeFragment, null);
        }

        fragmentTransaction.commit();
    }

    static class RequestCode {
        public static final int READ_EXTERNAL_STORAGE = 1;
    }
    ;

    private void requestPermissions() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {

            if (ActivityCompat.shouldShowRequestPermissionRationale(
                    this, Manifest.permission.READ_EXTERNAL_STORAGE)) {
                Toast.makeText(
                                this,
                                "Please grant permissions to read external storage",
                                Toast.LENGTH_LONG)
                        .show();
            }

            String[] permissionStrings = new String[] {Manifest.permission.READ_EXTERNAL_STORAGE};
            ActivityCompat.requestPermissions(
                    this, permissionStrings, RequestCode.READ_EXTERNAL_STORAGE);
        } else {
            ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE);
        }
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == RequestCode.READ_EXTERNAL_STORAGE) {
            if (grantResults.length <= 0 || grantResults[0] != PackageManager.PERMISSION_GRANTED) {
                Toast.makeText(
                                this,
                                "Permissions Denied to read external storage",
                                Toast.LENGTH_LONG)
                        .show();
            }
        }
    }
}
