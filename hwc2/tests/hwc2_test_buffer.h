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

#ifndef _HWC2_TEST_BUFFER_GENERATOR_H
#define _HWC2_TEST_BUFFER_GENERATOR_H

#include <android-base/unique_fd.h>
#include <hardware/hwcomposer2.h>

class hwc2_test_buffer_generator;

class hwc2_test_buffer {
public:
    hwc2_test_buffer();
    ~hwc2_test_buffer();

    void update_buffer_area(int32_t buffer_width, int32_t buffer_height);
    void update_format(android_pixel_format_t format);

    int  get(buffer_handle_t *out_handle, int32_t *out_fence);
    void set(buffer_handle_t handle, int32_t fence);

protected:
    void close_fence();

    std::unique_ptr<hwc2_test_buffer_generator> buffer_generator;

    int32_t buffer_width;
    int32_t buffer_height;
    android_pixel_format_t format;

    std::mutex mutex;
    std::condition_variable cv;

    bool pending;
    buffer_handle_t handle;
    int32_t fence;
};

#endif /* ifndef _HWC2_TEST_BUFFER_GENERATOR_H */
