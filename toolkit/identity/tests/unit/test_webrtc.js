/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// imports IdentityPicker, AuthModule
var webrtc = {};
Cu.import("resource://gre/modules/identity/WebRTC.jsm", webrtc);

function test_overall() {
  do_check_neq(webrtc.AuthModule, null);
  run_next_test();
}

let TESTS = [test_overall];

TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}
