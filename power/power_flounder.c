/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cutils/properties.h>
//#define LOG_NDEBUG 0

#define LOG_TAG "FlounderPowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

#define SCALING_GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
#define BOOSTPULSE_INTERACTIVE "/sys/devices/system/cpu/cpufreq/interactive/boostpulse"
#define CPU_MAX_FREQ_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq"
#define FACEDOWN_PATH "/sys/class/htc_sensorhub/sensor_hub/facedown_enabled"
#define TOUCH_SYNA_INTERACTIVE_PATH "/sys/devices/platform/spi-tegra114.2/spi_master/spi2/spi2.0/input/input0/interactive"
#define WAKE_GESTURE_PATH "/sys/devices/platform/spi-tegra114.2/spi_master/spi2/spi2.0/input/input0/wake_gesture"
#define IO_IS_BUSY_PATH "/sys/devices/system/cpu/cpufreq/interactive/io_is_busy"
#define LOW_POWER_MAX_FREQ "1020000"
#define NORMAL_MAX_FREQ "2295000"

struct flounder_power_module {
    struct power_module base;
    pthread_mutex_t lock;
    int boostpulse_fd;
    int boostpulse_warned;
};

static char governor[20];

static bool low_power_mode = false;
static char *max_cpu_freq = NORMAL_MAX_FREQ;
static char *low_power_max_cpu_freq = LOW_POWER_MAX_FREQ;

static int sysfs_read(char *path, char *s, int num_bytes)
{
    char buf[80];
    int count;
    int ret = 0;
    int fd = open(path, O_RDONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);

        return -1;
    }

    if ((count = read(fd, s, num_bytes - 1)) < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);

        ret = -1;
    } else {
        s[count] = '\0';
    }

    close(fd);

    return ret;
}

static void sysfs_write(const char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
    }
    close(fd);
}

static int get_scaling_governor() {
    if (sysfs_read(SCALING_GOVERNOR_PATH, governor,
                sizeof(governor)) == -1) {
        return -1;
    } else {
        // Strip newline at the end.
        int len = strlen(governor);

        len--;

        while (len >= 0 && (governor[len] == '\n' || governor[len] == '\r'))
            governor[len--] = '\0';
    }

    return 0;
}

static void configure_governor()
{
    if (strncmp(governor, "interactive", 11) == 0) {
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/timer_rate", "20000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/timer_slack", "20000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/min_sample_time", "80000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/hispeed_freq", "1530000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/go_hispeed_load", "99");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/target_loads", "65 228000:75 624000:85");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/above_hispeed_delay", "20000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/boostpulse_duration", "1000000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/io_is_busy", "0");
    }
}

static void power_set_interactive(struct power_module __unused *module, int on)
{
   ALOGV("power_set_interactive: %d\n", on);

     /* Lower maximum frequency when screen is off.*/

    sysfs_write(CPU_MAX_FREQ_PATH, (!on || low_power_mode) ? low_power_max_cpu_freq : max_cpu_freq);
    sysfs_write(FACEDOWN_PATH, on ? "0" : "1");
    sysfs_write(TOUCH_SYNA_INTERACTIVE_PATH, on ? "1" : "0");
    ALOGV("power_set_interactive: %d done\n", on); 
}

static int boostpulse_open(struct flounder_power_module *flounder)
{
    char buf[80];
    int len;

    pthread_mutex_lock(&flounder->lock);

    if (flounder->boostpulse_fd < 0) {
        if (get_scaling_governor() < 0) {
            ALOGE("Can't read scaling governor.");
            flounder->boostpulse_warned = 1;
        } else {
            if (strncmp(governor, "interactive", 11) == 0)
                flounder->boostpulse_fd = open(BOOSTPULSE_INTERACTIVE, O_WRONLY);

            if (flounder->boostpulse_fd < 0 && !flounder->boostpulse_warned) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("Error opening boostpulse: %s\n", buf);
                flounder->boostpulse_warned = 1;
            } else if (flounder->boostpulse_fd > 0) {
                ALOGD("Opened %s boostpulse interface", governor);

            }
        }
    }


    pthread_mutex_unlock(&flounder->lock);
    return flounder->boostpulse_fd;
}

static void set_feature(struct power_module *module, feature_t feature, int state)
{
    struct flounder_power_module *flounder =
            (struct flounder_power_module *) module;
    switch (feature) {
    case POWER_FEATURE_DOUBLE_TAP_TO_WAKE:
        pthread_mutex_lock(&flounder->lock);
        sysfs_write(WAKE_GESTURE_PATH, state ? "1" : "0");
        pthread_mutex_unlock(&flounder->lock);
        break;
    default:
        ALOGW("Error setting the feature, it doesn't exist %d\n", feature);
        break;
    }
}

static void flounder_power_hint(struct power_module *module, power_hint_t hint,
                                void *data)
{
    struct flounder_power_module *flounder =
            (struct flounder_power_module *) module;
    char buf[80];
    int len;

    switch (hint) {
    case POWER_HINT_INTERACTION:
        if (boostpulse_open(flounder) >= 0) {
            len = write(flounder->boostpulse_fd, "1", 1);

            if (len < 0) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("Error writing to boostpulse: %s\n", buf);

                pthread_mutex_lock(&flounder->lock);
                close(flounder->boostpulse_fd);
                flounder->boostpulse_fd = -1;
                flounder->boostpulse_warned = 0;
                pthread_mutex_unlock(&flounder->lock);
            }
        }
        break;

   case POWER_HINT_VSYNC:
        break;

    case POWER_HINT_LOW_POWER:
        pthread_mutex_lock(&flounder->lock);
        if (data) {
            sysfs_write(CPU_MAX_FREQ_PATH, low_power_max_cpu_freq);
        } else {
            sysfs_write(CPU_MAX_FREQ_PATH, max_cpu_freq);
        }
        low_power_mode = data;
        pthread_mutex_unlock(&flounder->lock);
        break;

    default:
            break;
    }
}

static void power_init(struct power_module __unused *module)
{
    get_scaling_governor();
    configure_governor();
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct flounder_power_module HAL_MODULE_INFO_SYM = {
    base: {
        common: {
            tag: HARDWARE_MODULE_TAG,
            module_api_version: POWER_MODULE_API_VERSION_0_2,
            hal_api_version: HARDWARE_HAL_API_VERSION,
            id: POWER_HARDWARE_MODULE_ID,
            name: "Flounder Power HAL",
            author: "The Android Open Source Project",
            methods: &power_module_methods,
        },

        init: power_init,
        setInteractive: power_set_interactive,
        powerHint: flounder_power_hint,
        setFeature: set_feature,
    },

    lock: PTHREAD_MUTEX_INITIALIZER,
    boostpulse_fd: -1,
    boostpulse_warned: 0,
};

