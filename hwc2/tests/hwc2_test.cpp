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
        : hwc2_device(nullptr),
          layers() { }

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
        hwc2_display_t display;
        hwc2_layer_t layer;

        for (auto itr = layers.begin(); itr != layers.end();) {
            display = itr->first;
            layer = itr->second;
            itr++;
            destroy_layer(display, layer);
        }

        if (hwc2_device)
            hwc2_close(hwc2_device);
    }

    void register_callback(hwc2_callback_descriptor_t descriptor,
            hwc2_callback_data_t callback_data, hwc2_function_pointer_t pointer,
            hwc2_error_t *out_err)
    {
        HWC2_PFN_REGISTER_CALLBACK pfn = (HWC2_PFN_REGISTER_CALLBACK)
                get_function(HWC2_FUNCTION_REGISTER_CALLBACK);
        ASSERT_TRUE(pfn) << "failed to get function";

        *out_err = (hwc2_error_t) pfn(hwc2_device, descriptor, callback_data,
                pointer);
    }

    void register_callback(hwc2_callback_descriptor_t descriptor,
            hwc2_callback_data_t callback_data, hwc2_function_pointer_t pointer)
    {
        hwc2_error_t err = HWC2_ERROR_NONE;
        ASSERT_NO_FATAL_FAILURE(register_callback(descriptor, callback_data,
                pointer, &err));
        ASSERT_EQ(err, HWC2_ERROR_NONE) << "failed to register callback";
    }

    void get_display_type(hwc2_display_t display, hwc2_display_type_t *out_type,
            hwc2_error_t *out_err)
    {
        HWC2_PFN_GET_DISPLAY_TYPE pfn = (HWC2_PFN_GET_DISPLAY_TYPE)
                get_function(HWC2_FUNCTION_GET_DISPLAY_TYPE);
        ASSERT_TRUE(pfn) << "failed to get function";

        *out_err = (hwc2_error_t) pfn(hwc2_device, display, (int32_t *) out_type);
    }

    void get_display_type(hwc2_display_t display, hwc2_display_type_t *out_type)
    {
        hwc2_error_t err = HWC2_ERROR_NONE;
        ASSERT_NO_FATAL_FAILURE(get_display_type(display, out_type, &err));
        ASSERT_EQ(err, HWC2_ERROR_NONE) << "failed to get display type";
    }

    void create_layer(hwc2_display_t display, hwc2_layer_t *out_layer,
            hwc2_error_t *out_err)
    {
        HWC2_PFN_CREATE_LAYER pfn = (HWC2_PFN_CREATE_LAYER)
                get_function(HWC2_FUNCTION_CREATE_LAYER);
        ASSERT_TRUE(pfn) << "failed to get function";

        *out_err = (hwc2_error_t) pfn(hwc2_device, display, out_layer);

        if (*out_err == HWC2_ERROR_NONE)
            layers.insert(std::make_pair(display, *out_layer));
    }

    void create_layer(hwc2_display_t display, hwc2_layer_t *out_layer)
    {
        hwc2_error_t err = HWC2_ERROR_NONE;
        ASSERT_NO_FATAL_FAILURE(create_layer(display, out_layer, &err));
        ASSERT_EQ(err, HWC2_ERROR_NONE) << "failed to create layer";
    }

    void destroy_layer(hwc2_display_t display, hwc2_layer_t layer,
            hwc2_error_t *out_err)
    {
        HWC2_PFN_DESTROY_LAYER pfn = (HWC2_PFN_DESTROY_LAYER)
                get_function(HWC2_FUNCTION_DESTROY_LAYER);
        ASSERT_TRUE(pfn) << "failed to get function";

        *out_err = (hwc2_error_t) pfn(hwc2_device, display, layer);

        if (*out_err == HWC2_ERROR_NONE)
            layers.erase(std::make_pair(display, layer));
    }

    void destroy_layer(hwc2_display_t display, hwc2_layer_t layer)
    {
        hwc2_error_t err = HWC2_ERROR_NONE;
        ASSERT_NO_FATAL_FAILURE(destroy_layer(display, layer, &err));
        ASSERT_EQ(err, HWC2_ERROR_NONE) << "failed to destroy layer " << layer;
    }

    void get_display_attribute(hwc2_display_t display, hwc2_config_t config,
            hwc2_attribute_t attribute, int32_t *out_value,
            hwc2_error_t *out_err)
    {
        HWC2_PFN_GET_DISPLAY_ATTRIBUTE pfn = (HWC2_PFN_GET_DISPLAY_ATTRIBUTE)
                get_function(HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE);
        ASSERT_TRUE(pfn) << "failed to get function";

        *out_err = (hwc2_error_t) pfn(hwc2_device, display, config, attribute,
                out_value);
    }

    void get_display_attribute(hwc2_display_t display, hwc2_config_t config,
            hwc2_attribute_t attribute, int32_t *out_value)
    {
        hwc2_error_t err = HWC2_ERROR_NONE;
        ASSERT_NO_FATAL_FAILURE(get_display_attribute(display, config,
                attribute, out_value, &err));
        ASSERT_EQ(err, HWC2_ERROR_NONE) << "failed to get display attribute "
                << getAttributeName(attribute) << " for config " << config;
    }

    void get_display_configs(hwc2_display_t display,
            std::vector<hwc2_config_t> *out_configs, hwc2_error_t *out_err)
    {
        HWC2_PFN_GET_DISPLAY_CONFIGS pfn = (HWC2_PFN_GET_DISPLAY_CONFIGS)
                get_function(HWC2_FUNCTION_GET_DISPLAY_CONFIGS);
        ASSERT_TRUE(pfn) << "failed to get function";

        uint32_t num_configs = 0;

        *out_err = (hwc2_error_t) pfn(hwc2_device, display, &num_configs,
                nullptr);
        if (*out_err != HWC2_ERROR_NONE)
            return;

        out_configs->resize(num_configs);

        *out_err = (hwc2_error_t) pfn(hwc2_device, display, &num_configs,
                out_configs->data());
    }

    void get_display_configs(hwc2_display_t display,
            std::vector<hwc2_config_t> *out_configs)
    {
        hwc2_error_t err = HWC2_ERROR_NONE;
        ASSERT_NO_FATAL_FAILURE(get_display_configs(display, out_configs, &err));
        ASSERT_EQ(err, HWC2_ERROR_NONE) << "failed to get configs for display "
                << display;
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

    /* NOTE: will create min(new_layer_cnt, max supported layers) layers */
    void create_layers(hwc2_display_t display,
            std::vector<hwc2_layer_t> &layers, size_t new_layer_cnt)
    {
        std::vector<hwc2_layer_t> new_layers;
        hwc2_layer_t layer;
        hwc2_error_t err = HWC2_ERROR_NONE;

        for (size_t i = 0; i < new_layer_cnt; i++) {

            EXPECT_NO_FATAL_FAILURE(create_layer(display, &layer, &err));
            if (err == HWC2_ERROR_NO_RESOURCES)
                break;
            if (err != HWC2_ERROR_NONE) {
                new_layers.clear();
                ASSERT_EQ(err, HWC2_ERROR_NONE) << "failed to create layer";
            }
            new_layers.push_back(layer);
        }

        layers.insert(layers.end(), new_layers.begin(), new_layers.end());
    }

    void destroy_layers(hwc2_display_t display,
            std::vector<hwc2_layer_t> &layers)
    {
        for (hwc2_layer_t layer: layers)
            EXPECT_NO_FATAL_FAILURE(destroy_layer(display, layer));
        layers.clear();
    }

    void get_invalid_config(hwc2_display_t display, hwc2_config_t *out_config)
    {
        std::vector<hwc2_config_t> configs;

        ASSERT_NO_FATAL_FAILURE(get_display_configs(display, &configs));

        std::set<hwc2_config_t> configs_set(configs.begin(), configs.end());
        hwc2_config_t CONFIG_MAX = UINT32_MAX;

        ASSERT_LE(configs_set.size() - 1, CONFIG_MAX) << "every config value"
                " (2^32 values) has been taken which shouldn't happen";

        hwc2_config_t config;
        for (config = 0; config < CONFIG_MAX; config++)
            if (configs_set.find(config) == configs_set.end())
                break;

        *out_config = config;
    }

    hwc2_device_t *hwc2_device;

    /* Store all created layers that have not been destroyed. If an ASSERT_*
     * fails, then destroy the layers on exit */
    std::set<std::pair<hwc2_display_t, hwc2_layer_t>> layers;
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

static const std::array<hwc2_callback_descriptor_t, 3> callback_descriptors = {{
    HWC2_CALLBACK_HOTPLUG,
    HWC2_CALLBACK_REFRESH,
    HWC2_CALLBACK_VSYNC,
}};

TEST_F(hwc2_test, REGISTER_CALLBACK)
{
    hwc2_callback_data_t data = (hwc2_callback_data_t) "data";

    for (hwc2_callback_descriptor_t descriptor: callback_descriptors)
        ASSERT_NO_FATAL_FAILURE(register_callback(descriptor, data,
                []() { return; }));
}

TEST_F(hwc2_test, REGISTER_CALLBACK_bad_parameter)
{
    hwc2_callback_data_t data = (hwc2_callback_data_t) "data";
    hwc2_error_t err = HWC2_ERROR_NONE;

    ASSERT_NO_FATAL_FAILURE(register_callback(HWC2_CALLBACK_INVALID, data,
            []() { return; }, &err));
    EXPECT_EQ(err, HWC2_ERROR_BAD_PARAMETER) << "returned wrong error code";
}

TEST_F(hwc2_test, REGISTER_CALLBACK_null_data)
{
    hwc2_callback_data_t data = nullptr;

    for (hwc2_callback_descriptor_t descriptor: callback_descriptors)
        ASSERT_NO_FATAL_FAILURE(register_callback(descriptor, data,
                []() { return; }));
}

TEST_F(hwc2_test, GET_DISPLAY_TYPE)
{
    hwc2_display_t display = HWC_DISPLAY_PRIMARY;
    hwc2_display_type_t type;

    ASSERT_NO_FATAL_FAILURE(get_display_type(display, &type));
    EXPECT_EQ(type, HWC2_DISPLAY_TYPE_PHYSICAL) << "failed to return correct"
            " display type";
}

TEST_F(hwc2_test, GET_DISPLAY_TYPE_bad_display)
{
    hwc2_display_t display = HWC_NUM_PHYSICAL_DISPLAY_TYPES + 1;
    hwc2_display_type_t type;
    hwc2_error_t err = HWC2_ERROR_NONE;

    ASSERT_NO_FATAL_FAILURE(get_display_type(display, &type, &err));
    EXPECT_EQ(err, HWC2_ERROR_BAD_DISPLAY) << "returned wrong error code";
}

TEST_F(hwc2_test, CREATE_DESTROY_LAYER)
{
    hwc2_display_t display = HWC_DISPLAY_PRIMARY;
    hwc2_layer_t layer;

    ASSERT_NO_FATAL_FAILURE(create_layer(display, &layer));

    ASSERT_NO_FATAL_FAILURE(destroy_layer(display, layer));
}

TEST_F(hwc2_test, CREATE_LAYER_bad_display)
{
    hwc2_display_t display = HWC_NUM_PHYSICAL_DISPLAY_TYPES + 1;
    hwc2_layer_t layer;
    hwc2_error_t err = HWC2_ERROR_NONE;

    ASSERT_NO_FATAL_FAILURE(create_layer(display, &layer, &err));
    EXPECT_EQ(err, HWC2_ERROR_BAD_DISPLAY) << "returned wrong error code";
}

TEST_F(hwc2_test, CREATE_LAYER_no_resources)
{
    hwc2_display_t display = HWC_DISPLAY_PRIMARY;
    std::vector<hwc2_layer_t> layers;
    size_t layer_cnt = 1000;

    ASSERT_NO_FATAL_FAILURE(create_layers(display, layers, layer_cnt));

    ASSERT_NO_FATAL_FAILURE(destroy_layers(display, layers));
}

TEST_F(hwc2_test, DESTROY_LAYER_bad_display)
{
    hwc2_display_t display = HWC_DISPLAY_PRIMARY;
    hwc2_display_t bad_display = HWC_NUM_PHYSICAL_DISPLAY_TYPES + 1;
    hwc2_layer_t layer = 0;
    hwc2_error_t err = HWC2_ERROR_NONE;

    ASSERT_NO_FATAL_FAILURE(destroy_layer(bad_display, layer, &err));
    EXPECT_EQ(err, HWC2_ERROR_BAD_DISPLAY) << "returned wrong error code";

    ASSERT_NO_FATAL_FAILURE(create_layer(display, &layer));

    ASSERT_NO_FATAL_FAILURE(destroy_layer(bad_display, layer, &err));
    EXPECT_EQ(err, HWC2_ERROR_BAD_DISPLAY) << "returned wrong error code";

    ASSERT_NO_FATAL_FAILURE(destroy_layer(display, layer));
}

TEST_F(hwc2_test, DESTROY_LAYER_bad_layer)
{
    hwc2_display_t display = HWC_DISPLAY_PRIMARY;
    hwc2_layer_t layer;
    hwc2_error_t err = HWC2_ERROR_NONE;

    ASSERT_NO_FATAL_FAILURE(destroy_layer(display, UINT64_MAX / 2, &err));
    EXPECT_EQ(err, HWC2_ERROR_BAD_LAYER) << "returned wrong error code";

    ASSERT_NO_FATAL_FAILURE(destroy_layer(display, 0, &err));
    EXPECT_EQ(err, HWC2_ERROR_BAD_LAYER) << "returned wrong error code";

    ASSERT_NO_FATAL_FAILURE(destroy_layer(display, UINT64_MAX - 1, &err));
    EXPECT_EQ(err, HWC2_ERROR_BAD_LAYER) << "returned wrong error code";

    ASSERT_NO_FATAL_FAILURE(destroy_layer(display, 1, &err));
    EXPECT_EQ(err, HWC2_ERROR_BAD_LAYER) << "returned wrong error code";

    ASSERT_NO_FATAL_FAILURE(destroy_layer(display, UINT64_MAX, &err));
    EXPECT_EQ(err, HWC2_ERROR_BAD_LAYER) << "returned wrong error code";

    ASSERT_NO_FATAL_FAILURE(create_layer(display, &layer));

    ASSERT_NO_FATAL_FAILURE(destroy_layer(display, layer + 1, &err));
    EXPECT_EQ(err, HWC2_ERROR_BAD_LAYER) << "returned wrong error code";

    ASSERT_NO_FATAL_FAILURE(destroy_layer(display, layer));

    ASSERT_NO_FATAL_FAILURE(destroy_layer(display, layer, &err));
    EXPECT_EQ(err, HWC2_ERROR_BAD_LAYER) << "returned wrong error code";
}

static const std::array<hwc2_attribute_t, 2> required_attributes = {{
    HWC2_ATTRIBUTE_WIDTH,
    HWC2_ATTRIBUTE_HEIGHT,
}};

static const std::array<hwc2_attribute_t, 3> optional_attributes = {{
    HWC2_ATTRIBUTE_VSYNC_PERIOD,
    HWC2_ATTRIBUTE_DPI_X,
    HWC2_ATTRIBUTE_DPI_Y,
}};

TEST_F(hwc2_test, GET_DISPLAY_ATTRIBUTE)
{
    hwc2_display_t display = HWC_DISPLAY_PRIMARY;
    std::vector<hwc2_config_t> configs;

    ASSERT_NO_FATAL_FAILURE(get_display_configs(display, &configs));

    int32_t value;

    for (hwc2_config_t config: configs) {
        for (hwc2_attribute_t attribute: required_attributes) {
            ASSERT_NO_FATAL_FAILURE(get_display_attribute(display, config,
                    attribute, &value));
            EXPECT_GE(value, 0) << "missing required attribute "
                    << getAttributeName(attribute) << " for config " << config;
        }
        for (hwc2_attribute_t attribute: optional_attributes)
            ASSERT_NO_FATAL_FAILURE(get_display_attribute(display, config,
                    attribute, &value));
    }
}

TEST_F(hwc2_test, GET_DISPLAY_ATTRIBUTE_invalid_attribute)
{
    hwc2_display_t display = HWC_DISPLAY_PRIMARY;
    std::vector<hwc2_config_t> configs;
    hwc2_error_t err = HWC2_ERROR_NONE;

    ASSERT_NO_FATAL_FAILURE(get_display_configs(display, &configs));

    hwc2_attribute_t attribute = HWC2_ATTRIBUTE_INVALID;
    int32_t value;

    for (hwc2_config_t config: configs) {
        ASSERT_NO_FATAL_FAILURE(get_display_attribute(display, config,
                attribute, &value, &err));
        EXPECT_EQ(value, -1) << "failed to return -1 for an invalid"
                " attribute for config " << config;
    }
}

TEST_F(hwc2_test, GET_DISPLAY_ATTRIBUTE_bad_display)
{
    hwc2_display_t display = HWC_NUM_PHYSICAL_DISPLAY_TYPES + 1;
    hwc2_config_t config = 0;
    int32_t value;
    hwc2_error_t err = HWC2_ERROR_NONE;

    for (hwc2_attribute_t attribute: required_attributes) {
        ASSERT_NO_FATAL_FAILURE(get_display_attribute(display, config,
                attribute, &value, &err));
        EXPECT_EQ(err, HWC2_ERROR_BAD_DISPLAY) << "returned wrong error code";
    }

    for (hwc2_attribute_t attribute: optional_attributes) {
        ASSERT_NO_FATAL_FAILURE(get_display_attribute(display, config,
                attribute, &value, &err));
        EXPECT_EQ(err, HWC2_ERROR_BAD_DISPLAY) << "returned wrong error code";
    }
}

TEST_F(hwc2_test, GET_DISPLAY_ATTRIBUTE_bad_config)
{
    hwc2_display_t display = HWC_DISPLAY_PRIMARY;
    hwc2_config_t config;
    int32_t value;
    hwc2_error_t err = HWC2_ERROR_NONE;

    ASSERT_NO_FATAL_FAILURE(get_invalid_config(display, &config));

    for (hwc2_attribute_t attribute: required_attributes) {
        ASSERT_NO_FATAL_FAILURE(get_display_attribute(display, config,
                attribute, &value, &err));
        EXPECT_EQ(err, HWC2_ERROR_BAD_CONFIG) << "returned wrong error code";
    }

    for (hwc2_attribute_t attribute: optional_attributes) {
        ASSERT_NO_FATAL_FAILURE(get_display_attribute(display, config,
                attribute, &value, &err));
        EXPECT_EQ(err, HWC2_ERROR_BAD_CONFIG) << "returned wrong error code";
    }
}

TEST_F(hwc2_test, GET_DISPLAY_CONFIGS)
{
    hwc2_display_t display = HWC_DISPLAY_PRIMARY;
    std::vector<hwc2_config_t> configs;

    ASSERT_NO_FATAL_FAILURE(get_display_configs(display, &configs));
}

TEST_F(hwc2_test, GET_DISPLAY_CONFIGS_bad_display)
{
    hwc2_display_t display = HWC_NUM_PHYSICAL_DISPLAY_TYPES + 1;
    std::vector<hwc2_config_t> configs;
    hwc2_error_t err = HWC2_ERROR_NONE;

    ASSERT_NO_FATAL_FAILURE(get_display_configs(display, &configs, &err));

    EXPECT_EQ(err, HWC2_ERROR_BAD_DISPLAY) << "returned wrong error code";
    EXPECT_TRUE(configs.empty()) << "returned configs for bad display";
}

TEST_F(hwc2_test, GET_DISPLAY_CONFIGS_same)
{
    hwc2_display_t display = HWC_DISPLAY_PRIMARY;
    std::vector<hwc2_config_t> configs1, configs2;

    ASSERT_NO_FATAL_FAILURE(get_display_configs(display, &configs1));
    ASSERT_NO_FATAL_FAILURE(get_display_configs(display, &configs2));

    EXPECT_TRUE(std::is_permutation(configs1.begin(), configs1.end(),
            configs2.begin())) << "returned two different config sets";
}

TEST_F(hwc2_test, GET_DISPLAY_CONFIGS_duplicate)
{
    hwc2_display_t display = HWC_DISPLAY_PRIMARY;
    std::vector<hwc2_config_t> configs;

    ASSERT_NO_FATAL_FAILURE(get_display_configs(display, &configs));

    std::set<hwc2_config_t> configs_set(configs.begin(), configs.end());
    EXPECT_EQ(configs.size(), configs_set.size()) << "returned duplicate"
            " configs";
}
