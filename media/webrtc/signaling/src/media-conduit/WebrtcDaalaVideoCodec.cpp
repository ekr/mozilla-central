/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "logging.h"
#include "nspr.h"

#include <cstdio>
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
    enc_ctx_(nullptr) {
  nsIThread* thread;

  nsresult rv = NS_NewNamedThread("encoder-thread", &thread);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  thread_ = thread;
}

static void dump_ogg_packet(const ogg_packet& op) {
  printf("Packet\n");
  for (long i=0; i<op.bytes; ++i) {
    printf("0x%.2x,", op.packet[i]);
    if ((i+1) % 8)
      printf(" ");
    else
      printf("\n");
  }
  printf("\n\n");
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
  info.keyframe_rate = 1;

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

    dump_ogg_packet(op);
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

static void init_plane(const uint8_t *data,
                       const od_img *img,
                       unsigned char dec,
                       od_img_plane *plane) {
  plane->data = static_cast<unsigned char *>(const_cast<uint8_t *>
                                             (data));
  plane->xdec = plane->ydec = dec;
  plane->xstride = 1;
  plane->ystride = img->width >> dec;
}

int32_t WebrtcDaalaVideoEncoder::Encode(
    const webrtc::I420VideoFrame& inputImage,
    const webrtc::CodecSpecificInfo* codecSpecificInfo,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  PRIntervalTime t0 = PR_IntervalNow();
  int encoded_ct = 0;

  const uint8_t* y = inputImage.buffer(webrtc::kYPlane);
  const uint8_t* u = inputImage.buffer(webrtc::kUPlane);
  const uint8_t* v = inputImage.buffer(webrtc::kVPlane);

  od_img daala_img;
  daala_img.nplanes = 3;
  daala_img.width = inputImage.width();
  daala_img.height = inputImage.height();
  init_plane(y, &daala_img, 0, &daala_img.planes[0]);
  init_plane(u, &daala_img, 1, &daala_img.planes[1]);
  init_plane(v, &daala_img, 1, &daala_img.planes[2]);

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
    {
      MutexAutoLock lock(mutex_);
      // Warning: constructing the encoded and pushing it on made things sad.
      // The value of data was bogus.
      // TODO(ekr@rtfm.com): investigate
      EncodedFrame dummy;
      frames_.push(dummy);
      EncodedFrame& encoded = frames_.front();
      encoded.width_ = inputImage.width();
      encoded.height_ = inputImage.height();
      encoded.timestamp_ = inputImage.timestamp();
      encoded.data = new DataBuffer(op.packet, op.bytes);
      // encoded.data = new DataBuffer(op.packet, 500);
      ++encoded_ct;
    }
  }
  PRIntervalTime t1 = PR_IntervalNow();
  MOZ_MTLOG(ML_DEBUG, "Daala Frame encoded; Encoding time = "
            << PR_IntervalToMilliseconds(t1 - t0) << "ms");

  if (encoded_ct) {
    RUN_ON_THREAD(thread_,
                  WrapRunnable(
                      // RefPtr keeps object alive.
                      nsRefPtr<WebrtcDaalaVideoEncoder>(this),
                      &WebrtcDaalaVideoEncoder::EmitFrames),
                  NS_DISPATCH_NORMAL);
  }

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
  encoded_image._buffer= const_cast<uint8_t *>(frame->data->data());
  encoded_image._length = frame->data->len();
  encoded_image._size = frame->data->len();
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
static unsigned char kDummyPacket1[] = {
0x80, 0x64, 0x61, 0x61, 0x6c, 0x61, 0x00, 0x00,
0x00, 0x80, 0x02, 0x00, 0x00, 0xe0, 0x01, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x03, 0x00,
0x00, 0x01, 0x01, 0x01, 0x01
};

static unsigned char kDummyPacket2[] = {
0x81, 0x64, 0x61, 0x61, 0x6c, 0x61, 0x2f, 0x00,
0x00, 0x00, 0x58, 0x69, 0x70, 0x68, 0x27, 0x73,
0x20, 0x65, 0x78, 0x70, 0x65, 0x72, 0x69, 0x6d,
0x65, 0x6e, 0x74, 0x61, 0x6c, 0x20, 0x65, 0x6e,
0x63, 0x6f, 0x64, 0x65, 0x72, 0x20, 0x6c, 0x69,
0x62, 0x72, 0x61, 0x72, 0x79, 0x20, 0x53, 0x65,
0x70, 0x20, 0x33, 0x30, 0x20, 0x32, 0x30, 0x31,
0x33, 0x00, 0x00, 0x00, 0x00
};

static unsigned char kDummyPacket3[] = {
  0x82, 0x64, 0x61, 0x61, 0x6c, 0x61
};

WebrtcDaalaVideoDecoder::WebrtcDaalaVideoDecoder()
    : callback_(nullptr),
      mutex_("WebrtcDaalaVideoDecoder"),
      dec_ctx_(nullptr) {
  nsIThread* thread;

  nsresult rv = NS_NewNamedThread("encoder-thread", &thread);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  thread_ = thread;
}

int32_t WebrtcDaalaVideoDecoder::InitDecode(
    const webrtc::VideoCodec* codecSettings,
    int32_t numberOfCores) {

  daala_info info;
  daala_info_init(&info);
  daala_comment dc;
  daala_comment_init(&dc);
  daala_setup_info* ds = nullptr;
  ogg_packet op;
  int rv;

  memset(&op, 0, sizeof(op));
  op.packet = kDummyPacket1;
  op.bytes = sizeof(kDummyPacket1);
  op.b_o_s = 1;
  rv = daala_decode_header_in(&info, &dc, &ds, &op);
  if (rv <= 0) {
    MOZ_MTLOG(ML_ERROR, "Failure reading header packet 1");
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  memset(&op, 0, sizeof(op));
  op.packet = kDummyPacket2;
  op.bytes = sizeof(kDummyPacket2);
  rv = daala_decode_header_in(&info, &dc, &ds, &op);
  if (rv <= 0) {
    MOZ_MTLOG(ML_ERROR, "Failure reading header packet 2");
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  memset(&op, 0, sizeof(op));
  op.packet = kDummyPacket3;
  op.bytes = sizeof(kDummyPacket3);
  rv = daala_decode_header_in(&info, &dc, &ds, &op);
  if (rv <= 0) {
    MOZ_MTLOG(ML_ERROR, "Failure reading header packet 3");
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  dec_ctx_ = daala_decode_alloc(&info, ds);
  if (!dec_ctx_) {
    MOZ_MTLOG(ML_ERROR, "Failure creating Daala ctx");
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcDaalaVideoDecoder::Decode(
    const webrtc::EncodedImage& inputImage,
    bool missingFrames,
    const webrtc::RTPFragmentationHeader* fragmentation,
    const webrtc::CodecSpecificInfo*
    codecSpecificInfo,
    int64_t renderTimeMs) {
  MOZ_MTLOG(ML_DEBUG, "Daala::Decode");
  ogg_packet op;
  memset(&op, 0, sizeof(op));
  op.packet = inputImage._buffer;
  op.bytes = inputImage._length;

  od_img img;
  memset(&img, 0, sizeof(img));
  int rv = daala_decode_packet_in(dec_ctx_, &img, &op);
  if (rv) {
    MOZ_MTLOG(ML_ERROR, "Failure reading data");
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  MOZ_ASSERT(!(img.width & 1));
  MOZ_ASSERT(!(img.height & 1));

  // TODO(ekr@rtfm.com): Assert that xstride == 1
  size_t y_len = img.height * img.width;
  ScopedDeleteArray<uint8_t> y(new uint8_t[y_len]);
  size_t u_len = (img.height * img.width) / 4;
  ScopedDeleteArray<uint8_t> u(new uint8_t[u_len]);
  size_t v_len = (img.height * img.width) / 4;
  ScopedDeleteArray<uint8_t> v(new uint8_t[v_len]);

  // Now copy the Daala packet into len.
  // First Y
  size_t to_offset = 0;
  size_t from_offset = 0;
  for (int32_t row = 0; row < img.height; ++row) {

    memcpy(y + to_offset, img.planes[0].data + from_offset,
           img.width);

    to_offset += img.width;
    from_offset += img.planes[0].ystride;
  }

  // Now U and V
  // TODO(ekr@rtfm.com): assert that the strides are equal.
  to_offset = from_offset = 0;
  for (int32_t row = 0; row < img.height/2; ++row) {
    memcpy(u + to_offset, img.planes[1].data + from_offset,
           img.width/2);
    memcpy(v + to_offset, img.planes[2].data + from_offset,
           img.width/2);

    to_offset += img.width/2;
    from_offset += img.planes[2].ystride;
  }

  MutexAutoLock lock(mutex_);
  if (decoded_image_.CreateFrame(y_len, y,
                                 u_len, u,
                                 v_len, v,
                                 img.width, img.height,
                                 img.width,
                                 img.width/2,
                                 img.width/2))
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
