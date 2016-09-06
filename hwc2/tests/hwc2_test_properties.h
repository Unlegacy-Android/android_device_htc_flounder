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

#ifndef _HWC2_TEST_PROPERTIES_H
#define _HWC2_TEST_PROPERTIES_H

#include <array>
#include <vector>

#include <ui/Region.h>

#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11

#include "hwc2_test_buffer.h"

typedef enum {
    HWC2_TEST_COVERAGE_DEFAULT = 0,
    HWC2_TEST_COVERAGE_BASIC,
    HWC2_TEST_COVERAGE_COMPLETE,
    HWC2_TEST_NUM_COVERAGE_TYPES,
} hwc2_test_coverage_t;


class hwc2_test_container {
public:
    virtual ~hwc2_test_container() { };

    /* Resets the container */
    virtual void reset() = 0;

    /* Attempts to advance to the next valid value. Returns true if one can be
     * found */
    virtual bool advance() = 0;

    virtual std::string dump() const = 0;

    /* Returns true if the container supports the given composition type */
    virtual bool is_supported(hwc2_composition_t composition) = 0;
};


template <class T>
class hwc2_test_property : public hwc2_test_container {
public:
    hwc2_test_property(const std::vector<T> &list,
            const std::array<bool, 6> &composition_support)
        : list(list),
          list_idx(0),
          composition_support(composition_support) { }

    virtual void reset()
    {
        list_idx = 0;
    }

    virtual bool advance()
    {
        if (list_idx + 1 < list.size()) {
            list_idx++;
            update_dependents();
            return true;
        }
        reset();
        update_dependents();
        return false;
    }

    virtual T get() const
    {
        return list.at(list_idx);
    }

    virtual bool is_supported(hwc2_composition_t composition)
    {
        return composition_support.at(composition);
    }

protected:
    /* If a derived class has dependents, override this function */
    virtual void update_dependents() { }

    const std::vector<T> &list;
    uint32_t list_idx;

    const std::array<bool, 6> &composition_support;
};

class hwc2_test_source_crop;
class hwc2_test_surface_damage;

class hwc2_test_buffer_area : public hwc2_test_property<std::pair<int32_t, int32_t>> {
public:
    hwc2_test_buffer_area(hwc2_test_coverage_t coverage, int32_t display_width,
            int32_t display_height);

    std::string dump() const;

    void set_dependent(hwc2_test_buffer *buffer);
    void set_dependent(hwc2_test_source_crop *source_crop);
    void set_dependent(hwc2_test_surface_damage *surface_damage);

protected:
    void update();
    void update_dependents();

    const std::vector<float> &scalars;
    static const std::vector<float> default_scalars;
    static const std::vector<float> basic_scalars;
    static const std::vector<float> complete_scalars;

    int32_t display_width;
    int32_t display_height;

    hwc2_test_buffer *buffer;
    hwc2_test_source_crop *source_crop;
    hwc2_test_surface_damage *surface_damage;

    std::vector<std::pair<int32_t, int32_t>> buffer_areas;

    static const std::array<bool, 6> composition_support;
};


class hwc2_test_blend_mode : public hwc2_test_property<hwc2_blend_mode_t> {
public:
    hwc2_test_blend_mode(hwc2_test_coverage_t coverage);

    std::string dump() const;

protected:
    static const std::vector<hwc2_blend_mode_t> default_blend_modes;
    static const std::vector<hwc2_blend_mode_t> basic_blend_modes;
    static const std::vector<hwc2_blend_mode_t> complete_blend_modes;

    static const std::array<bool, 6> composition_support;
};


class hwc2_test_color : public hwc2_test_property<hwc_color_t> {
public:
    hwc2_test_color(hwc2_test_coverage_t coverage);

    std::string dump() const;

protected:
    static const std::vector<hwc_color_t> default_colors;
    static const std::vector<hwc_color_t> basic_colors;
    static const std::vector<hwc_color_t> complete_colors;

    static const std::array<bool, 6> composition_support;
};


class hwc2_test_composition : public hwc2_test_property<hwc2_composition_t> {
public:
    hwc2_test_composition(hwc2_test_coverage_t coverage);

    std::string dump() const;

protected:
    static const std::vector<hwc2_composition_t> default_compositions;
    static const std::vector<hwc2_composition_t> basic_compositions;
    static const std::vector<hwc2_composition_t> complete_compositions;

    static const std::array<bool, 6> composition_support;
};


class hwc2_test_cursor : public hwc2_test_property<std::pair<int32_t, int32_t>> {
public:
    hwc2_test_cursor(hwc2_test_coverage_t coverage, int32_t display_width,
            int32_t display_height);

    std::string dump() const;

protected:
    void update();

    const std::vector<float> &scalars;
    static const std::vector<float> default_scalars;
    static const std::vector<float> basic_scalars;
    static const std::vector<float> complete_scalars;

    int32_t display_width;
    int32_t display_height;

    std::vector<std::pair<int32_t, int32_t>> cursors;

    static const std::array<bool, 6> composition_support;
};


class hwc2_test_dataspace : public hwc2_test_property<android_dataspace_t> {
public:
    hwc2_test_dataspace(hwc2_test_coverage_t coverage);

    std::string dump() const;

protected:
    static const std::vector<android_dataspace_t> default_dataspaces;
    static const std::vector<android_dataspace_t> basic_dataspaces;
    static const std::vector<android_dataspace_t> complete_dataspaces;

    static const std::array<bool, 6> composition_support;
};


class hwc2_test_display_frame : public hwc2_test_property<hwc_rect_t> {
public:
    hwc2_test_display_frame(hwc2_test_coverage_t coverage,
            int32_t display_width, int32_t display_height);

    std::string dump() const;

protected:
    void update();

    const std::vector<hwc_frect_t> &frect_scalars;
    const static std::vector<hwc_frect_t> default_frect_scalars;
    const static std::vector<hwc_frect_t> basic_frect_scalars;
    const static std::vector<hwc_frect_t> complete_frect_scalars;

    int32_t display_width;
    int32_t display_height;

    std::vector<hwc_rect_t> display_frames;

    static const std::array<bool, 6> composition_support;
};


class hwc2_test_format : public hwc2_test_property<android_pixel_format_t> {
public:
    hwc2_test_format(hwc2_test_coverage_t coverage);

    std::string dump() const;

    void set_dependent(hwc2_test_buffer *buffer);

protected:
    void update_dependents();

    hwc2_test_buffer *buffer;

    static const std::vector<android_pixel_format_t> default_formats;
    static const std::vector<android_pixel_format_t> basic_formats;
    static const std::vector<android_pixel_format_t> complete_formats;

    static const std::array<bool, 6> composition_support;
};


class hwc2_test_plane_alpha : public hwc2_test_property<float> {
public:
    hwc2_test_plane_alpha(hwc2_test_coverage_t coverage);

    std::string dump() const;

protected:
    static const std::vector<float> default_plane_alphas;
    static const std::vector<float> basic_plane_alphas;
    static const std::vector<float> complete_plane_alphas;

    static const std::array<bool, 6> composition_support;
};


class hwc2_test_source_crop : public hwc2_test_property<hwc_frect_t> {
public:
    hwc2_test_source_crop(hwc2_test_coverage_t coverage, float buffer_width = 0,
            float buffer_height = 0);

    std::string dump() const;

    void update_buffer_area(float buffer_width, float buffer_height);

protected:
    void update();

    const std::vector<hwc_frect_t> &frect_scalars;
    const static std::vector<hwc_frect_t> default_frect_scalars;
    const static std::vector<hwc_frect_t> basic_frect_scalars;
    const static std::vector<hwc_frect_t> complete_frect_scalars;

    float buffer_width;
    float buffer_height;

    std::vector<hwc_frect_t> source_crops;

    static const std::array<bool, 6> composition_support;
};


class hwc2_test_surface_damage : public hwc2_test_property<hwc_region_t> {
public:
    hwc2_test_surface_damage(hwc2_test_coverage_t coverage);
    ~hwc2_test_surface_damage();

    std::string dump() const;

    void update_buffer_area(int32_t buffer_width, int32_t buffer_height);

protected:
    void update();
    void free_surface_damages();

    const std::vector<std::vector<hwc_frect_t>> &region_scalars;
    const static std::vector<std::vector<hwc_frect_t>> default_region_scalars;
    const static std::vector<std::vector<hwc_frect_t>> basic_region_scalars;
    const static std::vector<std::vector<hwc_frect_t>> complete_region_scalars;

    int32_t buffer_width;
    int32_t buffer_height;

    std::vector<hwc_region_t> surface_damages;

    static const std::array<bool, 6> composition_support;
};


class hwc2_test_transform : public hwc2_test_property<hwc_transform_t> {
public:
    hwc2_test_transform(hwc2_test_coverage_t coverage);

    std::string dump() const;

protected:
    static const std::vector<hwc_transform_t> default_transforms;
    static const std::vector<hwc_transform_t> basic_transforms;
    static const std::vector<hwc_transform_t> complete_transforms;

    static const std::array<bool, 6> composition_support;
};


class hwc2_test_visible_region {
public:
    hwc2_test_visible_region();
    ~hwc2_test_visible_region();

    std::string dump() const;

    void set(const android::Region &visible_region);
    const hwc_region_t get() const;
    void release();

protected:
    hwc_region_t visible_region;
};

#endif /* ifndef _HWC2_TEST_PROPERTIES_H */
