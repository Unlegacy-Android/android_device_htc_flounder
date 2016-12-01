/* * Copyright (C) 2016 The Android Open Source Project
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

#include <sstream>

#include "hwc2_test_layers.h"

hwc2_test_layers::hwc2_test_layers(const std::vector<hwc2_layer_t> &layers,
        hwc2_test_coverage_t coverage, int32_t display_width,
        int32_t display_height)
    : test_layers()
{
    for (auto layer: layers)
        test_layers.emplace(std::piecewise_construct,
                std::forward_as_tuple(layer), std::forward_as_tuple(coverage,
                display_width, display_height));

    /* Iterate over the layers in order and assign z orders in the same order.
     * This allows us to iterate over z orders in the same way when computing
     * visible regions */
    uint32_t next_z_order = layers.size();

    for (auto &test_layer: test_layers)
        test_layer.second.set_z_order(next_z_order--);

    set_visible_regions();
}

std::string hwc2_test_layers::dump() const
{
    std::stringstream dmp;
    for (auto &test_layer: test_layers)
        dmp << test_layer.second.dump();
    return dmp.str();
}

void hwc2_test_layers::reset()
{
    for (auto &test_layer: test_layers)
        test_layer.second.reset();
    set_visible_regions();
}

bool hwc2_test_layers::advance_visible_regions()
{
    for (auto &test_layer: test_layers) {
        if (test_layer.second.advance_visible_region()) {
            set_visible_regions();
            return true;
        }
        test_layer.second.reset();
    }
    return false;
}

const hwc_region_t hwc2_test_layers::get_visible_region(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_visible_region();
}

uint32_t hwc2_test_layers::get_z_order(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_z_order();
}

void hwc2_test_layers::set_visible_regions()
{
    /* The region of the display that is covered by layers above the current
     * layer */
    android::Region above_opaque_layers;

    /* Iterate over test layers from max z order to min z order. */
    for (auto &test_layer: test_layers) {

        android::Region visible_region;
        const hwc_rect_t display_frame =
                test_layer.second.get_display_frame();

        /* Set the visible region of this layer to its display frame */
        visible_region.set(android::Rect(display_frame.left,
                display_frame.top, display_frame.right,
                display_frame.bottom));

        /* Remove the area covered by opaque layers above this layer
         * from this layer's visible region */
        visible_region.subtractSelf(above_opaque_layers);

        test_layer.second.set_visible_region(visible_region);

        /* If this layer is opaque, store the region it covers */
        if (test_layer.second.get_plane_alpha() == 1.0f)
            above_opaque_layers.orSelf(visible_region);
    }
}
