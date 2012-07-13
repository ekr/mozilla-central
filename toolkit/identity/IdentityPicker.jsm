/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["pickId"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const PROTOCOL = "browserid 1.0";
const IDP_INTERFACE = 'rtcweb://idp-interface';

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/identity/Identity.jsm");

XPCOMUtils.defineLazyModuleGetter(
  this, "createAuthModule", "resource://gre/modules/identity/WebRTC.jsm");

function IdentitySelector() {
  this.idServices = {
    "persona": IdentityService
  };
  return this;
}

IdentitySelector.prototype = {
  /**
   * pickId: Pick one of our identityes and return an auth module for it
   *
   * @param aIdentity
   *        (string)     The identity to use
   *
   * @param aCallback
   *        (function)   Callback will be invoked with err (if any) as first
   *                     arg, and an object containing id and idp as second arg.
   */
  selectId: function(aIdentity, aCallback) {
    // From draft-rescorla-rtcweb-generic-idp-01
    //
    // 1.  The calling JS instantiates a PeerConnection and tells it that it
    //     is interested in having it authenticated via BrowserID
    //     (i.e., it provides "browserid.org" as the IdP name.)
    //
    // 2.  The PeerConnection instantiates the BrowserID signer in the IdP
    //     proxy
    //
    // 3.  The BrowserID signer contacts Alice's identity provider,
    //     authenticating as Alice (likely via a cookie).
    //
    // 4.  The identity provider returns a short-term certificate
    //     attesting to Alice's identity and her short-term public
    //     key.
    //
    // 5.  The Browser-ID code signs the fingerprint and returns the
    //     signed assertion + certificate to the PeerConnection.

    createAuthModule(idp, function(err, aAuthModule) {
      return aCallback(null, {
        identity: identity,
        idp: idp,
        protocol: PROTOCOL
      });
    });
  }
};

var selectId = (new IdentitySelector()).selectId;