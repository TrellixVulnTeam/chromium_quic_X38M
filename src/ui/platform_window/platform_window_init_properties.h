// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_INIT_PROPERTIES_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_INIT_PROPERTIES_H_

#include <string>

#include "build/build_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_FUCHSIA)
#include <fuchsia/ui/views/cpp/fidl.h>
#endif

namespace ui {

enum class PlatformWindowType {
  kWindow,
  kPopup,
  kMenu,
  kTooltip,
};

enum class PlatformWindowOpacity {
  kInferOpacity,
  kOpaqueWindow,
  kTranslucentWindow,
};

// Initial properties which are passed to PlatformWindow to be initialized
// with a desired set of properties.
struct PlatformWindowInitProperties {
  PlatformWindowInitProperties();

  // Initializes properties with the specified |bounds|.
  explicit PlatformWindowInitProperties(const gfx::Rect& bounds);

  PlatformWindowInitProperties(PlatformWindowInitProperties&& props);

  ~PlatformWindowInitProperties();

  // Tells desired PlatformWindow type. It can be popup, menu or anything else.
  PlatformWindowType type = PlatformWindowType::kWindow;
  // Sets the desired initial bounds. Can be empty.
  gfx::Rect bounds;
  // Tells PlatformWindow which native widget its parent holds. It is usually
  // used to find a parent from internal list of PlatformWindows.
  gfx::AcceleratedWidget parent_widget = gfx::kNullAcceleratedWidget;
  // Tells the opacity type of a window. Check the comment in the
  // Widget::InitProperties::WindowOpacity.
  PlatformWindowOpacity opacity = PlatformWindowOpacity::kOpaqueWindow;

#if defined(OS_FUCHSIA)
  fuchsia::ui::views::ViewToken view_token;
#endif

  bool activatable = true;
  bool force_show_in_taskbar;
  bool keep_on_top = false;
  bool visible_on_all_workspaces = false;
  bool remove_standard_frame = false;
  std::string workspace;

  // Only used by X11. Specifies the res_name and res_class fields,
  // respectively, of the WM_CLASS window property. Controls window grouping
  // and desktop file matching in Linux window managers.
  std::string wm_role_name;
  std::string wm_class_name;
  std::string wm_class_class;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_INIT_PROPERTIES_H_
