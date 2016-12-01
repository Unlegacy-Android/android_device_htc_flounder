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

#include "hwc2_test_properties.h"

hwc2_test_buffer_area::hwc2_test_buffer_area(hwc2_test_coverage_t coverage,
        int32_t display_width, int32_t display_height)
    : hwc2_test_property(buffer_areas),
      scalars((coverage == HWC2_TEST_COVERAGE_COMPLETE)? complete_scalars:
            (coverage == HWC2_TEST_COVERAGE_BASIC)? basic_scalars:
            default_scalars),
      display_width(display_width),
      display_height(display_height),
      source_crop(nullptr),
      buffer_areas()
{
    update();
}

std::string hwc2_test_buffer_area::dump() const
{
    std::stringstream dmp;
    const std::pair<int32_t, int32_t> &curr = get();
    dmp << "\tbuffer area: width " << curr.first << ", height " << curr.second
            << "\n";
    return dmp.str();
}

void hwc2_test_buffer_area::set_dependent(hwc2_test_source_crop *source_crop)
{
    this->source_crop = source_crop;
    update_dependents();
}

void hwc2_test_buffer_area::update()
{
    buffer_areas.clear();

    if (display_width == 0 && display_height == 0) {
        buffer_areas.push_back({0, 0});
        return;
    }

    for (auto scalar: scalars)
        buffer_areas.push_back({scalar * display_width, scalar * display_height});

    update_dependents();
}

void hwc2_test_buffer_area::update_dependents()
{
    const std::pair<int32_t, int32_t> &curr = get();

    if (source_crop)
        source_crop->update_buffer_area(curr.first, curr.second);
}

const std::vector<float> hwc2_test_buffer_area::default_scalars = {
    1.0f,
};

const std::vector<float> hwc2_test_buffer_area::basic_scalars = {
    1.0f, 0.5f,
};

const std::vector<float> hwc2_test_buffer_area::complete_scalars = {
    1.0f, 0.75f, 0.5f
};


hwc2_test_blend_mode::hwc2_test_blend_mode(hwc2_test_coverage_t coverage)
    : hwc2_test_property(
            (coverage == HWC2_TEST_COVERAGE_COMPLETE)? complete_blend_modes:
            (coverage == HWC2_TEST_COVERAGE_BASIC)? basic_blend_modes:
            default_blend_modes) { }

std::string hwc2_test_blend_mode::dump() const
{
    std::stringstream dmp;
    dmp << "\tblend mode: " << getBlendModeName(get()) << "\n";
    return dmp.str();
}

const std::vector<hwc2_blend_mode_t> hwc2_test_blend_mode::default_blend_modes = {
    HWC2_BLEND_MODE_NONE,
};

const std::vector<hwc2_blend_mode_t> hwc2_test_blend_mode::basic_blend_modes = {
    HWC2_BLEND_MODE_NONE,
    HWC2_BLEND_MODE_PREMULTIPLIED,
};

const std::vector<hwc2_blend_mode_t> hwc2_test_blend_mode::complete_blend_modes = {
    HWC2_BLEND_MODE_NONE,
    HWC2_BLEND_MODE_PREMULTIPLIED,
    HWC2_BLEND_MODE_COVERAGE,
};


hwc2_test_color::hwc2_test_color(hwc2_test_coverage_t coverage)
    : hwc2_test_property(
            (coverage == HWC2_TEST_COVERAGE_COMPLETE)? complete_colors:
            (coverage == HWC2_TEST_COVERAGE_BASIC)? basic_colors:
            default_colors) { }

std::string hwc2_test_color::dump() const
{
    std::stringstream dmp;
    const hwc_color_t &color = get();
    dmp << "\tcolor: r " << std::to_string(color.r) << ", g "
            << std::to_string(color.g) << ", b " << std::to_string(color.b)
            << ", a " << std::to_string(color.a) << "\n";
    return dmp.str();
}

const std::vector<hwc_color_t> hwc2_test_color::default_colors = {
    {UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX},
};

const std::vector<hwc_color_t> hwc2_test_color::basic_colors = {
    {UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX},
    {        0,         0,         0,         0},
};

const std::vector<hwc_color_t> hwc2_test_color::complete_colors = {
    {UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX},
    {UINT8_MAX, UINT8_MAX, UINT8_MAX,         0},
    {UINT8_MAX, UINT8_MAX,         0, UINT8_MAX},
    {UINT8_MAX, UINT8_MAX,         0,         0},
    {UINT8_MAX,         0, UINT8_MAX, UINT8_MAX},
    {UINT8_MAX,         0, UINT8_MAX,         0},
    {UINT8_MAX,         0,         0, UINT8_MAX},
    {UINT8_MAX,         0,         0,         0},
    {        0, UINT8_MAX, UINT8_MAX, UINT8_MAX},
    {        0, UINT8_MAX, UINT8_MAX,         0},
    {        0, UINT8_MAX,         0, UINT8_MAX},
    {        0, UINT8_MAX,         0,         0},
    {        0,         0, UINT8_MAX, UINT8_MAX},
    {        0,         0, UINT8_MAX,         0},
    {        0,         0,         0, UINT8_MAX},
    {        0,         0,         0,         0},
};


hwc2_test_composition::hwc2_test_composition(hwc2_test_coverage_t coverage)
    : hwc2_test_property(
            (coverage == HWC2_TEST_COVERAGE_COMPLETE)? complete_compositions:
            (coverage == HWC2_TEST_COVERAGE_BASIC)? basic_compositions:
            default_compositions) { }

std::string hwc2_test_composition::dump() const
{
    std::stringstream dmp;
    dmp << "\tcomposition: " << getCompositionName(get()) << "\n";
    return dmp.str();
}

const std::vector<hwc2_composition_t> hwc2_test_composition::default_compositions = {
    HWC2_COMPOSITION_DEVICE,
};

const std::vector<hwc2_composition_t> hwc2_test_composition::basic_compositions = {
    HWC2_COMPOSITION_CLIENT,
    HWC2_COMPOSITION_DEVICE,
};

const std::vector<hwc2_composition_t> hwc2_test_composition::complete_compositions = {
    HWC2_COMPOSITION_CLIENT,
    HWC2_COMPOSITION_DEVICE,
    HWC2_COMPOSITION_SOLID_COLOR,
    HWC2_COMPOSITION_CURSOR,
    HWC2_COMPOSITION_SIDEBAND,
};


hwc2_test_dataspace::hwc2_test_dataspace(hwc2_test_coverage_t coverage)
    : hwc2_test_property(
            (coverage == HWC2_TEST_COVERAGE_COMPLETE)? complete_dataspaces:
            (coverage == HWC2_TEST_COVERAGE_BASIC)? basic_dataspaces:
            default_dataspaces) { }

std::string hwc2_test_dataspace::dump() const
{
    std::stringstream dmp;
    dmp << "\tdataspace: " << get() << "\n";
    return dmp.str();
}

const std::vector<android_dataspace_t> hwc2_test_dataspace::default_dataspaces = {
    HAL_DATASPACE_UNKNOWN,
};

const std::vector<android_dataspace_t> hwc2_test_dataspace::basic_dataspaces = {
    HAL_DATASPACE_UNKNOWN,
    HAL_DATASPACE_V0_SRGB,
};

const std::vector<android_dataspace_t> hwc2_test_dataspace::complete_dataspaces = {
    HAL_DATASPACE_UNKNOWN,
    HAL_DATASPACE_ARBITRARY,
    HAL_DATASPACE_STANDARD_SHIFT,
    HAL_DATASPACE_STANDARD_MASK,
    HAL_DATASPACE_STANDARD_UNSPECIFIED,
    HAL_DATASPACE_STANDARD_BT709,
    HAL_DATASPACE_STANDARD_BT601_625,
    HAL_DATASPACE_STANDARD_BT601_625_UNADJUSTED,
    HAL_DATASPACE_STANDARD_BT601_525,
    HAL_DATASPACE_STANDARD_BT601_525_UNADJUSTED,
    HAL_DATASPACE_STANDARD_BT2020,
    HAL_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE,
    HAL_DATASPACE_STANDARD_BT470M,
    HAL_DATASPACE_STANDARD_FILM,
    HAL_DATASPACE_TRANSFER_SHIFT,
    HAL_DATASPACE_TRANSFER_MASK,
    HAL_DATASPACE_TRANSFER_UNSPECIFIED,
    HAL_DATASPACE_TRANSFER_LINEAR,
    HAL_DATASPACE_TRANSFER_SRGB,
    HAL_DATASPACE_TRANSFER_SMPTE_170M,
    HAL_DATASPACE_TRANSFER_GAMMA2_2,
    HAL_DATASPACE_TRANSFER_GAMMA2_8,
    HAL_DATASPACE_TRANSFER_ST2084,
    HAL_DATASPACE_TRANSFER_HLG,
    HAL_DATASPACE_RANGE_SHIFT,
    HAL_DATASPACE_RANGE_MASK,
    HAL_DATASPACE_RANGE_UNSPECIFIED,
    HAL_DATASPACE_RANGE_FULL,
    HAL_DATASPACE_RANGE_LIMITED,
    HAL_DATASPACE_SRGB_LINEAR,
    HAL_DATASPACE_V0_SRGB_LINEAR,
    HAL_DATASPACE_SRGB,
    HAL_DATASPACE_V0_SRGB,
    HAL_DATASPACE_JFIF,
    HAL_DATASPACE_V0_JFIF,
    HAL_DATASPACE_BT601_625,
    HAL_DATASPACE_V0_BT601_625,
    HAL_DATASPACE_BT601_525,
    HAL_DATASPACE_V0_BT601_525,
    HAL_DATASPACE_BT709,
    HAL_DATASPACE_V0_BT709,
    HAL_DATASPACE_DEPTH,
};


hwc2_test_display_frame::hwc2_test_display_frame(hwc2_test_coverage_t coverage,
        int32_t display_width, int32_t display_height)
    : hwc2_test_property(display_frames),
      frect_scalars((coverage == HWC2_TEST_COVERAGE_COMPLETE)? complete_frect_scalars:
            (coverage == HWC2_TEST_COVERAGE_BASIC)? basic_frect_scalars:
            default_frect_scalars),
      display_width(display_width),
      display_height(display_height),
      display_frames()
{
    update();
}

std::string hwc2_test_display_frame::dump() const
{
    std::stringstream dmp;
    const hwc_rect_t &display_frame = get();
    dmp << "\tdisplay frame: left " << display_frame.left << ", top "
            << display_frame.top << ", right " << display_frame.right
            << ", bottom " << display_frame.bottom << "\n";
    return dmp.str();
}

void hwc2_test_display_frame::update()
{
    display_frames.clear();

    if (display_width == 0 && display_height == 0) {
        display_frames.push_back({0, 0, 0, 0});
        return;
    }

    for (const auto &frect_scalar: frect_scalars)
        display_frames.push_back({
                static_cast<int>(frect_scalar.left * display_width),
                static_cast<int>(frect_scalar.top * display_height),
                static_cast<int>(frect_scalar.right * display_width),
                static_cast<int>(frect_scalar.bottom * display_height)});
}

const std::vector<hwc_frect_t> hwc2_test_display_frame::default_frect_scalars = {
    {0.0, 0.0, 1.0, 1.0},
};

const std::vector<hwc_frect_t> hwc2_test_display_frame::basic_frect_scalars = {
    {0.0, 0.0, 1.0, 1.0},
    {0.0, 0.0, 1.0, 0.05},
    {0.0, 0.95, 1.0, 1.0},
};

const std::vector<hwc_frect_t> hwc2_test_display_frame::complete_frect_scalars = {
    {0.0, 0.0, 1.0, 1.0},
    {0.0, 0.05, 1.0, 0.95},
    {0.0, 0.05, 1.0, 1.0},
    {0.0, 0.0, 1.0, 0.05},
    {0.0, 0.95, 1.0, 1.0},
    {0.25, 0.0, 0.75, 0.35},
    {0.25, 0.25, 0.75, 0.75},
};


hwc2_test_plane_alpha::hwc2_test_plane_alpha(hwc2_test_coverage_t coverage)
    : hwc2_test_property(
            (coverage == HWC2_TEST_COVERAGE_COMPLETE)? complete_plane_alphas:
            (coverage == HWC2_TEST_COVERAGE_BASIC)? basic_plane_alphas:
            default_plane_alphas) { }

std::string hwc2_test_plane_alpha::dump() const
{
    std::stringstream dmp;
    dmp << "\tplane alpha: " << get() << "\n";
    return dmp.str();
}

const std::vector<float> hwc2_test_plane_alpha::default_plane_alphas = {
    1.0f,
};

const std::vector<float> hwc2_test_plane_alpha::basic_plane_alphas = {
    1.0f, 0.5f,
};

const std::vector<float> hwc2_test_plane_alpha::complete_plane_alphas = {
    1.0f, 0.75f, 0.5f, 0.25f, 0.0f,
};


hwc2_test_source_crop::hwc2_test_source_crop(hwc2_test_coverage_t coverage,
        float buffer_width, float buffer_height)
    : hwc2_test_property(source_crops),
      frect_scalars((coverage == HWC2_TEST_COVERAGE_COMPLETE)? complete_frect_scalars:
            (coverage == HWC2_TEST_COVERAGE_BASIC)? basic_frect_scalars:
            default_frect_scalars),
      buffer_width(buffer_width),
      buffer_height(buffer_height),
      source_crops()
{
    update();
}

std::string hwc2_test_source_crop::dump() const
{
    std::stringstream dmp;
    const hwc_frect_t &source_crop = get();
    dmp << "\tsource crop: left " << source_crop.left << ", top "
            << source_crop.top << ", right " << source_crop.right << ", bottom "
            << source_crop.bottom << "\n";
    return dmp.str();
}

void hwc2_test_source_crop::update_buffer_area(float buffer_width,
        float buffer_height)
{
    this->buffer_width = buffer_width;
    this->buffer_height = buffer_height;
    update();
}

void hwc2_test_source_crop::update()
{
    source_crops.clear();

    if (buffer_width == 0 && buffer_height == 0) {
        source_crops.push_back({0, 0, 0, 0});
        return;
    }

    for (const auto &frect_scalar: frect_scalars)
        source_crops.push_back({
                frect_scalar.left * buffer_width,
                frect_scalar.top * buffer_height,
                frect_scalar.right * buffer_width,
                frect_scalar.bottom * buffer_height});
}

const std::vector<hwc_frect_t> hwc2_test_source_crop::default_frect_scalars = {
    {0.0, 0.0, 1.0, 1.0},
};

const std::vector<hwc_frect_t> hwc2_test_source_crop::basic_frect_scalars = {
    {0.0, 0.0, 1.0, 1.0},
    {0.0, 0.0, 0.5, 0.5},
    {0.5, 0.5, 1.0, 1.0},
};

const std::vector<hwc_frect_t> hwc2_test_source_crop::complete_frect_scalars = {
    {0.0, 0.0, 1.0, 1.0},
    {0.0, 0.0, 0.5, 0.5},
    {0.5, 0.5, 1.0, 1.0},
    {0.0, 0.0, 0.25, 0.25},
    {0.25, 0.25, 0.75, 0.75},
};


hwc2_test_transform::hwc2_test_transform(hwc2_test_coverage_t coverage)
    : hwc2_test_property(
            (coverage == HWC2_TEST_COVERAGE_COMPLETE)? complete_transforms:
            (coverage == HWC2_TEST_COVERAGE_BASIC)? basic_transforms:
            default_transforms) { }

std::string hwc2_test_transform::dump() const
{
    std::stringstream dmp;
    dmp << "\ttransform: " << getTransformName(get()) << "\n";
    return dmp.str();
}

const std::vector<hwc_transform_t> hwc2_test_transform::default_transforms = {
    static_cast<hwc_transform_t>(0),
};

const std::vector<hwc_transform_t> hwc2_test_transform::basic_transforms = {
    static_cast<hwc_transform_t>(0),
    HWC_TRANSFORM_FLIP_H,
    HWC_TRANSFORM_ROT_90,
};

const std::vector<hwc_transform_t> hwc2_test_transform::complete_transforms = {
    static_cast<hwc_transform_t>(0),
    HWC_TRANSFORM_FLIP_H,
    HWC_TRANSFORM_FLIP_V,
    HWC_TRANSFORM_ROT_90,
    HWC_TRANSFORM_ROT_180,
    HWC_TRANSFORM_ROT_270,
    HWC_TRANSFORM_FLIP_H_ROT_90,
    HWC_TRANSFORM_FLIP_V_ROT_90,
};
