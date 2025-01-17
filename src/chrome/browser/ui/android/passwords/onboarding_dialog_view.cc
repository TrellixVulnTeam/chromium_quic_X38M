// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/passwords/onboarding_dialog_view.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string16.h"
#include "chrome/android/chrome_jni_headers/OnboardingDialogBridge_jni.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_onboarding.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

OnboardingDialogView::OnboardingDialogView(
    ChromePasswordManagerClient* client,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save)
    : form_to_save_(std::move(form_to_save)), client_(client) {}

OnboardingDialogView::~OnboardingDialogView() {
  Java_OnboardingDialogBridge_destroy(base::android::AttachCurrentThread(),
                                      java_object_);
}

void OnboardingDialogView::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::WindowAndroid* window_android =
      client_->web_contents()->GetTopLevelNativeWindow();
  java_object_.Reset(Java_OnboardingDialogBridge_create(
      env, window_android->GetJavaObject(), reinterpret_cast<long>(this)));

  base::string16 onboarding_title =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ONBOARDING_TITLE);
  base::string16 onboarding_details =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ONBOARDING_DETAILS);

  Java_OnboardingDialogBridge_showDialog(
      env, java_object_,
      base::android::ConvertUTF16ToJavaString(env, onboarding_title),
      base::android::ConvertUTF16ToJavaString(env, onboarding_details));

  client_->GetPrefs()->SetInteger(
      password_manager::prefs::kPasswordManagerOnboardingState,
      static_cast<int>(password_manager::OnboardingState::kShown));
}

void OnboardingDialogView::OnboardingAccepted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  client_->OnOnboardingSuccessful(std::move(form_to_save_));
  delete this;
}

void OnboardingDialogView::OnboardingRejected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  delete this;
}
