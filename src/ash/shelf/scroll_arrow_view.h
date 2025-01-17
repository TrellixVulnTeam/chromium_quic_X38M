// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SCROLL_ARROW_VIEW_H_
#define ASH_SHELF_SCROLL_ARROW_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class ASH_EXPORT ScrollArrowView : public views::Button {
 public:
  enum ArrowType { kLeft, kRight };
  ScrollArrowView(ArrowType arrow_type,
                  bool is_horizontal_alignment,
                  views::ButtonListener* button_listener);
  ~ScrollArrowView() override;

  void set_is_horizontal_alignment(bool is_horizontal_alignment) {
    is_horizontal_alignment_ = is_horizontal_alignment;
  }

  // views::View:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

  // views::InkDropHost:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;

 private:
  ArrowType arrow_type_ = kLeft;
  bool is_horizontal_alignment_ = true;

  DISALLOW_COPY_AND_ASSIGN(ScrollArrowView);
};

}  // namespace ash

#endif  // ASH_SHELF_SCROLL_ARROW_VIEW_H_
