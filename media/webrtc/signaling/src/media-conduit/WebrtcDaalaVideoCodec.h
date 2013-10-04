/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
Sections of this code from Daala

Daala video codec
Copyright (c) 2006-2013 Daala project contributors.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.*/

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Class templates copied from WebRTC:
/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <queue>

#include "nsThreadUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/Scoped.h"
#include "databuffer.h"
#include "MediaConduitInterface.h"
#include "AudioConduit.h"
#include "VideoConduit.h"
#include "modules/video_coding/codecs/interface/video_codec_interface.h"

typedef struct od_img_plane od_img_plane;
typedef struct od_img od_img;
typedef struct daala_enc_ctx daala_enc_ctx;
typedef struct daala_dec_ctx daala_dec_ctx;

namespace mozilla {

class EncodedFrame {
 public:
  EncodedFrame() : data(nullptr) {}
  ~EncodedFrame() { delete data; }

  uint32_t width_;
  uint32_t height_;
  uint32_t timestamp_;
  DataBuffer* data;
};

class WebrtcDaalaVideoEncoder : public WebrtcVideoEncoder {
 public:
  WebrtcDaalaVideoEncoder();

  virtual ~WebrtcDaalaVideoEncoder() {
  }

  // Implement VideoEncoder interface.
  virtual int32_t InitEncode(const webrtc::VideoCodec* codecSettings,
                                     int32_t numberOfCores,
                                   uint32_t maxPayloadSize);

  virtual int32_t Encode(const webrtc::I420VideoFrame& inputImage,
      const webrtc::CodecSpecificInfo* codecSpecificInfo,
      const std::vector<webrtc::VideoFrameType>* frame_types);

  virtual int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback);

  virtual int32_t Release();

  virtual int32_t SetChannelParameters(uint32_t packetLoss,
                                             int rtt);

  virtual int32_t SetRates(uint32_t newBitRate,
                                 uint32_t frameRate);

 private:
  void EmitFrames();
  void EmitFrame(EncodedFrame *frame);

  nsCOMPtr<nsIThread> thread_;
  std::queue<EncodedFrame*> frames_;
  uint32_t max_payload_size_;
  uint32_t timestamp_;
  webrtc::EncodedImageCallback* callback_;
  mozilla::Mutex mutex_;
  ::daala_enc_ctx *enc_ctx_;
};


class WebrtcDaalaVideoDecoder : public WebrtcVideoDecoder {
 public:
  WebrtcDaalaVideoDecoder();

  virtual ~WebrtcDaalaVideoDecoder() {
  }

  // Implement VideoDecoder interface.
  virtual int32_t InitDecode(const webrtc::VideoCodec* codecSettings,
                                   int32_t numberOfCores);
  virtual int32_t Decode(const webrtc::EncodedImage& inputImage,
                               bool missingFrames,
                               const webrtc::RTPFragmentationHeader* fragmentation,
                               const webrtc::CodecSpecificInfo*
                               codecSpecificInfo = NULL,
                               int64_t renderTimeMs = -1);
  virtual int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback);

  virtual int32_t Release();

  virtual int32_t Reset();

 private:
  void RunCallback();

  nsCOMPtr<nsIThread> thread_;
  webrtc::DecodedImageCallback* callback_;
  webrtc::I420VideoFrame decoded_image_;
  mozilla::Mutex mutex_;
  ::daala_dec_ctx *dec_ctx_;
};

}
