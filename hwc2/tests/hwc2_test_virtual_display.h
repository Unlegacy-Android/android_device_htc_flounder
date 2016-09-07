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

#ifndef _HWC2_TEST_VIRTUAL_DISPLAY_H
#define _HWC2_TEST_VIRTUAL_DISPLAY_H

#include "hwc2_test_properties.h"

#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11

class hwc2_test_virtual_display {
public:
    hwc2_test_virtual_display(hwc2_test_coverage_t coverage);

    std::string dump() const;

    int get_buffer(buffer_handle_t *out_handle,
            android::base::unique_fd *out_acquire_fence);

    void reset();
    bool advance();

    android_pixel_format_t get_format() const;
    const std::pair<int32_t, int32_t> get_display_dimension() const;

private:
    std::array<hwc2_test_container *, 2> properties = {{
        &display_dimension, &format,
    }};

    hwc2_test_buffer buffer;

    hwc2_test_display_dimension display_dimension;
    hwc2_test_format format;
};

#endif /* ifndef _HWC2_TEST_VIRTUAL_DISPLAY_H */
