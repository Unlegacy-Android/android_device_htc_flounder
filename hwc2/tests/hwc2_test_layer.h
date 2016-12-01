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

#include "hwc2_test_properties.h"

#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11

class hwc2_test_layer {
public:
    hwc2_test_layer(hwc2_test_coverage_t coverage, uint32_t z_order = 0);

    std::string dump() const;

    void reset();

    hwc2_blend_mode_t      get_blend_mode() const;
    const hwc_color_t      get_color() const;
    hwc2_composition_t     get_composition() const;
    android_dataspace_t    get_dataspace() const;
    float                  get_plane_alpha() const;
    hwc_transform_t        get_transform() const;
    uint32_t               get_z_order() const;

    bool advance_blend_mode();
    bool advance_color();
    bool advance_composition();
    bool advance_dataspace();
    bool advance_plane_alpha();
    bool advance_transform();

private:
    std::array<hwc2_test_container *, 6> properties = {{
        &blend_mode, &color, &composition, &dataspace, &plane_alpha, &transform
    }};

    hwc2_test_blend_mode blend_mode;
    hwc2_test_color color;
    hwc2_test_composition composition;
    hwc2_test_dataspace dataspace;
    hwc2_test_plane_alpha plane_alpha;
    hwc2_test_transform transform;

    uint32_t z_order;
};

#endif /* ifndef _HWC2_TEST_LAYER_H */
