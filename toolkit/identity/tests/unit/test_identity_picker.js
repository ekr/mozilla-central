/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/identity/IdentityPicker.jsm", pickId);

function test_overall() {
  do_check_eq(typeof pickId, "function");
  run_next_test();
}

let TESTS = [test_overall];

TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}
