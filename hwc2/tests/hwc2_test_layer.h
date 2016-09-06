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

#ifndef _HWC2_TEST_LAYER_H
#define _HWC2_TEST_LAYER_H

#include <android-base/unique_fd.h>
#include <map>

#include "hwc2_test_properties.h"

#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11

class hwc2_test_layer {
public:
    hwc2_test_layer(hwc2_test_coverage_t coverage, int32_t display_width,
            int32_t display_height);

    hwc2_test_layer(hwc2_test_coverage_t coverage, int32_t display_width,
            int32_t display_height, const std::map<hwc2_test_property_t,
            hwc2_test_coverage_t> &coverage_exceptions);

    std::string dump() const;

    int get_buffer(buffer_handle_t *out_handle,
            android::base::unique_fd *out_acquire_fence);
    int get_buffer(buffer_handle_t *out_handle, int32_t *out_acquire_fence);

    void set_z_order(uint32_t z_order);
    void set_visible_region(const android::Region &region);

    void reset();
    bool advance();

    hwc2_blend_mode_t      get_blend_mode() const;
    const std::pair<int32_t, int32_t> get_buffer_area() const;
    const hwc_color_t      get_color() const;
    hwc2_composition_t     get_composition() const;
    const std::pair<int32_t, int32_t> get_cursor() const;
    android_dataspace_t    get_dataspace() const;
    const hwc_rect_t       get_display_frame() const;
    android_pixel_format_t get_format() const;
    float                  get_plane_alpha() const;
    const hwc_frect_t      get_source_crop() const;
    const hwc_region_t     get_surface_damage() const;
    hwc_transform_t        get_transform() const;
    const hwc_region_t     get_visible_region() const;
    uint32_t               get_z_order() const;

    bool advance_blend_mode();
    bool advance_buffer_area();
    bool advance_color();
    bool advance_composition();
    bool advance_cursor();
    bool advance_dataspace();
    bool advance_display_frame();
    bool advance_format();
    bool advance_plane_alpha();
    bool advance_source_crop();
    bool advance_surface_damage();
    bool advance_transform();
    bool advance_visible_region();

private:
    std::array<hwc2_test_container *, 12> properties = {{
        &blend_mode, &transform, &color, &cursor, &dataspace, &plane_alpha,
        &source_crop, &surface_damage, &buffer_area, &format, &display_frame,
        &composition
    }};

    hwc2_test_buffer buffer;

    hwc2_test_blend_mode blend_mode;
    hwc2_test_buffer_area buffer_area;
    hwc2_test_color color;
    hwc2_test_composition composition;
    hwc2_test_cursor cursor;
    hwc2_test_dataspace dataspace;
    hwc2_test_display_frame display_frame;
    hwc2_test_format format;
    hwc2_test_plane_alpha plane_alpha;
    hwc2_test_source_crop source_crop;
    hwc2_test_surface_damage surface_damage;
    hwc2_test_transform transform;
    hwc2_test_visible_region visible_region;

    uint32_t z_order;
};

#endif /* ifndef _HWC2_TEST_LAYER_H */
