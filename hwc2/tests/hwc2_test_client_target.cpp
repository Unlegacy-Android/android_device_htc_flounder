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

#include <sstream>

#include <ui/Rect.h>

#include "hwc2_test_client_target.h"

hwc2_test_client_target::hwc2_test_client_target()
    : buffer() { }

int hwc2_test_client_target::get_buffer(const hwc2_test_layers &test_layers,
        const std::set<hwc2_layer_t> &client_layers,
        bool clear_client_target, int32_t display_width,
        int32_t display_height, android_pixel_format_t format,
        buffer_handle_t *out_handle, int32_t *out_acquire_fence)
{
    if (!clear_client_target) {
        bool needs_client_target = false;

        for (auto client_layer: client_layers) {
            if (test_layers.get_visible_region(client_layer).numRects > 0) {
                needs_client_target = true;
                break;
            }
        }

        if (!needs_client_target) {
           *out_handle = nullptr;
           *out_acquire_fence = -1;
           return 0;
        }
    }

    buffer.update_buffer_area(display_width, display_height);
    buffer.update_format(format);
    return buffer.get(out_handle, out_acquire_fence, &test_layers,
            &client_layers);
}


hwc2_test_client_target_support::hwc2_test_client_target_support(
        hwc2_test_coverage_t coverage, int32_t display_width,
        int32_t display_height)
    : buffer_area(coverage, display_width, display_height),
      dataspace(coverage),
      format(coverage),
      surface_damage(coverage)
{
    buffer_area.set_dependent(&surface_damage);
}

std::string hwc2_test_client_target_support::dump() const
{
    std::stringstream dmp;

    dmp << "client target: \n";

    for (auto property: properties)
        dmp << property->dump();

    return dmp.str();
}

void hwc2_test_client_target_support::reset()
{
    for (auto property: properties)
        property->reset();
}

bool hwc2_test_client_target_support::advance()
{
    for (auto property: properties)
        if (property->advance())
            return true;
    return false;
}

const std::pair<int32_t, int32_t> hwc2_test_client_target_support::get_buffer_area() const
{
    return buffer_area.get();
}

android_dataspace_t hwc2_test_client_target_support::get_dataspace() const
{
    return dataspace.get();
}

android_pixel_format_t hwc2_test_client_target_support::get_format() const
{
    return format.get();
}

const hwc_region_t hwc2_test_client_target_support::get_surface_damage() const
{
    return surface_damage.get();
}
