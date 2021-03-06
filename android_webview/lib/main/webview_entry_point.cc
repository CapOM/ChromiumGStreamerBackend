// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_version_info_values.h"
#include "android_webview/lib/main/webview_jni_onload.h"
#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"

// This is called by the VM when the shared library is first loaded.
// Most of the initialization is done in LibraryLoadedOnMainThread(), not here.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  // WebView uses native JNI exports; disable manual JNI registration because
  // we don't have a good way to detect the JNI registrations which is called,
  // outside of OnJNIOnLoadRegisterJNI code path.
  base::android::DisableManualJniRegistration();

  base::android::SetVersionNumber(PRODUCT_VERSION);

  if (base::android::IsManualJniRegistrationDisabled()) {
    base::android::InitVM(vm);
  } else {
    if (android_webview::OnJNIOnLoadRegisterJNI(vm))
      return -1;
  }

  if (!android_webview::OnJNIOnLoadInit()) {
    return -1;
  }
  return JNI_VERSION_1_4;
}
