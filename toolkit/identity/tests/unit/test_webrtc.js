/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// imports IdentityPicker, AuthModule
var webrtc = {};
Cu.import("resource://gre/modules/identity/WebRTC.jsm", webrtc);

add_test(function test_overall() {
  do_check_neq(webrtc.createAuthModule, null);
  run_next_test();
});

add_test(function test_instantiate_auth_module() {
  webrtc.createAuthModule({
                              idp:"browserid.org",
                              protocol:"persona"
                          }
                          , function(err, result) {
                       do_check_eq(err, null);
                       do_check_neq(result, null);
                       run_next_test();
                   });
});

function run_test() {
  run_next_test();
}
