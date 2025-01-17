// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_button.h"

#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Duration of button animations, in seconds.
const CGFloat kButtonAnimationDuration = 0.2;
// To achieve a circular corner radius, divide length of a side by 2.
const CGFloat kButtonCircularCornerRadiusDivisor = 2.0;
}  // namespace

@interface BadgeButton ()

// Read/Write override.
@property(nonatomic, assign, readwrite) BadgeType badgeType;
// Read/Write override.
@property(nonatomic, assign, readwrite) BOOL accepted;

@end

@implementation BadgeButton

+ (instancetype)badgeButtonWithType:(BadgeType)badgeType {
  BadgeButton* button = [self buttonWithType:UIButtonTypeSystem];
  button.badgeType = badgeType;

  return button;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.cornerRadius =
      self.bounds.size.height / kButtonCircularCornerRadiusDivisor;
}

- (void)setAccepted:(BOOL)accepted animated:(BOOL)animated {
  self.accepted = accepted;
  void (^changeTintColor)() = ^{
    self.tintColor = accepted ? [UIColor colorNamed:kBlueColor]
                              : [UIColor colorNamed:kToolbarButtonColor];
  };
  if (animated) {
    [UIView animateWithDuration:kButtonAnimationDuration
                     animations:changeTintColor];
  } else {
    changeTintColor();
  }
}

@end
