LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libavcodec
LOCAL_SRC_FILES := FFmpeg/lib/$(TARGET_ARCH_ABI)/libavcodec.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/FFmpeg/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavfilter
LOCAL_SRC_FILES := FFmpeg/lib/$(TARGET_ARCH_ABI)/libavfilter.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/FFmpeg/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavformat
LOCAL_SRC_FILES := FFmpeg/lib/$(TARGET_ARCH_ABI)/libavformat.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/FFmpeg/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavutil
LOCAL_SRC_FILES := FFmpeg/lib/$(TARGET_ARCH_ABI)/libavutil.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/FFmpeg/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libswresample
LOCAL_SRC_FILES := FFmpeg/lib/$(TARGET_ARCH_ABI)/libswresample.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/FFmpeg/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libswscale
LOCAL_SRC_FILES := FFmpeg/lib/$(TARGET_ARCH_ABI)/libswscale.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/FFmpeg/include
include $(PREBUILT_STATIC_LIBRARY)
#
# include $(CLEAR_VARS)
# LOCAL_MODULE := libFFmpeg
# LOCAL_SRC_FILES := FFmpeg/lib/$(TARGET_ARCH_ABI)/libavcodec.a \
# 	FFmpeg/lib/$(TARGET_ARCH_ABI)/libavfilter.a \
# 	FFmpeg/lib/$(TARGET_ARCH_ABI)/libavformat.a \
# 	FFmpeg/lib/$(TARGET_ARCH_ABI)/libavutil.a \
# 	FFmpeg/lib/$(TARGET_ARCH_ABI)/libswresample.a \
# 	FFmpeg/lib/$(TARGET_ARCH_ABI)/libswscale.a
# LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/FFmpeg/include
# include $(PREBUILT_STATIC_LIBRARY)
include $(CLEAR_VARS)
LOCAL_MODULE     := nativemedia
LOCAL_SRC_FILES  := MediaCodec.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_LDLIBS     += -llog -ldl -lz

LOCAL_ARM_NENO := true

LOCAL_STATIC_LIBRARIES += libavformat libavcodec libswscale libswresample libavutil
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
#OPENCV_CAMERA_MODULES:=off
#OPENCV_INSTALL_MODULES:=off
OPENCV_LIB_TYPE:=STATIC
ifdef OPENCV_ANDROID_SDK
  ifneq ("","$(wildcard $(OPENCV_ANDROID_SDK)/OpenCV.mk)")
    include ${OPENCV_ANDROID_SDK}/OpenCV.mk
  else
    include ${OPENCV_ANDROID_SDK}/sdk/native/jni/OpenCV.mk
  endif
else
  include ../../sdk/native/jni/OpenCV.mk
endif

LOCAL_MODULE     := mgtracker
LOCAL_SRC_FILES  := jni_part.cpp LibCMT.cpp\
CMT/common_cmt.cpp CMT/Consensus.cpp CMT/fastcluster.cpp CMT/Fusion.cpp CMT/Matcher.cpp CMT/MGCMT.cpp CMT/MGLearnT.cpp CMT/MGPerformanceAdapter.cpp CMT/MGPredictor.cpp CMT/Tracker.cpp

# LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_LDLIBS     += -llog -ldl

include $(BUILD_SHARED_LIBRARY)
