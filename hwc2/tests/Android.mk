#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := hwc2-unit-tests
LOCAL_MODULE_TAGS := tests
LOCAL_CFLAGS += \
    -fstack-protector-all \
    -g \
    -Wall -Wextra \
    -Werror \
    -fno-builtin \
    -DEGL_EGLEXT_PROTOTYPES \
    -DGL_GLEXT_PROTOTYPES
LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libhardware \
    libEGL \
    libGLESv2 \
    libui \
    libgui \
    liblog
LOCAL_STATIC_LIBRARIES := \
    libbase \
    libadf \
    libadfhwc
LOCAL_SRC_FILES := \
    hwc2_test.cpp \
    hwc2_test_properties.cpp \
    hwc2_test_layer.cpp \
    hwc2_test_layers.cpp \
    hwc2_test_buffer.cpp \
    hwc2_test_client_target.cpp

include $(BUILD_NATIVE_TEST)
