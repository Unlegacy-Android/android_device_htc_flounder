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
    : composition(coverage) { }

std::string hwc2_test_layer::dump() const
{
    std::stringstream dmp;

    dmp << "layer: \n";
    dmp << composition.dump();

    return dmp.str();
}

void hwc2_test_layer::reset()
{
    composition.reset();
}

hwc2_composition_t hwc2_test_layer::get_composition() const
{
    return composition.get();
}

bool hwc2_test_layer::advance_composition()
{
    return composition.advance();
}
