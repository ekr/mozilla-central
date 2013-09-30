/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "logging.h"
#include "nspr.h"

#include <iostream>

#include <mozilla/Scoped.h>
#include "VideoConduit.h"
#include "AudioConduit.h"

#include "video_engine/include/vie_external_codec.h"

#include "runnable_utils.h"
#include "WebrtcDaalaVideoCodec.h"
#include "daaladec.h"
#include "daalaenc.h"

namespace mozilla {

MOZ_MTLOG_MODULE("daala")

// Encoder.
WebrtcDaalaVideoEncoder::WebrtcDaalaVideoEncoder()
  : timestamp_(0),
    callback_(nullptr),
    mutex_("WebrtcDaalaVideoEncoder"),
    enc_ctx_(nullptr),
    img_(nullptr) {
  nsIThread* thread;

  nsresult rv = NS_NewNamedThread("encoder-thread", &thread);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  thread_ = thread;
}

int32_t WebrtcDaalaVideoEncoder::InitEncode(
    const webrtc::VideoCodec* codecSettings,
    int32_t numberOfCores,
    uint32_t maxPayloadSize) {
  max_payload_size_ = maxPayloadSize;

  daala_info info;
  daala_info_init(&info);
  info.pic_width = codecSettings->width;
  info.pic_height = codecSettings->height;
  info.nplanes = 3;
  info.plane_info[0].xdec = 0;
  info.plane_info[0].ydec = 0;
  info.plane_info[1].xdec = 1;
  info.plane_info[1].ydec = 1;
  info.plane_info[2].xdec = 1;
  info.plane_info[2].ydec = 1;
  info.keyframe_rate = 300;

  enc_ctx_ = daala_encode_create(&info);
  if (!enc_ctx_)
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;

  daala_comment dc;
  daala_comment_init(&dc);

  // Flush and discard the comments.
  for(;;) {
    ogg_packet op;
    int r = daala_encode_flush_header(enc_ctx_,
                                      &dc,
                                      &op);
    if (!r)
      break;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}


static void init_plane(const uint8_t *data, unsigned char dec,
                       od_img_plane *plane) {
  plane->data = static_cast<unsigned char *>(const_cast<uint8_t *>
                                             (data));
  plane->xdec = plane->ydec = dec;
  plane->xstride = plane->ystride = 0;
}

int32_t WebrtcDaalaVideoEncoder::Encode(
    const webrtc::I420VideoFrame& inputImage,
    const webrtc::CodecSpecificInfo* codecSpecificInfo,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  PRIntervalTime t0 = PR_IntervalNow();
  EncodedFrame encoded;

  const uint8_t* y = inputImage.buffer(webrtc::kYPlane);
  const uint8_t* u = inputImage.buffer(webrtc::kUPlane);
  const uint8_t* v = inputImage.buffer(webrtc::kVPlane);

  od_img daala_img;
  daala_img.nplanes = 3;
  daala_img.width = inputImage.width();
  daala_img.height = inputImage.height();
  init_plane(y, 0, &daala_img.planes[0]);
  init_plane(u, 1, &daala_img.planes[1]);
  init_plane(v, 1, &daala_img.planes[2]);

  int rv = daala_encode_img_in(enc_ctx_,
                               &daala_img,
                               1); // Dummy duration
  if (rv) {
    MOZ_MTLOG(ML_ERROR, "Failure encoding image");
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  for(;;) {
    ogg_packet op;

    rv = daala_encode_packet_out(enc_ctx_, 0, &op);
    if (rv == 0)
      break;

    if (rv < 0) {
      MOZ_MTLOG(ML_ERROR, "Failure encoding output packet "
                << op.bytes);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    /* We have a packet, let's output it */
    MOZ_MTLOG(ML_DEBUG, "Packet size " << op.bytes);
  }
  PRIntervalTime t1 = PR_IntervalNow();
  MOZ_MTLOG(ML_DEBUG, "Daala Frame encoded; Encoding time = "
            << PR_IntervalToMilliseconds(t1 - t0) << "ms");

  const uint8_t* buffer = y;

  encoded.width_ = inputImage.width();
  encoded.height_ = inputImage.height();
  encoded.value_ = *buffer;
  encoded.timestamp_ = timestamp_;
  timestamp_ += 90000 / 30;

  MutexAutoLock lock(mutex_);
  frames_.push(encoded);

  RUN_ON_THREAD(thread_,
                WrapRunnable(
                    // RefPtr keeps object alive.
                    nsRefPtr<WebrtcDaalaVideoEncoder>(this),
                    &WebrtcDaalaVideoEncoder::EmitFrames),
                NS_DISPATCH_NORMAL);

  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcDaalaVideoEncoder::EmitFrames() {
  MutexAutoLock lock(mutex_);

  while(!frames_.empty()) {
    EncodedFrame *frame = &frames_.front();
    EmitFrame(frame);
    frames_.pop();
  }
}

void WebrtcDaalaVideoEncoder::EmitFrame(EncodedFrame *frame) {
  webrtc::EncodedImage encoded_image;
  encoded_image._encodedWidth = frame->width_;
  encoded_image._encodedHeight = frame->height_;
  encoded_image._timeStamp = frame->timestamp_;  // TODO(ekr@rtfm.com): Fix times
  encoded_image.capture_time_ms_ = 0;
  encoded_image._frameType = webrtc::kKeyFrame;
  encoded_image._buffer=reinterpret_cast<uint8_t *>(frame);
  encoded_image._length = sizeof(EncodedFrame);
  encoded_image._size = sizeof(EncodedFrame);
  encoded_image._completeFrame = true;

  callback_->Encoded(encoded_image, NULL, NULL);
}

int32_t WebrtcDaalaVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  callback_ = callback;

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcDaalaVideoEncoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcDaalaVideoEncoder::SetChannelParameters(uint32_t packetLoss,
						     int rtt) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcDaalaVideoEncoder::SetRates(uint32_t newBitRate,
					 uint32_t frameRate) {
  return WEBRTC_VIDEO_CODEC_OK;
}



// Decoder.
WebrtcDaalaVideoDecoder::WebrtcDaalaVideoDecoder()
    : callback_(nullptr),
      mutex_("WebrtcDaalaVideoDecoder") {
  nsIThread* thread;

  nsresult rv = NS_NewNamedThread("encoder-thread", &thread);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  thread_ = thread;
}

int32_t WebrtcDaalaVideoDecoder::InitDecode(
    const webrtc::VideoCodec* codecSettings,
    int32_t numberOfCores) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcDaalaVideoDecoder::Decode(
    const webrtc::EncodedImage& inputImage,
    bool missingFrames,
    const webrtc::RTPFragmentationHeader* fragmentation,
    const webrtc::CodecSpecificInfo*
    codecSpecificInfo,
    int64_t renderTimeMs) {
  if (sizeof(EncodedFrame) != inputImage._length)
    return WEBRTC_VIDEO_CODEC_ERROR;

  EncodedFrame* frame = reinterpret_cast<EncodedFrame*>(
      inputImage._buffer);
  size_t len = frame->width_ * frame->height_;
  ScopedDeleteArray<uint8_t> data(new uint8_t[len]);
  memset(data.get(), frame->value_, len);

  MutexAutoLock lock(mutex_);
  if (decoded_image_.CreateFrame(len, data,
                                 len/4, data,
                                 len/4, data,
                                 frame->width_, frame->height_,
                                 frame->width_,
                                 frame->width_/2,
                                 frame->width_/2))
    return WEBRTC_VIDEO_CODEC_ERROR;
  decoded_image_.set_timestamp(inputImage._timeStamp);

  RUN_ON_THREAD(thread_,
                // Shared pointer keeps the object live.
                WrapRunnable(nsRefPtr<WebrtcDaalaVideoDecoder>(this),
                             &WebrtcDaalaVideoDecoder::RunCallback),
                NS_DISPATCH_NORMAL);

  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcDaalaVideoDecoder::RunCallback() {
  MutexAutoLock lock(mutex_);

  callback_->Decoded(decoded_image_);
}

int32_t WebrtcDaalaVideoDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) {
  callback_ = callback;

  return WEBRTC_VIDEO_CODEC_OK;
}


int32_t WebrtcDaalaVideoDecoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcDaalaVideoDecoder::Reset() {
  return WEBRTC_VIDEO_CODEC_OK;
}

}
