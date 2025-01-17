// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lock_screen_action/lock_screen_action_background_controller_impl.h"

#include "ash/lock_screen_action/lock_screen_action_background_view.h"
#include "base/bind.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr char kLockScreenActionBackgroundWidgetName[] =
    "LockScreenActionBackground";

}  // namespace

LockScreenActionBackgroundControllerImpl::
    LockScreenActionBackgroundControllerImpl()
    : widget_observer_(this), weak_ptr_factory_(this) {}

LockScreenActionBackgroundControllerImpl::
    ~LockScreenActionBackgroundControllerImpl() {
  if (background_widget_ && !background_widget_->IsClosed())
    background_widget_->Close();
}

bool LockScreenActionBackgroundControllerImpl::IsBackgroundWindow(
    aura::Window* window) const {
  return window->GetName() == kLockScreenActionBackgroundWidgetName;
}

bool LockScreenActionBackgroundControllerImpl::ShowBackground() {
  if (state() == LockScreenActionBackgroundState::kShown ||
      state() == LockScreenActionBackgroundState::kShowing) {
    return false;
  }

  if (!parent_window_)
    return false;

  if (!background_widget_)
    background_widget_ = CreateWidget();

  UpdateState(LockScreenActionBackgroundState::kShowing);

  background_widget_->Show();

  contents_view_->AnimateShow(base::BindOnce(
      &LockScreenActionBackgroundControllerImpl::OnBackgroundShown,
      weak_ptr_factory_.GetWeakPtr()));

  return true;
}

bool LockScreenActionBackgroundControllerImpl::HideBackgroundImmediately() {
  if (state() == LockScreenActionBackgroundState::kHidden)
    return false;

  UpdateState(LockScreenActionBackgroundState::kHidden);

  background_widget_->Hide();
  return true;
}

bool LockScreenActionBackgroundControllerImpl::HideBackground() {
  if (state() == LockScreenActionBackgroundState::kHidden ||
      state() == LockScreenActionBackgroundState::kHiding) {
    return false;
  }

  DCHECK(background_widget_);

  UpdateState(LockScreenActionBackgroundState::kHiding);

  contents_view_->AnimateHide(base::BindOnce(
      &LockScreenActionBackgroundControllerImpl::OnBackgroundHidden,
      weak_ptr_factory_.GetWeakPtr()));

  return true;
}

void LockScreenActionBackgroundControllerImpl::OnWidgetDestroyed(
    views::Widget* widget) {
  if (widget != background_widget_)
    return;
  widget_observer_.Remove(widget);

  background_widget_ = nullptr;
  contents_view_ = nullptr;

  UpdateState(LockScreenActionBackgroundState::kHidden);
}

views::Widget* LockScreenActionBackgroundControllerImpl::CreateWidget() {
  // Passed to the widget as its delegate.
  contents_view_ = new LockScreenActionBackgroundView();

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.name = kLockScreenActionBackgroundWidgetName;
  params.parent = parent_window_;
  params.delegate = contents_view_;

  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->SetVisibilityChangedAnimationsEnabled(false);
  widget_observer_.Add(widget);

  return widget;
}

void LockScreenActionBackgroundControllerImpl::OnBackgroundShown() {
  if (state() != LockScreenActionBackgroundState::kShowing)
    return;

  UpdateState(LockScreenActionBackgroundState::kShown);
}

void LockScreenActionBackgroundControllerImpl::OnBackgroundHidden() {
  if (state() != LockScreenActionBackgroundState::kHiding)
    return;

  background_widget_->Hide();

  UpdateState(LockScreenActionBackgroundState::kHidden);
}

}  // namespace ash
