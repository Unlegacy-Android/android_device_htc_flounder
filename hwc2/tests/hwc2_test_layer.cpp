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

#include "hwc2_test_layer.h"

hwc2_test_layer::hwc2_test_layer(hwc2_test_coverage_t coverage,
        int32_t display_width, int32_t display_height, uint32_t z_order)
    : buffer(),
      blend_mode(coverage),
      buffer_area(coverage, display_width, display_height),
      color(coverage),
      composition(coverage),
      cursor(coverage, display_width, display_height),
      dataspace(coverage),
      display_frame(coverage, display_width, display_height),
      format(coverage),
      plane_alpha(coverage),
      source_crop(coverage),
      surface_damage(coverage),
      transform(coverage),
      z_order(z_order)
{
    buffer_area.set_dependent(&buffer);
    buffer_area.set_dependent(&source_crop);
    buffer_area.set_dependent(&surface_damage);
    format.set_dependent(&buffer);
}

std::string hwc2_test_layer::dump() const
{
    std::stringstream dmp;

    dmp << "layer: \n";

    for (auto property: properties)
        dmp << property->dump();

    dmp << "\tz order: " << z_order << "\n";

    return dmp.str();
}

int hwc2_test_layer::get_buffer(buffer_handle_t *out_handle,
        android::base::unique_fd *out_acquire_fence)
{
    int32_t acquire_fence;
    int ret = buffer.get(out_handle, &acquire_fence);
    out_acquire_fence->reset(acquire_fence);
    return ret;
}

void hwc2_test_layer::reset()
{
    for (auto property: properties)
        property->reset();
}

hwc2_blend_mode_t hwc2_test_layer::get_blend_mode() const
{
    return blend_mode.get();
}

const hwc_color_t hwc2_test_layer::get_color() const
{
    return color.get();
}

hwc2_composition_t hwc2_test_layer::get_composition() const
{
    return composition.get();
}

const std::pair<int32_t, int32_t> hwc2_test_layer::get_cursor() const
{
    return cursor.get();
}

android_dataspace_t hwc2_test_layer::get_dataspace() const
{
    return dataspace.get();
}

const hwc_rect_t hwc2_test_layer::get_display_frame() const
{
    return display_frame.get();
}

float hwc2_test_layer::get_plane_alpha() const
{
    return plane_alpha.get();
}

const hwc_frect_t hwc2_test_layer::get_source_crop() const
{
    return source_crop.get();
}

const hwc_region_t hwc2_test_layer::get_surface_damage() const
{
    return surface_damage.get();
}

hwc_transform_t hwc2_test_layer::get_transform() const
{
    return transform.get();
}

uint32_t hwc2_test_layer::get_z_order() const
{
    return z_order;
}

bool hwc2_test_layer::advance_blend_mode()
{
    return blend_mode.advance();
}

bool hwc2_test_layer::advance_buffer_area()
{
    return buffer_area.advance();
}

bool hwc2_test_layer::advance_color()
{
    return color.advance();
}

bool hwc2_test_layer::advance_composition()
{
    return composition.advance();
}

bool hwc2_test_layer::advance_cursor()
{
    return cursor.advance();
}

bool hwc2_test_layer::advance_dataspace()
{
    return dataspace.advance();
}

bool hwc2_test_layer::advance_display_frame()
{
    return display_frame.advance();
}

bool hwc2_test_layer::advance_format()
{
    return format.advance();
}

bool hwc2_test_layer::advance_plane_alpha()
{
    return plane_alpha.advance();
}

bool hwc2_test_layer::advance_source_crop()
{
    return source_crop.advance();
}

bool hwc2_test_layer::advance_surface_damage()
{
    return surface_damage.advance();
}

bool hwc2_test_layer::advance_transform()
{
    return transform.advance();
}
