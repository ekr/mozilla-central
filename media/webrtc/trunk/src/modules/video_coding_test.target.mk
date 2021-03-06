# This file is generated by gyp; do not edit.

TOOLSET := target
TARGET := video_coding_test
DEFS_Debug := '-D_FILE_OFFSET_BITS=64' \
	'-DCHROMIUM_BUILD' \
	'-DUSE_NSS=1' \
	'-DTOOLKIT_USES_GTK=1' \
	'-DGTK_DISABLE_SINGLE_INCLUDES=1' \
	'-DENABLE_REMOTING=1' \
	'-DENABLE_P2P_APIS=1' \
	'-DENABLE_CONFIGURATION_POLICY' \
	'-DENABLE_INPUT_SPEECH' \
	'-DENABLE_NOTIFICATIONS' \
	'-DENABLE_GPU=1' \
	'-DENABLE_EGLIMAGE=1' \
	'-DUSE_SKIA=1' \
	'-DENABLE_REGISTER_PROTOCOL_HANDLER=1' \
	'-DENABLE_WEB_INTENTS=1' \
	'-DENABLE_PLUGIN_INSTALLATION=1' \
	'-DWEBRTC_TARGET_PC' \
	'-DWEBRTC_LINUX' \
	'-DWEBRTC_THREAD_RR' \
	'-DUNIT_TEST' \
	'-DGTEST_HAS_RTTI=0' \
	'-D__STDC_FORMAT_MACROS' \
	'-DDYNAMIC_ANNOTATIONS_ENABLED=1' \
	'-DWTF_USE_DYNAMIC_ANNOTATIONS=1' \
	'-D_DEBUG'

# Flags passed to all source files.
CFLAGS_Debug := -Werror \
	-pthread \
	-fno-exceptions \
	-fno-strict-aliasing \
	-Wall \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-fvisibility=hidden \
	-pipe \
	-fPIC \
	-Wextra \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-O0 \
	-g

# Flags passed to only C files.
CFLAGS_C_Debug := 

# Flags passed to only C++ files.
CFLAGS_CC_Debug := -fno-rtti \
	-fno-threadsafe-statics \
	-fvisibility-inlines-hidden \
	-Wsign-compare

INCS_Debug := -Isrc \
	-I. \
	-Isrc/modules/interface \
	-Isrc/modules/video_coding/codecs/vp8/main/interface \
	-Isrc/system_wrappers/interface \
	-Isrc/common_video/interface \
	-Isrc/modules/video_coding/main/source \
	-Itest \
	-Itesting/gtest/include \
	-Isrc/modules/video_coding/main/interface \
	-Isrc/modules/video_coding/codecs/interface \
	-Isrc/modules/rtp_rtcp/interface \
	-Isrc/modules/utility/interface \
	-Isrc/modules/audio_coding/main/interface \
	-Isrc/modules/video_processing/main/interface

DEFS_Release := '-D_FILE_OFFSET_BITS=64' \
	'-DCHROMIUM_BUILD' \
	'-DUSE_NSS=1' \
	'-DTOOLKIT_USES_GTK=1' \
	'-DGTK_DISABLE_SINGLE_INCLUDES=1' \
	'-DENABLE_REMOTING=1' \
	'-DENABLE_P2P_APIS=1' \
	'-DENABLE_CONFIGURATION_POLICY' \
	'-DENABLE_INPUT_SPEECH' \
	'-DENABLE_NOTIFICATIONS' \
	'-DENABLE_GPU=1' \
	'-DENABLE_EGLIMAGE=1' \
	'-DUSE_SKIA=1' \
	'-DENABLE_REGISTER_PROTOCOL_HANDLER=1' \
	'-DENABLE_WEB_INTENTS=1' \
	'-DENABLE_PLUGIN_INSTALLATION=1' \
	'-DWEBRTC_TARGET_PC' \
	'-DWEBRTC_LINUX' \
	'-DWEBRTC_THREAD_RR' \
	'-DUNIT_TEST' \
	'-DGTEST_HAS_RTTI=0' \
	'-D__STDC_FORMAT_MACROS' \
	'-DNDEBUG' \
	'-DNVALGRIND' \
	'-DDYNAMIC_ANNOTATIONS_ENABLED=0'

# Flags passed to all source files.
CFLAGS_Release := -Werror \
	-pthread \
	-fno-exceptions \
	-fno-strict-aliasing \
	-Wall \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-fvisibility=hidden \
	-pipe \
	-fPIC \
	-Wextra \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-O2 \
	-fno-ident \
	-fdata-sections \
	-ffunction-sections

# Flags passed to only C files.
CFLAGS_C_Release := 

# Flags passed to only C++ files.
CFLAGS_CC_Release := -fno-rtti \
	-fno-threadsafe-statics \
	-fvisibility-inlines-hidden \
	-Wsign-compare

INCS_Release := -Isrc \
	-I. \
	-Isrc/modules/interface \
	-Isrc/modules/video_coding/codecs/vp8/main/interface \
	-Isrc/system_wrappers/interface \
	-Isrc/common_video/interface \
	-Isrc/modules/video_coding/main/source \
	-Itest \
	-Itesting/gtest/include \
	-Isrc/modules/video_coding/main/interface \
	-Isrc/modules/video_coding/codecs/interface \
	-Isrc/modules/rtp_rtcp/interface \
	-Isrc/modules/utility/interface \
	-Isrc/modules/audio_coding/main/interface \
	-Isrc/modules/video_processing/main/interface

OBJS := $(obj).target/$(TARGET)/src/modules/video_coding/main/test/codec_database_test.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/decode_from_storage_test.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/generic_codec_test.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/jitter_buffer_test.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/media_opt_test.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/mt_test_common.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/mt_rx_tx_test.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/normal_test.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/quality_modes_test.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/receiver_timing_tests.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/rtp_player.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/test_callbacks.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/test_util.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/tester_main.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/video_rtp_play_mt.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/video_rtp_play.o \
	$(obj).target/$(TARGET)/src/modules/video_coding/main/test/video_source.o

# Add to the list of files we specially track dependencies for.
all_deps += $(OBJS)

# Make sure our dependencies are built before any of us.
$(OBJS): | $(obj).target/testing/libgtest.a $(obj).target/test/libtest_support.a $(obj).target/test/libmetrics.a $(obj).target/src/modules/libwebrtc_video_coding.a $(obj).target/src/modules/librtp_rtcp.a $(obj).target/src/modules/libwebrtc_utility.a $(obj).target/src/modules/libvideo_processing.a $(obj).target/src/common_video/libwebrtc_libyuv.a $(obj).target/testing/gtest_prod.stamp $(obj).target/testing/libgmock.a $(obj).target/third_party/libyuv/libyuv.a $(obj).target/src/modules/libwebrtc_i420.a $(obj).target/src/system_wrappers/source/libsystem_wrappers.a $(obj).target/src/modules/libwebrtc_vp8.a $(obj).target/third_party/libvpx/libvpx.a $(obj).target/src/modules/libaudio_coding_module.a $(obj).target/src/modules/libCNG.a $(obj).target/src/common_audio/libsignal_processing.a $(obj).target/src/modules/libG711.a $(obj).target/src/modules/libG722.a $(obj).target/src/modules/libiLBC.a $(obj).target/src/modules/libiSAC.a $(obj).target/src/modules/libiSACFix.a $(obj).target/src/modules/libPCM16B.a $(obj).target/src/modules/libNetEq.a $(obj).target/src/common_audio/libresampler.a $(obj).target/src/common_audio/libvad.a $(obj).target/src/modules/libvideo_processing_sse2.a

# CFLAGS et al overrides must be target-local.
# See "Target-specific Variable Values" in the GNU Make manual.
$(OBJS): TOOLSET := $(TOOLSET)
$(OBJS): GYP_CFLAGS := $(DEFS_$(BUILDTYPE)) $(INCS_$(BUILDTYPE))  $(CFLAGS_$(BUILDTYPE)) $(CFLAGS_C_$(BUILDTYPE))
$(OBJS): GYP_CXXFLAGS := $(DEFS_$(BUILDTYPE)) $(INCS_$(BUILDTYPE))  $(CFLAGS_$(BUILDTYPE)) $(CFLAGS_CC_$(BUILDTYPE))

# Suffix rules, putting all outputs into $(obj).

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(srcdir)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)

# Try building from generated source, too.

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(obj).$(TOOLSET)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(obj)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)

# End of this set of suffix rules
### Rules for final target.
LDFLAGS_Debug := -pthread \
	-Wl,-z,noexecstack \
	-fPIC \
	-B$(builddir)/../../third_party/gold

LDFLAGS_Release := -pthread \
	-Wl,-z,noexecstack \
	-fPIC \
	-B$(builddir)/../../third_party/gold \
	-Wl,-O1 \
	-Wl,--as-needed \
	-Wl,--gc-sections

LIBS := -lrt

$(builddir)/video_coding_test: GYP_LDFLAGS := $(LDFLAGS_$(BUILDTYPE))
$(builddir)/video_coding_test: LIBS := $(LIBS)
$(builddir)/video_coding_test: LD_INPUTS := $(OBJS) $(obj).target/testing/libgtest.a $(obj).target/test/libtest_support.a $(obj).target/test/libmetrics.a $(obj).target/src/modules/libwebrtc_video_coding.a $(obj).target/src/modules/librtp_rtcp.a $(obj).target/src/modules/libwebrtc_utility.a $(obj).target/src/modules/libvideo_processing.a $(obj).target/src/common_video/libwebrtc_libyuv.a $(obj).target/testing/libgmock.a $(obj).target/third_party/libyuv/libyuv.a $(obj).target/src/modules/libwebrtc_i420.a $(obj).target/src/system_wrappers/source/libsystem_wrappers.a $(obj).target/src/modules/libwebrtc_vp8.a $(obj).target/third_party/libvpx/libvpx.a $(obj).target/src/modules/libaudio_coding_module.a $(obj).target/src/modules/libCNG.a $(obj).target/src/common_audio/libsignal_processing.a $(obj).target/src/modules/libG711.a $(obj).target/src/modules/libG722.a $(obj).target/src/modules/libiLBC.a $(obj).target/src/modules/libiSAC.a $(obj).target/src/modules/libiSACFix.a $(obj).target/src/modules/libPCM16B.a $(obj).target/src/modules/libNetEq.a $(obj).target/src/common_audio/libresampler.a $(obj).target/src/common_audio/libvad.a $(obj).target/src/modules/libvideo_processing_sse2.a
$(builddir)/video_coding_test: TOOLSET := $(TOOLSET)
$(builddir)/video_coding_test: $(OBJS) $(obj).target/testing/libgtest.a $(obj).target/test/libtest_support.a $(obj).target/test/libmetrics.a $(obj).target/src/modules/libwebrtc_video_coding.a $(obj).target/src/modules/librtp_rtcp.a $(obj).target/src/modules/libwebrtc_utility.a $(obj).target/src/modules/libvideo_processing.a $(obj).target/src/common_video/libwebrtc_libyuv.a $(obj).target/testing/libgmock.a $(obj).target/third_party/libyuv/libyuv.a $(obj).target/src/modules/libwebrtc_i420.a $(obj).target/src/system_wrappers/source/libsystem_wrappers.a $(obj).target/src/modules/libwebrtc_vp8.a $(obj).target/third_party/libvpx/libvpx.a $(obj).target/src/modules/libaudio_coding_module.a $(obj).target/src/modules/libCNG.a $(obj).target/src/common_audio/libsignal_processing.a $(obj).target/src/modules/libG711.a $(obj).target/src/modules/libG722.a $(obj).target/src/modules/libiLBC.a $(obj).target/src/modules/libiSAC.a $(obj).target/src/modules/libiSACFix.a $(obj).target/src/modules/libPCM16B.a $(obj).target/src/modules/libNetEq.a $(obj).target/src/common_audio/libresampler.a $(obj).target/src/common_audio/libvad.a $(obj).target/src/modules/libvideo_processing_sse2.a FORCE_DO_CMD
	$(call do_cmd,link)

all_deps += $(builddir)/video_coding_test
# Add target alias
.PHONY: video_coding_test
video_coding_test: $(builddir)/video_coding_test

