/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcDaalaVideoCodec.h"
#include "DaalaVideoCodec.h"

namespace mozilla {

VideoEncoder* DaalaVideoCodec::CreateEncoder() {
  WebrtcDaalaVideoEncoder *enc =
      new WebrtcDaalaVideoEncoder();
  return enc;
}

VideoDecoder* DaalaVideoCodec::CreateDecoder() {
  WebrtcDaalaVideoDecoder *dec =
      new WebrtcDaalaVideoDecoder();
  return dec;
}

}
