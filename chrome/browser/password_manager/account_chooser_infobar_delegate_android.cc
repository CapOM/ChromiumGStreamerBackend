// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/account_chooser_infobar_delegate_android.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/android/infobars/account_chooser_infobar.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// static
void AccountChooserInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents,
    ManagePasswordsUIController* ui_controller) {
  InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(make_scoped_ptr(new AccountChooserInfoBar(
          make_scoped_ptr(new AccountChooserInfoBarDelegateAndroid(
              web_contents, ui_controller)))));
}

AccountChooserInfoBarDelegateAndroid::AccountChooserInfoBarDelegateAndroid(
    content::WebContents* web_contents,
    ManagePasswordsUIController* ui_controller)
    : ui_controller_(ui_controller) {
  bool is_smartlock_branding_enabled =
      password_bubble_experiment::IsSmartLockBrandingEnabled(
          ProfileSyncServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext())));
  message_link_range_ = gfx::Range();
  GetAccountChooserDialogTitleTextAndLinkRange(is_smartlock_branding_enabled,
                                               &message_, &message_link_range_);
}

AccountChooserInfoBarDelegateAndroid::~AccountChooserInfoBarDelegateAndroid() {
}

void AccountChooserInfoBarDelegateAndroid::ChooseCredential(
    size_t credential_index,
    password_manager::CredentialType credential_type) {
  using namespace password_manager;
  if (credential_type == CredentialType::CREDENTIAL_TYPE_EMPTY) {
    ui_controller_->ChooseCredential(autofill::PasswordForm(), credential_type);
    return;
  }
  DCHECK(credential_type == CredentialType::CREDENTIAL_TYPE_PASSWORD ||
         credential_type == CredentialType::CREDENTIAL_TYPE_FEDERATED);
  const auto& credentials_forms =
      (credential_type == CredentialType::CREDENTIAL_TYPE_PASSWORD)
          ? ui_controller_->GetCurrentForms()
          : ui_controller_->GetFederatedForms();
  if (credential_index < credentials_forms.size()) {
    ui_controller_->ChooseCredential(*credentials_forms[credential_index],
                                     credential_type);
  }
}

void AccountChooserInfoBarDelegateAndroid::LinkClicked() {
  InfoBarService::WebContentsFromInfoBar(infobar())
      ->OpenURL(content::OpenURLParams(
          GURL(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SMART_LOCK_PAGE)),
          content::Referrer(), NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
          false /* is_renderer_initiated */));
}

void AccountChooserInfoBarDelegateAndroid::InfoBarDismissed() {
  ChooseCredential(-1, password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY);
}

infobars::InfoBarDelegate::Type
AccountChooserInfoBarDelegateAndroid::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}
