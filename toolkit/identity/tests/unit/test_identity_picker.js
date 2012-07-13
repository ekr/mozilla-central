/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/identity/IdentityPicker.jsm");

add_test(function test_overall() {
  do_check_eq(typeof pickId, "function");
  run_next_test();
});

add_test(function test_pickid() {
  pickId(function(err, authModule) {
    do_check_eq(err, null);
    do_check_eq(authModule.idp, "browserid.org");
    run_next_test();
  });
});

function run_test() {
  run_next_test();
}
