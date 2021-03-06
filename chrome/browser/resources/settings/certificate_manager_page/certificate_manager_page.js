// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-certificate-manager-page' is the settings page containing SSL
 * certificate settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-certificate-manager-page prefs="{{prefs}}">
 *      </cr-settings-certificate-manager-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-certificate-manager-page
 */
Polymer({
  is: 'cr-settings-certificate-manager-page',

  properties: {
    /**
     * Preferences state.
     * TODO(dschuyler) check whether this is necessary.
     */
    prefs: {
      type: Object,
      notify: true,
    },
  },
});
