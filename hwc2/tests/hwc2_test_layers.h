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

#ifndef _HWC2_TEST_LAYERS_H
#define _HWC2_TEST_LAYERS_H

#include <map>

#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11

#include "hwc2_test_properties.h"
#include "hwc2_test_layer.h"

class hwc2_test_layers {
public:
    hwc2_test_layers(const std::vector<hwc2_layer_t> &layers,
            hwc2_test_coverage_t coverage, int32_t display_width,
            int32_t display_height);

    std::string dump() const;

    void reset();

    bool advance_visible_regions();

    const hwc_region_t get_visible_region(hwc2_layer_t layer) const;
    uint32_t           get_z_order(hwc2_layer_t layer) const;

private:
    void set_visible_regions();

    std::map<hwc2_layer_t, hwc2_test_layer> test_layers;
};

#endif /* ifndef _HWC2_TEST_LAYERS_H */
