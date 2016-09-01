/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <array>
#include <gtest/gtest.h>
#include <dlfcn.h>
#include <hardware/hardware.h>

#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11

class hwc2_test : public testing::Test {
public:
    hwc2_test()
        : hwc2_device(nullptr) { }

    virtual void SetUp()
    {
        hw_module_t const *hwc2_module;

        int err = hw_get_module(HWC_HARDWARE_MODULE_ID, &hwc2_module);
        ASSERT_GE(err, 0) << "failed to get hwc hardware module: "
                << strerror(-err);

        /* The following method will fail if you have not run "adb shell stop" */
        err = hwc2_open(hwc2_module, &hwc2_device);
        ASSERT_GE(err, 0) << "failed to open hwc hardware module: "
                << strerror(-err);
    }

    virtual void TearDown()
    {
        if (hwc2_device)
            hwc2_close(hwc2_device);
    }

protected:
    hwc2_function_pointer_t get_function(hwc2_function_descriptor_t descriptor)
    {
        return hwc2_device->getFunction(hwc2_device, descriptor);
    }

    void get_capabilities(std::vector<hwc2_capability_t> *out_capabilities)
    {
        uint32_t num_capabilities = 0;

        hwc2_device->getCapabilities(hwc2_device, &num_capabilities, nullptr);

        out_capabilities->resize(num_capabilities);

        hwc2_device->getCapabilities(hwc2_device, &num_capabilities,
                (int32_t *) out_capabilities->data());
    }

    hwc2_device_t *hwc2_device;
};


static const std::array<hwc2_function_descriptor_t, 42> required_functions = {{
    HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES,
    HWC2_FUNCTION_CREATE_LAYER,
    HWC2_FUNCTION_CREATE_VIRTUAL_DISPLAY,
    HWC2_FUNCTION_DESTROY_LAYER,
    HWC2_FUNCTION_DESTROY_VIRTUAL_DISPLAY,
    HWC2_FUNCTION_DUMP,
    HWC2_FUNCTION_GET_ACTIVE_CONFIG,
    HWC2_FUNCTION_GET_CHANGED_COMPOSITION_TYPES,
    HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT,
    HWC2_FUNCTION_GET_COLOR_MODES,
    HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE,
    HWC2_FUNCTION_GET_DISPLAY_CONFIGS,
    HWC2_FUNCTION_GET_DISPLAY_NAME,
    HWC2_FUNCTION_GET_DISPLAY_REQUESTS,
    HWC2_FUNCTION_GET_DISPLAY_TYPE,
    HWC2_FUNCTION_GET_DOZE_SUPPORT,
    HWC2_FUNCTION_GET_HDR_CAPABILITIES,
    HWC2_FUNCTION_GET_MAX_VIRTUAL_DISPLAY_COUNT,
    HWC2_FUNCTION_GET_RELEASE_FENCES,
    HWC2_FUNCTION_PRESENT_DISPLAY,
    HWC2_FUNCTION_REGISTER_CALLBACK,
    HWC2_FUNCTION_SET_ACTIVE_CONFIG,
    HWC2_FUNCTION_SET_CLIENT_TARGET,
    HWC2_FUNCTION_SET_COLOR_MODE,
    HWC2_FUNCTION_SET_COLOR_TRANSFORM,
    HWC2_FUNCTION_SET_CURSOR_POSITION,
    HWC2_FUNCTION_SET_LAYER_BLEND_MODE,
    HWC2_FUNCTION_SET_LAYER_BUFFER,
    HWC2_FUNCTION_SET_LAYER_COLOR,
    HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE,
    HWC2_FUNCTION_SET_LAYER_DATASPACE,
    HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME,
    HWC2_FUNCTION_SET_LAYER_PLANE_ALPHA,
    /* HWC2_FUNCTION_SET_LAYER_SIDEBAND_STREAM unsupported by flounder */
    HWC2_FUNCTION_SET_LAYER_SOURCE_CROP,
    HWC2_FUNCTION_SET_LAYER_SURFACE_DAMAGE,
    HWC2_FUNCTION_SET_LAYER_TRANSFORM,
    HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION,
    HWC2_FUNCTION_SET_LAYER_Z_ORDER,
    HWC2_FUNCTION_SET_OUTPUT_BUFFER,
    HWC2_FUNCTION_SET_POWER_MODE,
    HWC2_FUNCTION_SET_VSYNC_ENABLED,
    HWC2_FUNCTION_VALIDATE_DISPLAY,
}};

TEST_F(hwc2_test, GET_FUNCTION)
{
    hwc2_function_pointer_t pfn;

    for (hwc2_function_descriptor_t descriptor: required_functions) {
        pfn = get_function(descriptor);
        EXPECT_TRUE(pfn) << "failed to get function "
                << getFunctionDescriptorName(descriptor);
    }
}

TEST_F(hwc2_test, GET_FUNCTION_invalid_function)
{
    hwc2_function_pointer_t pfn = get_function(HWC2_FUNCTION_INVALID);
    EXPECT_FALSE(pfn) << "failed to get invalid function";
}

TEST_F(hwc2_test, GET_CAPABILITIES)
{
    std::vector<hwc2_capability_t> capabilities;

    get_capabilities(&capabilities);

    EXPECT_EQ(std::find(capabilities.begin(), capabilities.end(),
            HWC2_CAPABILITY_INVALID), capabilities.end());
}
