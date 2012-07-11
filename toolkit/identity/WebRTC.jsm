/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["createAuthModule"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/identity/RelyingParty.jsm");

Cu.import("resource://services-common/utils.js");

function AuthModule(aIDP) {
  // XXX: How do we know aIDP.idp aIDP.origin are valid?
  this._idp = aIDP;
}
AuthModule.prototype = {
  sign: function(aID, aCallback) {
    // XXX: Check for validity of these fields?
    if (!aIDP.origin) {
      aCallback(new Error("Origin not provided!"), null);
      return;
    }

    if (!aID.identity) {
      aCallback(new Error("Identity not provided!"), null);
      return;
    }

    if (!aID.message) {
      aCallback(new Error("Message not provided!"), null);
      return;
    }

    RelyingParty._generateAssertion(
      aID.origin, aID.identity, aID.message, aCallback
    );
  },
  verify: function(aAssertion, aCallback) {

  }
};

function createAuthModule(aIDP, aCallback) {
  if (!aIDP.idp) {
    aCallback(new Error("IDP not provided!"), null);
    return;
  }

  if (!aIDP.protocol) {
    aCallback(new Error("Protocol not provided!"), null);
    return;
  }

  if (aIDP.protocol != "persona") {
    aCallback(new Error("Protocol " + aIDP.protocol + " not supported!"), null);
    return;
  }

  CommonUtils.nextTick(function() {
    // For Persona, creation of the AuthModule is actually synchronous,
    // but it maybe different for other protocol, so we use a callback.
    aCallback(null, new AuthModule(aIDP));
  });
}

