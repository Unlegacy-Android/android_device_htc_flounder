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
