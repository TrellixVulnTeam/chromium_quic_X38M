// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_LOCK_ACTION_HANDLER_LAYOUT_MANAGER_H_
#define ASH_WM_LOCK_ACTION_HANDLER_LAYOUT_MANAGER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/lock_screen_action/lock_screen_action_background_observer.h"
#include "ash/lock_screen_action/lock_screen_action_background_state.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/tray_action/tray_action_observer.h"
#include "ash/wm/lock_layout_manager.h"
#include "base/macros.h"
#include "base/scoped_observer.h"

namespace ash {

class LockScreenActionBackgroundController;
class Shelf;
class TrayAction;

// Window layout manager for windows intended to handle lock tray actions.
// Since "new note" is currently the only supported action, the layout
// manager uses new note tray action state to determine it state.
// The layout is intended to be used for LockActionHandlerContainer. The
// container state depends on the lock screen "new_note" action state:
//   * for active action state - the windows should be visible above the lock
//     screen
//   * for rest of the states - the windows should not be visible.
// The layout manager will observe new note action state changes and update
// the container's children state as needed.
// The windows in this container will have be maximized, if possible. If they
// are not resizable, they will be centered on the screen - similar to windows
// in lock screen container.
// Unlike lock layout manager, when maximizing windows, this layout manager will
// ensure that the windows do not obscure the system shelf.
class ASH_EXPORT LockActionHandlerLayoutManager
    : public LockLayoutManager,
      public TrayActionObserver,
      public LockScreenActionBackgroundObserver {
 public:
  LockActionHandlerLayoutManager(
      aura::Window* window,
      Shelf* shelf,
      LockScreenActionBackgroundController* action_background_controller);
  ~LockActionHandlerLayoutManager() override;

  // WmDefaultLayoutManager:
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visibile) override;

  // TrayActionObserver:
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override;

  // LockScreenActionBackgroundObserver:
  void OnLockScreenActionBackgroundStateChanged(
      LockScreenActionBackgroundState state) override;

 private:
  // Updates the child window visibility depending on lock screen note action
  // state and the lock screen action background state.
  void UpdateChildren(mojom::TrayActionState action_state,
                      LockScreenActionBackgroundState background_state);

  LockScreenActionBackgroundController* action_background_controller_;

  ScopedObserver<TrayAction, TrayActionObserver> tray_action_observer_;
  ScopedObserver<LockScreenActionBackgroundController,
                 LockScreenActionBackgroundObserver>
      action_background_observer_;

  DISALLOW_COPY_AND_ASSIGN(LockActionHandlerLayoutManager);
};

}  // namespace ash

#endif  // ASH_WM_LOCK_ACTION_HANDLER_LAYOUT_MANAGER_H_
