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

hwc2_test_layer::hwc2_test_layer(hwc2_test_coverage_t coverage)
    : blend_mode(coverage),
      composition(coverage),
      dataspace(coverage) { }

std::string hwc2_test_layer::dump() const
{
    std::stringstream dmp;

    dmp << "layer: \n";

    for (auto property: properties)
        dmp << property->dump();

    return dmp.str();
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

hwc2_composition_t hwc2_test_layer::get_composition() const
{
    return composition.get();
}

android_dataspace_t hwc2_test_layer::get_dataspace() const
{
    return dataspace.get();
}

bool hwc2_test_layer::advance_blend_mode()
{
    return blend_mode.advance();
}

bool hwc2_test_layer::advance_composition()
{
    return composition.advance();
}

bool hwc2_test_layer::advance_dataspace()
{
    return dataspace.advance();
}
