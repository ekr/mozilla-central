/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cc = Components.classes;
let Ci = Components.interfaces;

/**
  A class to add exceptions to override SSL certificate problems. The functionality
  itself is borrowed from exceptionDialog.js.
*/
function SSLExceptions() {
  this._overrideService = Cc["@mozilla.org/security/certoverride;1"]
                          .getService(Ci.nsICertOverrideService);
}


SSLExceptions.prototype = {
  _overrideService: null,
  _sslStatus: null,

  getInterface: function SSLE_getInterface(aIID) {
    return this.QueryInterface(aIID);
  },
  QueryInterface: function SSLE_QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIBadCertListener2) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /**
    To collect the SSL status we intercept the certificate error here
    and store the status for later use.
  */
  notifyCertProblem: function SSLE_notifyCertProblem(socketInfo, sslStatus, targetHost) {
    this._sslStatus = sslStatus.QueryInterface(Ci.nsISSLStatus);
    return true; // suppress error UI
  },

  /**
    Returns true if the private browsing mode is currently active
   */
  _inPrivateBrowsingMode: function SSLE_inPrivateBrowsingMode() {
    try {
      var pb = Cc["@mozilla.org/privatebrowsing;1"].getService(Ci.nsIPrivateBrowsingService);
      return pb.privateBrowsingEnabled;
    } catch (ex) {
      Components.utils.reportError("Could not get the Private Browsing service");
    }
    return false;
  },

  /**
    Attempt to download the certificate for the location specified to get the SSLState
    for the certificate and the errors.
   */
  _checkCert: function SSLE_checkCert(aURI) {
    this._sslStatus = null;
  
    var req = new XMLHttpRequest();
    try {
      if(aURI) {
        req.open("GET", aURI.prePath, false);
        req.channel.notificationCallbacks = this;
        req.send(null);
      }
    } catch (e) {
      // We *expect* exceptions if there are problems with the certificate
      // presented by the site.  Log it, just in case, but we can proceed here,
      // with appropriate sanity checks
      Components.utils.reportError("Attempted to connect to a site with a bad certificate in the add exception dialog. " +
                                   "This results in a (mostly harmless) exception being thrown. " +
                                   "Logged for information purposes only: " + e);
    }

    return this._sslStatus;
  },

  /**
    Internal method to create an override.
  */
  _addOverride: function SSLE_addOverride(aURI, temporary) {
    var SSLStatus = this._checkCert(aURI);
    var certificate = SSLStatus.serverCert;

    var flags = 0;

    // in private browsing do not store exceptions permanently ever
    if (this._inPrivateBrowsingMode()) {
      temporary = true;
    }

    if(SSLStatus.isUntrusted)
      flags |= this._overrideService.ERROR_UNTRUSTED;
    if(SSLStatus.isDomainMismatch)
      flags |= this._overrideService.ERROR_MISMATCH;
    if(SSLStatus.isNotValidAtThisTime)
      flags |= this._overrideService.ERROR_TIME;

    this._overrideService.rememberValidityOverride(
      aURI.asciiHost,
      aURI.port,
      certificate,
      flags,
      temporary);
  },

  /**
    Creates a permanent exception to override all overridable errors for
    the given URL.
  */
  addPermanentException: function SSLE_addPermanentException(aURI) {
    this._addOverride(aURI, false);
  },

  /**
    Creates a temporary exception to override all overridable errors for
    the given URL.
  */
  addTemporaryException: function SSLE_addTemporaryException(aURI) {
    this._addOverride(aURI, true);
  }
};
