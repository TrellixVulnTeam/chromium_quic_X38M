// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_CHOOSER_RESOURCE_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_CHOOSER_RESOURCE_LOADER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ChooserResourceLoader {
  STATIC_ONLY(ChooserResourceLoader);

 public:
  // Returns the picker common stylesheet as a string.
  static Vector<char> GetPickerCommonStyleSheet();

  // Returns the picker common javascript as a string.
  static Vector<char> GetPickerCommonJS();

  // Returns the picker button stylesheet as a string.
  static Vector<char> GetPickerButtonStyleSheet();

  // Returns the suggestion picker stylesheet as a string.
  static Vector<char> GetSuggestionPickerStyleSheet();

  // Returns the suggestion picker javascript as a string.
  static Vector<char> GetSuggestionPickerJS();

  // Returns the suggestion picker stylesheet as a string.
  static Vector<char> GetCalendarPickerStyleSheet();

  // Returns the calendar picker refresh stylesheet as a string.
  static Vector<char> GetCalendarPickerRefreshStyleSheet();

  // Returns the suggestion picker javascript as a string.
  static Vector<char> GetCalendarPickerJS();

  // Returns the color suggestion picker stylesheet as a string.
  static Vector<char> GetColorSuggestionPickerStyleSheet();

  // Returns the color suggestion picker javascript as a string.
  static Vector<char> GetColorSuggestionPickerJS();

  // Returns the color picker stylesheet as a string.
  static Vector<char> GetColorPickerStyleSheet();

  // Returns the color picker javascript as a string.
  static Vector<char> GetColorPickerJS();

  // Returns the color picker common javascript as a string.
  static Vector<char> GetColorPickerCommonJS();

  // Returns the list picker stylesheet as a string.
  static Vector<char> GetListPickerStyleSheet();

  // Returns the list picker javascript as a string.
  static Vector<char> GetListPickerJS();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_CHOOSER_RESOURCE_LOADER_H_
