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

#ifndef _HWC2_TEST_CLIENT_TARGET_H
#define _HWC2_TEST_CLIENT_TARGET_H

#include <set>

#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11

#include "hwc2_test_properties.h"
#include "hwc2_test_layers.h"

/* Generates client target buffers from client composition layers */
class hwc2_test_client_target {
public:
    hwc2_test_client_target();

    int get_buffer(const hwc2_test_layers &layers,
            const std::set<hwc2_layer_t> &client_layers,
            bool clear_client_target, int32_t display_width,
            int32_t display_height, android_pixel_format_t format,
            buffer_handle_t *out_handle, int32_t *out_acquire_fence);

private:
    hwc2_test_buffer buffer;
};

/* Generates valid client targets to test which ones the device will support */
class hwc2_test_client_target_support {
public:
    hwc2_test_client_target_support(hwc2_test_coverage_t coverage,
            int32_t display_width, int32_t display_height);

    std::string dump() const;

    void reset();
    bool advance();

    const std::pair<int32_t, int32_t> get_buffer_area() const;
    android_dataspace_t get_dataspace() const;
    android_pixel_format_t get_format() const;
    const hwc_region_t get_surface_damage() const;

private:
    std::array<hwc2_test_container *, 4> properties = {{
        &dataspace, &format, &surface_damage, &buffer_area
    }};

    hwc2_test_buffer_area buffer_area;
    hwc2_test_dataspace dataspace;
    hwc2_test_format format;
    hwc2_test_surface_damage surface_damage;
};

#endif /* ifndef _HWC2_TEST_CLIENT_TARGET_H */
