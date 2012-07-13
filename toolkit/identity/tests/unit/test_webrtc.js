/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// imports IdentityPicker, AuthModule
let webrtc = {};
Cu.import("resource://gre/modules/identity/WebRTC.jsm", webrtc);
Cu.import("resource://gre/modules/identity/IdentityPicker.jsm");

let saved_state = {};
let TEST_FINGERPRINT = uuid();

function setup_picked_identity(aCallback) {
  // Setup a browserid identity
  setup_test_identity(TEST_USER, TEST_CERT, function() {
    // Now pick the browserid identity and get an authModule back for it
    webrtc.selectIdentity(TEST_FINGERPRINT, aCallback);
  });
}

add_test(function test_overall() {
  do_check_eq((typeof webrtc.selectIdentity), 'function');
  run_next_test();
});

/*
add_test(function test_instantiate_auth_module_fail() {
  webrtc.createAuthModule({
    //idp:"browserid.org",
    protocol:"persona"
  }, function(err, result) {
    do_check_neq(err, null);
    do_check_eq(result, null);
    run_next_test();
  });
});

add_test(function test_get_assertion() {
  webrtc.createAuthModule({
    idp:"browserid.org",
    protocol:"persona"
  }, function(err, result) {
    do_check_eq(err, null);
    do_check_neq(result, null);

    result.sign({
      identity:"ben@adida.net",
      origin:"https://example.com",
      message:"testmessage"
    }, function(err, result) {
      do_check_eq(err, null);
      do_check_neq(result, null);
      do_check_neq(result.assertion,null);

      saved_state.assertion = result.assertion;

      run_next_test();
    });
  });
});

add_test(function test_check_assertion_success() {
  webrtc.createAuthModule({
    idp: "browserid.org",
    protocol: "persona"
  }, function(err, result) {
    do_check_eq(err, null);
    do_check_neq(result, null);

    result.verify({
      assertion:saved_state.assertion
    }, function(err, result) {
      do_check_eq(err, null);
      do_check_neq(result, null);
      do_check_eq(result.identity, "ben@adida.net");
      do_check_eq(result.message,"testmessage");

      run_next_test();
    });
  });
});
*/
function run_test() {
  run_next_test();
}
