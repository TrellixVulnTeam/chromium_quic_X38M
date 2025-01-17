// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lock_screen_action/lock_screen_note_launcher.h"

#include "ash/public/mojom/tray_action.mojom.h"
#include "ash/shell.h"
#include "ash/tray_action/tray_action.h"
#include "base/bind.h"

namespace ash {

LockScreenNoteLauncher::LockScreenNoteLauncher()
    : tray_action_observer_(this) {}

LockScreenNoteLauncher::~LockScreenNoteLauncher() = default;

// static
bool LockScreenNoteLauncher::CanAttemptLaunch() {
  return Shell::Get()->tray_action()->GetLockScreenNoteState() ==
         mojom::TrayActionState::kAvailable;
}

bool LockScreenNoteLauncher::Run(mojom::LockScreenNoteOrigin action_origin,
                                 LaunchCallback callback) {
  DCHECK(callback_.is_null());

  if (!CanAttemptLaunch())
    return false;

  callback_ = std::move(callback);
  tray_action_observer_.Add(Shell::Get()->tray_action());

  Shell::Get()->tray_action()->RequestNewLockScreenNote(action_origin);
  return true;
}

void LockScreenNoteLauncher::OnLockScreenNoteStateChanged(
    mojom::TrayActionState state) {
  if (state == mojom::TrayActionState::kLaunching)
    return;

  OnLaunchDone(state == mojom::TrayActionState::kActive);
}

void LockScreenNoteLauncher::OnLaunchDone(bool success) {
  tray_action_observer_.RemoveAll();
  if (!callback_.is_null())
    std::move(callback_).Run(success);
}

}  // namespace ash
