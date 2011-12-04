LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libsamsung-ril-client
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := samsung-ril-client.c

LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += hardware/ril/samsung-ril/include 

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
