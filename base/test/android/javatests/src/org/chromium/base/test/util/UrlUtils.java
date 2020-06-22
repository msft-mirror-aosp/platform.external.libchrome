// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.os.Bundle;

import org.junit.Assert;

import org.chromium.base.PathUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.MainDex;

/**
 * Collection of URL utilities.
 */
@MainDex
public class UrlUtils {
    public static final String EXTRA_ROOT_DIRECTORY =
            "org.chromium.base.test.util.UrlUtils.RootDirectory";
    private static final String DATA_DIR = "/chrome/test/data/";
    private static String sRootDirectory = PathUtils.getExternalStorageDirectory();

    /**
     * Sets the root directory from state in the bundle.
     * @param bundle Bundle containing the intent extras.
     */
    public static void setPathsFromBundle(Bundle bundle) {
        String rootDirectory = bundle.getString(EXTRA_ROOT_DIRECTORY);
        if (rootDirectory != null) sRootDirectory = rootDirectory;
    }

    /**
     * Construct the full path of a test data file.
     * @param path Pathname relative to external/chrome/test/data
     */
    public static String getTestFilePath(String path) {
        // TODO(jbudorick): Remove DATA_DIR once everything has been isolated. crbug/400499
        return getIsolatedTestFilePath(DATA_DIR + path);
    }

    // TODO(jbudorick): Remove this function once everything has been isolated and switched back
    // to getTestFilePath. crbug/400499
    /**
     * Construct the full path of a test data file.
     * @param path Pathname relative to external/
     */
    public static String getIsolatedTestFilePath(String path) {
        return getIsolatedTestRoot() + "/" + path;
    }

    /**
     * Returns the root of the test data directory.
     */
    @CalledByNative
    public static String getIsolatedTestRoot() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return sRootDirectory + "/chromium_tests_root";
        }
    }

    /**
     * Construct a suitable URL for loading a test data file.
     * @param path Pathname relative to external/chrome/test/data
     */
    public static String getTestFileUrl(String path) {
        return "file://" + getTestFilePath(path);
    }

    // TODO(jbudorick): Remove this function once everything has been isolated and switched back
    // to getTestFileUrl. crbug/400499
    /**
     * Construct a suitable URL for loading a test data file.
     * @param path Pathname relative to external/
     */
    public static String getIsolatedTestFileUrl(String path) {
        return "file://" + getIsolatedTestFilePath(path);
    }

    /**
     * Construct a data:text/html URI for loading from an inline HTML.
     * @param html An unencoded HTML
     * @return String An URI that contains the given HTML
     */
    public static String encodeHtmlDataUri(String html) {
        try {
            // URLEncoder encodes into application/x-www-form-encoded, so
            // ' '->'+' needs to be undone and replaced with ' '->'%20'
            // to match the Data URI requirements.
            String encoded =
                    "data:text/html;utf-8," + java.net.URLEncoder.encode(html, "UTF-8");
            encoded = encoded.replace("+", "%20");
            return encoded;
        } catch (java.io.UnsupportedEncodingException e) {
            Assert.fail("Unsupported encoding: " + e.getMessage());
            return null;
        }
    }
}
