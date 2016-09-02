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

#include <vector>

#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11

typedef enum {
    HWC2_TEST_COVERAGE_DEFAULT = 0,
    HWC2_TEST_COVERAGE_BASIC,
    HWC2_TEST_COVERAGE_COMPLETE,
    HWC2_TEST_NUM_COVERAGE_TYPES,
} hwc2_test_coverage_t;


template <class T>
class hwc2_test_property {
public:
    hwc2_test_property(const std::vector<T> &list)
        : list(list),
          list_idx(0) { }

    virtual ~hwc2_test_property() { };

    virtual void reset()
    {
        list_idx = 0;
    }

    virtual bool advance()
    {
        if (list_idx + 1 < list.size()) {
            list_idx++;
            return true;
        }
        reset();
        return false;
    }

    virtual T get() const
    {
        return list.at(list_idx);
    }

    virtual std::string dump() const = 0;

protected:
    const std::vector<T> &list;
    uint32_t list_idx;
};


class hwc2_test_composition : public hwc2_test_property<hwc2_composition_t> {
public:
    hwc2_test_composition(hwc2_test_coverage_t coverage);

    std::string dump() const;

protected:
    static const std::vector<hwc2_composition_t> default_compositions;
    static const std::vector<hwc2_composition_t> basic_compositions;
    static const std::vector<hwc2_composition_t> complete_compositions;
};

#endif /* ifndef _HWC2_TEST_PROPERTIES_H */
