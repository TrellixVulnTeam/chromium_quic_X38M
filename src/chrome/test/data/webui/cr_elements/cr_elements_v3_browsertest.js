// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

/** Test fixture for shared Polymer 3 elements. */
// eslint-disable-next-line no-var
var CrElementsV3BrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://dummyurl';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }

  /** @override */
  get runAccessibilityChecks() {
    return true;
  }

  /** @override */
  setUp() {
    PolymerTest.prototype.setUp.call(this);
    // We aren't loading the main document.
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');
  }
};

// eslint-disable-next-line no-var
var CrElementsButtonV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_button_tests.m.js';
  }
};

TEST_F('CrElementsButtonV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsToastV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_toast_test.m.js';
  }
};

TEST_F('CrElementsToastV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrElementsViewManagerV3Test = class extends CrElementsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=cr_elements/cr_view_manager_test.m.js';
  }
};

TEST_F('CrElementsViewManagerV3Test', 'All', function() {
  mocha.run();
});
