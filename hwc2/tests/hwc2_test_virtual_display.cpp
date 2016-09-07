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

#include "hwc2_test_virtual_display.h"

hwc2_test_virtual_display::hwc2_test_virtual_display(
        hwc2_test_coverage_t coverage)
    : buffer(),
      display_dimension(coverage),
      format(coverage)
{
    display_dimension.set_dependent(&buffer);
}

std::string hwc2_test_virtual_display::dump() const
{
    std::stringstream dmp;

    dmp << "virtual display: \n";

    for (hwc2_test_container *property: properties)
        dmp << property->dump();

    return dmp.str();
}

int hwc2_test_virtual_display::get_buffer(buffer_handle_t *out_handle,
        android::base::unique_fd *out_acquire_fence)
{
    int32_t acquire_fence;
    int ret = buffer.get(out_handle, &acquire_fence);
    out_acquire_fence->reset(acquire_fence);
    return ret;
}

void hwc2_test_virtual_display::reset()
{
    for (hwc2_test_container *property: properties)
        property->reset();
}

bool hwc2_test_virtual_display::advance()
{
    for (hwc2_test_container *property: properties) {
        if (property->advance())
            return true;
    }
    return false;
}

const std::pair<int32_t, int32_t> hwc2_test_virtual_display::get_display_dimension() const
{
	return display_dimension.get();
}

android_pixel_format_t hwc2_test_virtual_display::get_format() const
{
	return format.get();
}
