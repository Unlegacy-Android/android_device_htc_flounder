/* * Copyright (C) 2016 The Android Open Source Project
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

#include "hwc2_test_layers.h"

hwc2_test_layers::hwc2_test_layers(const std::vector<hwc2_layer_t> &layers,
        hwc2_test_coverage_t coverage)
    : test_layers()
{
    uint32_t next_z_order = 0;

    for (auto layer: layers)
        test_layers.emplace(std::piecewise_construct,
                std::forward_as_tuple(layer), std::forward_as_tuple(coverage,
                next_z_order++));
}

std::string hwc2_test_layers::dump() const
{
    std::stringstream dmp;
    for (auto &test_layer: test_layers)
        dmp << test_layer.second.dump();
    return dmp.str();
}

void hwc2_test_layers::reset()
{
    for (auto &test_layer: test_layers)
        test_layer.second.reset();
}

uint32_t hwc2_test_layers::get_z_order(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_z_order();
}
