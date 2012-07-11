/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["pickId"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const PROTOCOL = "browserid 1.0";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/identity/Identity.jsm");



function IdentityPicker() {
  XPCOMUtils.defineLazyModuleGetter(
    this, "createAuthModule", "resource://gre/modules/identity/WebRTC.jsm");

  this.authModule = null;
  return this;
}

IdentityPicker.prototype = {

  /**
   * Tell the UI to pick an identity.  Callback returns err (if any),
   * and an object containing {identity, idp, protocol}
   *
   * for now, protocol is hardwired to BrowserID
   */
  pickId: function(aCallback) {
    let idp = "browserid.org";
    let identity = "Ben Adidia";

    this.createAuthModule(idp, function(err, aAuthModule) {
      return aCallback(null, {
        identity: identity,
        idp: idp,
        protocol: PROTOCOL
      });
    });
  }
};

var pickId = (new IdentityPicker()).pickId;