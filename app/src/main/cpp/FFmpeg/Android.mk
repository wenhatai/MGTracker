LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libFFmpeg
LOCAL_SRC_FILES := libtest.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../libtest/jni/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := socket
LOCAL_SRC_FILES := source/interface.c source/file.c
LOCAL_STATIC_LIBRARIES += libtest
include $(BUILD_SHARED_LIBRARY)
