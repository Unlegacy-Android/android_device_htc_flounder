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

#define LOG_TAG "audio_hw_primary"
/*#define LOG_NDEBUG 0*/
/*#define VERY_VERY_VERBOSE_LOGGING*/
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <math.h>
#include <dlfcn.h>
#include <sys/resource.h>
#include <sys/prctl.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include <cutils/atomic.h>
#include <cutils/sched_policy.h>

#include <hardware/audio_effect.h>
#include <system/thread_defs.h>
#include <audio_effects/effect_aec.h>
#include <audio_effects/effect_ns.h>
#include "audio_hw.h"

#include "sound/compress_params.h"

/* TODO: the following PCM device profiles could be read from a config file */
struct pcm_device_profile pcm_device_playback_hs = {
    .config = {
        .channels = PLAYBACK_DEFAULT_CHANNEL_COUNT,
        .rate = PLAYBACK_DEFAULT_SAMPLING_RATE,
        .period_size = PLAYBACK_PERIOD_SIZE,
        .period_count = PLAYBACK_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = PLAYBACK_START_THRESHOLD,
        .stop_threshold = PLAYBACK_STOP_THRESHOLD,
        .silence_threshold = 0,
        .avail_min = PLAYBACK_AVAILABLE_MIN,
    },
    .card = 1,
    .id = 0,
    .type = PCM_PLAYBACK,
    .devices = AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_WIRED_HEADPHONE,
};

struct pcm_device_profile pcm_device_capture = {
    .config = {
        .channels = CAPTURE_DEFAULT_CHANNEL_COUNT,
        .rate = CAPTURE_DEFAULT_SAMPLING_RATE,
        .period_size = CAPTURE_PERIOD_SIZE,
        .period_count = CAPTURE_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = CAPTURE_START_THRESHOLD,
        .stop_threshold = 0,
        .silence_threshold = 0,
        .avail_min = 0,
    },
    .card = 1,
    .id = 0,
    .type = PCM_CAPTURE,
    .devices = AUDIO_DEVICE_IN_BUILTIN_MIC|AUDIO_DEVICE_IN_WIRED_HEADSET|AUDIO_DEVICE_IN_BACK_MIC,
};

struct pcm_device_profile pcm_device_playback_spk = {
    .config = {
        .channels = PLAYBACK_DEFAULT_CHANNEL_COUNT,
        .rate = PLAYBACK_DEFAULT_SAMPLING_RATE,
        .period_size = PLAYBACK_PERIOD_SIZE,
        .period_count = PLAYBACK_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = PLAYBACK_START_THRESHOLD,
        .stop_threshold = PLAYBACK_STOP_THRESHOLD,
        .silence_threshold = 0,
        .avail_min = PLAYBACK_AVAILABLE_MIN,
    },
    .card = 1,
    .id = 1,
    .type = PCM_PLAYBACK,
    .devices = AUDIO_DEVICE_OUT_SPEAKER,
};

struct pcm_device_profile pcm_device_playback_sco = {
    .config = {
        .channels = SCO_DEFAULT_CHANNEL_COUNT,
        .rate = SCO_DEFAULT_SAMPLING_RATE,
        .period_size = SCO_PERIOD_SIZE,
        .period_count = SCO_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = SCO_START_THRESHOLD,
        .stop_threshold = SCO_STOP_THRESHOLD,
        .silence_threshold = 0,
        .avail_min = SCO_AVAILABLE_MIN,
    },
    .card = 1,
    .id = 2,
    .type = PCM_PLAYBACK,
    .devices =
            AUDIO_DEVICE_OUT_BLUETOOTH_SCO|AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET|
            AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT,
};

struct pcm_device_profile pcm_device_capture_sco = {
    .config = {
        .channels = SCO_DEFAULT_CHANNEL_COUNT,
        .rate = SCO_DEFAULT_SAMPLING_RATE,
        .period_size = SCO_PERIOD_SIZE,
        .period_count = SCO_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = CAPTURE_START_THRESHOLD,
        .stop_threshold = 0,
        .silence_threshold = 0,
        .avail_min = 0,
    },
    .card = 1,
    .id = 2,
    .type = PCM_CAPTURE,
    .devices = AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET,
};

struct pcm_device_profile pcm_device_playback_hdmi = {
    .config = {
        .channels = PLAYBACK_HDMI_MULTI_DEFAULT_CHANNEL_COUNT,
        .rate = PLAYBACK_DEFAULT_SAMPLING_RATE,
        .period_size = PLAYBACK_HDMI_MULTI_PERIOD_SIZE,
        .period_count = PLAYBACK_HDMI_MULTI_PERIOD_COUNT,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = PLAYBACK_HDMI_MULTI_START_THRESHOLD,
        .stop_threshold = PLAYBACK_HDMI_MULTI_STOP_THRESHOLD,
        .silence_threshold = 0,
        .avail_min = PLAYBACK_HDMI_MULTI_AVAILABLE_MIN,
    },
    .card = 0,
    .id = 3,
    .type = PCM_PLAYBACK,
    .devices = AUDIO_DEVICE_OUT_AUX_DIGITAL,
};


struct pcm_device_profile *pcm_devices[] = {
    &pcm_device_playback_hs,
    &pcm_device_capture,
    &pcm_device_playback_spk,
    &pcm_device_playback_sco,
    &pcm_device_capture_sco,
    &pcm_device_playback_hdmi,
    NULL,
};

static const char * const use_case_table[AUDIO_USECASE_MAX] = {
    [USECASE_AUDIO_PLAYBACK] = "playback",
    [USECASE_AUDIO_PLAYBACK_MULTI_CH] = "playback multi-channel",
    [USECASE_AUDIO_PLAYBACK_OFFLOAD] = "compress-offload-playback",
    [USECASE_AUDIO_CAPTURE] = "capture",
    [USECASE_VOICE_CALL] = "voice-call",
};


#define STRING_TO_ENUM(string) { #string, string }

static unsigned int audio_device_ref_count;

struct pcm_config pcm_config_deep_buffer = {
	.channels = 2,
	.rate = DEEP_BUFFER_OUTPUT_SAMPLING_RATE,
	.period_size = DEEP_BUFFER_OUTPUT_PERIOD_SIZE,
	.period_count = DEEP_BUFFER_OUTPUT_PERIOD_COUNT,
	.format = PCM_FORMAT_S16_LE,
	.start_threshold = DEEP_BUFFER_OUTPUT_PERIOD_SIZE / 4,
	.stop_threshold = INT_MAX,
	.avail_min = DEEP_BUFFER_OUTPUT_PERIOD_SIZE / 4,
};

struct string_to_enum {
    const char *name;
    uint32_t value;
};

static const struct string_to_enum out_channels_name_to_enum_table[] = {
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_STEREO),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_5POINT1),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_7POINT1),
};


static bool is_supported_format(audio_format_t format)
{
    if (format == AUDIO_FORMAT_MP3 ||
            format == AUDIO_FORMAT_AAC)
        return true;

    return false;
}

static int get_snd_codec_id(audio_format_t format)
{
    int id = 0;

    switch (format) {
    case AUDIO_FORMAT_MP3:
        id = SND_AUDIOCODEC_MP3;
        break;
    case AUDIO_FORMAT_AAC:
        id = SND_AUDIOCODEC_AAC;
        break;
    default:
        ALOGE("%s: Unsupported audio format", __func__);
    }

    return id;
}

/* Array to store sound devices */
static const char * const device_table[SND_DEVICE_MAX] = {
    [SND_DEVICE_NONE] = "none",
    /* Playback sound devices */
    [SND_DEVICE_OUT_HANDSET] = "handset",
    [SND_DEVICE_OUT_SPEAKER] = "speaker",
    [SND_DEVICE_OUT_SPEAKER_REVERSE] = "speaker-reverse",
    [SND_DEVICE_OUT_HEADPHONES] = "headphones",
    [SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES] = "speaker-and-headphones",
    [SND_DEVICE_OUT_VOICE_HANDSET] = "voice-handset",
    [SND_DEVICE_OUT_VOICE_SPEAKER] = "voice-speaker",
    [SND_DEVICE_OUT_VOICE_HEADPHONES] = "voice-headphones",
    [SND_DEVICE_OUT_HDMI] = "hdmi",
    [SND_DEVICE_OUT_SPEAKER_AND_HDMI] = "speaker-and-hdmi",
    [SND_DEVICE_OUT_BT_SCO] = "bt-sco-headset",
    [SND_DEVICE_OUT_VOICE_TTY_FULL_HEADPHONES] = "voice-tty-full-headphones",
    [SND_DEVICE_OUT_VOICE_TTY_VCO_HEADPHONES] = "voice-tty-vco-headphones",
    [SND_DEVICE_OUT_VOICE_TTY_HCO_HANDSET] = "voice-tty-hco-handset",

    /* Capture sound devices */
    [SND_DEVICE_IN_HANDSET_MIC] = "handset-mic",
    [SND_DEVICE_IN_SPEAKER_MIC] = "speaker-mic",
    [SND_DEVICE_IN_HEADSET_MIC] = "headset-mic",
    [SND_DEVICE_IN_HANDSET_MIC_AEC] = "handset-mic",
    [SND_DEVICE_IN_SPEAKER_MIC_AEC] = "voice-speaker-mic",
    [SND_DEVICE_IN_HEADSET_MIC_AEC] = "headset-mic",
    [SND_DEVICE_IN_VOICE_SPEAKER_MIC] = "voice-speaker-mic",
    [SND_DEVICE_IN_VOICE_HEADSET_MIC] = "voice-headset-mic",
    [SND_DEVICE_IN_HDMI_MIC] = "hdmi-mic",
    [SND_DEVICE_IN_BT_SCO_MIC] = "bt-sco-mic",
    [SND_DEVICE_IN_CAMCORDER_MIC] = "camcorder-mic",
    [SND_DEVICE_IN_VOICE_DMIC_1] = "voice-dmic-1",
    [SND_DEVICE_IN_VOICE_SPEAKER_DMIC_1] = "voice-speaker-dmic-1",
    [SND_DEVICE_IN_VOICE_TTY_FULL_HEADSET_MIC] = "voice-tty-full-headset-mic",
    [SND_DEVICE_IN_VOICE_TTY_VCO_HANDSET_MIC] = "voice-tty-vco-handset-mic",
    [SND_DEVICE_IN_VOICE_TTY_HCO_HEADSET_MIC] = "voice-tty-hco-headset-mic",
    [SND_DEVICE_IN_VOICE_REC_MIC] = "voice-rec-mic",
    [SND_DEVICE_IN_VOICE_REC_DMIC_1] = "voice-rec-dmic-1",
    [SND_DEVICE_IN_VOICE_REC_DMIC_NS_1] = "voice-rec-dmic-ns-1",
};

struct mixer_card *adev_get_mixer_for_card(struct audio_device *adev, int card)
{
    struct mixer_card *mixer_card;
    struct listnode *node;

    list_for_each(node, &adev->mixer_list) {
        mixer_card = node_to_item(node, struct mixer_card, adev_list_node);
        if (mixer_card->card == card)
            return mixer_card;
    }
    return NULL;
}

struct mixer_card *uc_get_mixer_for_card(struct audio_usecase *usecase, int card)
{
    struct mixer_card *mixer_card;
    struct listnode *node;

    list_for_each(node, &usecase->mixer_list) {
        mixer_card = node_to_item(node, struct mixer_card, uc_list_node[usecase->id]);
        if (mixer_card->card == card)
            return mixer_card;
    }
    return NULL;
}

void free_mixer_list(struct audio_device *adev)
{
    struct mixer_card *mixer_card;
    struct listnode *node;
    struct listnode *next;

    list_for_each_safe(node, next, &adev->mixer_list) {
        mixer_card = node_to_item(node, struct mixer_card, adev_list_node);
        list_remove(node);
        audio_route_free(mixer_card->audio_route);
        free(mixer_card);
    }
}

int mixer_init(struct audio_device *adev)
{
    int i;
    int card;
    int retry_num;
    struct mixer *mixer;
    struct audio_route *audio_route;
    char mixer_path[PATH_MAX];
    struct mixer_card *mixer_card;
    struct listnode *node;

    list_init(&adev->mixer_list);

    for (i = 0; pcm_devices[i] != NULL; i++) {
        card = pcm_devices[i]->card;
        if (adev_get_mixer_for_card(adev, card) == NULL) {
            retry_num = 0;
            do {
                mixer = mixer_open(card);
                if (mixer == NULL) {
                    if (++retry_num > RETRY_NUMBER) {
                        ALOGE("%s unable to open the mixer for card %d, aborting.",
                              __func__, card);
                        goto error;
                    }
                    usleep(RETRY_US);
                }
            } while (mixer == NULL);

            sprintf(mixer_path, "/system/etc/mixer_paths_%d.xml", card);
            audio_route = audio_route_init(card, mixer_path);
            if (!audio_route) {
                ALOGE("%s: Failed to init audio route controls for card %d, aborting.",
                      __func__, card);
                goto error;
            }
            mixer_card = calloc(1, sizeof(struct mixer_card));
            mixer_card->card = card;
            mixer_card->mixer = mixer;
            mixer_card->audio_route = audio_route;
            list_add_tail(&adev->mixer_list, &mixer_card->adev_list_node);
        }
    }

    return 0;

error:
    free_mixer_list(adev);
    return -ENODEV;
}

const char *get_snd_device_name(snd_device_t snd_device)
{
    const char *name = NULL;

    if (snd_device >= SND_DEVICE_MIN && snd_device < SND_DEVICE_MAX)
        name = device_table[snd_device];

    ALOGE_IF(name == NULL, "%s: invalid snd device %d", __func__, snd_device);

   return name;
}

const char *get_snd_device_display_name(snd_device_t snd_device)
{
    const char *name = get_snd_device_name(snd_device);

    if (name == NULL)
        name = "SND DEVICE NOT FOUND";

    return name;
}

struct pcm_device_profile *get_pcm_device(usecase_type_t uc_type, audio_devices_t devices)
{
    int i;

    devices &= ~AUDIO_DEVICE_BIT_IN;
    for (i = 0; pcm_devices[i] != NULL; i++) {
        if ((pcm_devices[i]->type == uc_type) &&
                (devices & pcm_devices[i]->devices))
            break;
    }
    return pcm_devices[i];
}

static struct audio_usecase *get_usecase_from_id(struct audio_device *adev,
                                                   audio_usecase_t uc_id)
{
    struct audio_usecase *usecase;
    struct listnode *node;

    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->id == uc_id)
            return usecase;
    }
    return NULL;
}

static struct audio_usecase *get_usecase_from_type(struct audio_device *adev,
                                                        usecase_type_t type)
{
    struct audio_usecase *usecase;
    struct listnode *node;

    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->type & type)
            return usecase;
    }
    return NULL;
}

/* always called with adev lock held */
static int set_voice_volume_l(struct audio_device *adev, float volume)
{
    int err = 0;
    (void)volume;

    if (adev->mode == AUDIO_MODE_IN_CALL) {
        /* TODO */
    }
    return err;
}


snd_device_t get_output_snd_device(struct audio_device *adev, audio_devices_t devices)
{

    audio_mode_t mode = adev->mode;
    snd_device_t snd_device = SND_DEVICE_NONE;

    ALOGV("%s: enter: output devices(%#x)", __func__, devices);
    if (devices == AUDIO_DEVICE_NONE ||
        devices & AUDIO_DEVICE_BIT_IN) {
        ALOGV("%s: Invalid output devices (%#x)", __func__, devices);
        goto exit;
    }

    if (mode == AUDIO_MODE_IN_CALL) {
        if (devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
            devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            if (adev->tty_mode == TTY_MODE_FULL)
                snd_device = SND_DEVICE_OUT_VOICE_TTY_FULL_HEADPHONES;
            else if (adev->tty_mode == TTY_MODE_VCO)
                snd_device = SND_DEVICE_OUT_VOICE_TTY_VCO_HEADPHONES;
            else if (adev->tty_mode == TTY_MODE_HCO)
                snd_device = SND_DEVICE_OUT_VOICE_TTY_HCO_HANDSET;
            else
                snd_device = SND_DEVICE_OUT_VOICE_HEADPHONES;
        } else if (devices & AUDIO_DEVICE_OUT_ALL_SCO) {
            snd_device = SND_DEVICE_OUT_BT_SCO;
        } else if (devices & AUDIO_DEVICE_OUT_SPEAKER) {
            snd_device = SND_DEVICE_OUT_VOICE_SPEAKER;
        } else if (devices & AUDIO_DEVICE_OUT_EARPIECE) {
            snd_device = SND_DEVICE_OUT_HANDSET;
        }
        if (snd_device != SND_DEVICE_NONE) {
            goto exit;
        }
    }

    if (popcount(devices) == 2) {
        if (devices == (AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
                        AUDIO_DEVICE_OUT_SPEAKER)) {
            snd_device = SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES;
        } else if (devices == (AUDIO_DEVICE_OUT_WIRED_HEADSET |
                               AUDIO_DEVICE_OUT_SPEAKER)) {
            snd_device = SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES;
        } else if (devices == (AUDIO_DEVICE_OUT_AUX_DIGITAL |
                               AUDIO_DEVICE_OUT_SPEAKER)) {
            snd_device = SND_DEVICE_OUT_SPEAKER_AND_HDMI;
        } else {
            ALOGE("%s: Invalid combo device(%#x)", __func__, devices);
            goto exit;
        }
        if (snd_device != SND_DEVICE_NONE) {
            goto exit;
        }
    }

    if (popcount(devices) != 1) {
        ALOGE("%s: Invalid output devices(%#x)", __func__, devices);
        goto exit;
    }

    if (devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
        devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
        snd_device = SND_DEVICE_OUT_HEADPHONES;
    } else if (devices & AUDIO_DEVICE_OUT_SPEAKER) {
        if (adev->speaker_lr_swap)
            snd_device = SND_DEVICE_OUT_SPEAKER_REVERSE;
        else
            snd_device = SND_DEVICE_OUT_SPEAKER;
    } else if (devices & AUDIO_DEVICE_OUT_ALL_SCO) {
        snd_device = SND_DEVICE_OUT_BT_SCO;
    } else if (devices & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
        snd_device = SND_DEVICE_OUT_HDMI ;
    } else if (devices & AUDIO_DEVICE_OUT_EARPIECE) {
        snd_device = SND_DEVICE_OUT_HANDSET;
    } else {
        ALOGE("%s: Unknown device(s) %#x", __func__, devices);
    }
exit:
    ALOGV("%s: exit: snd_device(%s)", __func__, device_table[snd_device]);
    return snd_device;
}

snd_device_t get_input_snd_device(struct audio_device *adev, audio_devices_t out_device)
{
    audio_source_t  source;
    audio_mode_t    mode   = adev->mode;
    audio_devices_t in_device;
    audio_channel_mask_t channel_mask;
    snd_device_t snd_device = SND_DEVICE_NONE;
    struct stream_in *active_input = NULL;
    struct audio_usecase *usecase;

    usecase = get_usecase_from_type(adev, PCM_CAPTURE|VOICE_CALL);
    if (usecase != NULL) {
        active_input = (struct stream_in *)usecase->stream;
    }
    source = (active_input == NULL) ?
                                AUDIO_SOURCE_DEFAULT : active_input->source;

    in_device = ((active_input == NULL) ?
                                    AUDIO_DEVICE_NONE : active_input->devices)
                                & ~AUDIO_DEVICE_BIT_IN;
    channel_mask = (active_input == NULL) ?
                                AUDIO_CHANNEL_IN_MONO : active_input->channel_mask;

    ALOGV("%s: enter: out_device(%#x) in_device(%#x)",
          __func__, out_device, in_device);
    if (mode == AUDIO_MODE_IN_CALL) {
        if (out_device == AUDIO_DEVICE_NONE) {
            ALOGE("%s: No output device set for voice call", __func__);
            goto exit;
        }
        if (adev->tty_mode != TTY_MODE_OFF) {
            if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                switch (adev->tty_mode) {
                case TTY_MODE_FULL:
                    snd_device = SND_DEVICE_IN_VOICE_TTY_FULL_HEADSET_MIC;
                    break;
                case TTY_MODE_VCO:
                    snd_device = SND_DEVICE_IN_VOICE_TTY_VCO_HANDSET_MIC;
                    break;
                case TTY_MODE_HCO:
                    snd_device = SND_DEVICE_IN_VOICE_TTY_HCO_HEADSET_MIC;
                    break;
                default:
                    ALOGE("%s: Invalid TTY mode (%#x)", __func__, adev->tty_mode);
                }
                goto exit;
            }
        }
        if (out_device & AUDIO_DEVICE_OUT_EARPIECE ||
                out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
            snd_device = SND_DEVICE_IN_HANDSET_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_VOICE_HEADSET_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_ALL_SCO) {
            snd_device = SND_DEVICE_IN_BT_SCO_MIC ;
        } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
            snd_device = SND_DEVICE_IN_VOICE_SPEAKER_MIC;
        }
    } else if (source == AUDIO_SOURCE_CAMCORDER) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC ||
            in_device & AUDIO_DEVICE_IN_BACK_MIC) {
            snd_device = SND_DEVICE_IN_CAMCORDER_MIC;
        }
    } else if (source == AUDIO_SOURCE_VOICE_RECOGNITION) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
            if (adev->dualmic_config == DUALMIC_CONFIG_1) {
                if (channel_mask == AUDIO_CHANNEL_IN_FRONT_BACK)
                    snd_device = SND_DEVICE_IN_VOICE_REC_DMIC_1;
                else if (adev->ns_in_voice_rec)
                    snd_device = SND_DEVICE_IN_VOICE_REC_DMIC_NS_1;
            }

            if (snd_device == SND_DEVICE_NONE) {
                snd_device = SND_DEVICE_IN_VOICE_REC_MIC;
            }
        }
    } else if (source == AUDIO_SOURCE_VOICE_COMMUNICATION) {
        if (out_device & AUDIO_DEVICE_OUT_SPEAKER)
            in_device = AUDIO_DEVICE_IN_BACK_MIC;
        if (active_input) {
            if (active_input->enable_aec) {
                if (in_device & AUDIO_DEVICE_IN_BACK_MIC) {
                    snd_device = SND_DEVICE_IN_SPEAKER_MIC_AEC;
                } else if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
                    snd_device = SND_DEVICE_IN_HANDSET_MIC_AEC;
                } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
                    snd_device = SND_DEVICE_IN_HEADSET_MIC_AEC;
                }
            }
            /* TODO: set echo reference */
        }
    } else if (source == AUDIO_SOURCE_DEFAULT) {
        goto exit;
    }


    if (snd_device != SND_DEVICE_NONE) {
        goto exit;
    }

    if (in_device != AUDIO_DEVICE_NONE &&
            !(in_device & AUDIO_DEVICE_IN_VOICE_CALL) &&
            !(in_device & AUDIO_DEVICE_IN_COMMUNICATION)) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
            snd_device = SND_DEVICE_IN_HANDSET_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_BACK_MIC) {
            snd_device = SND_DEVICE_IN_SPEAKER_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_HEADSET_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
            snd_device = SND_DEVICE_IN_BT_SCO_MIC ;
        } else if (in_device & AUDIO_DEVICE_IN_AUX_DIGITAL) {
            snd_device = SND_DEVICE_IN_HDMI_MIC;
        } else {
            ALOGE("%s: Unknown input device(s) %#x", __func__, in_device);
            ALOGW("%s: Using default handset-mic", __func__);
            snd_device = SND_DEVICE_IN_HANDSET_MIC;
        }
    } else {
        if (out_device & AUDIO_DEVICE_OUT_EARPIECE) {
            snd_device = SND_DEVICE_IN_HANDSET_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_HEADSET_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
            snd_device = SND_DEVICE_IN_SPEAKER_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
            snd_device = SND_DEVICE_IN_HANDSET_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET) {
            snd_device = SND_DEVICE_IN_BT_SCO_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
            snd_device = SND_DEVICE_IN_HDMI_MIC;
        } else {
            ALOGE("%s: Unknown output device(s) %#x", __func__, out_device);
            ALOGW("%s: Using default handset-mic", __func__);
            snd_device = SND_DEVICE_IN_HANDSET_MIC;
        }
    }
exit:
    ALOGV("%s: exit: in_snd_device(%s)", __func__, device_table[snd_device]);
    return snd_device;
}

int set_hdmi_channels(struct audio_device *adev,  int channel_count)
{
    struct mixer_ctl *ctl;
    const char *mixer_ctl_name = "";
    (void)adev;
    (void)channel_count;
    /* TODO */

    return 0;
}

int edid_get_max_channels(struct audio_device *adev)
{
    int max_channels = 2;
    struct mixer_ctl *ctl;
    (void)adev;

    /* TODO */
    return max_channels;
}

/* Delay in Us */
int64_t render_latency(audio_usecase_t usecase)
{
    (void)usecase;
    /* TODO */
    return 0;
}

static int enable_snd_device(struct audio_device *adev,
                             struct audio_usecase *uc_info,
                             snd_device_t snd_device,
                             bool update_mixer)
{
    struct mixer_card *mixer_card;
    struct listnode *node;
    const char *snd_device_name = get_snd_device_name(snd_device);

    if (snd_device_name == NULL)
        return -EINVAL;

    adev->snd_dev_ref_cnt[snd_device]++;
    if (adev->snd_dev_ref_cnt[snd_device] > 1) {
        ALOGV("%s: snd_device(%d: %s) is already active",
              __func__, snd_device, snd_device_name);
        return 0;
    }

    ALOGV("%s: snd_device(%d: %s)", __func__,
          snd_device, snd_device_name);

    list_for_each(node, &uc_info->mixer_list) {
        mixer_card = node_to_item(node, struct mixer_card, uc_list_node[uc_info->id]);
        audio_route_apply_path(mixer_card->audio_route, snd_device_name);
        if (update_mixer)
            audio_route_update_mixer(mixer_card->audio_route);
    }

    return 0;
}

static int disable_snd_device(struct audio_device *adev,
                              struct audio_usecase *uc_info,
                              snd_device_t snd_device,
                              bool update_mixer)
{
    struct mixer_card *mixer_card;
    struct listnode *node;
    const char *snd_device_name = get_snd_device_name(snd_device);

    if (snd_device_name == NULL)
        return -EINVAL;

    if (adev->snd_dev_ref_cnt[snd_device] <= 0) {
        ALOGE("%s: device ref cnt is already 0", __func__);
        return -EINVAL;
    }
    adev->snd_dev_ref_cnt[snd_device]--;
    if (adev->snd_dev_ref_cnt[snd_device] == 0) {
        ALOGV("%s: snd_device(%d: %s)", __func__,
              snd_device, snd_device_name);
        list_for_each(node, &uc_info->mixer_list) {
            mixer_card = node_to_item(node, struct mixer_card, uc_list_node[uc_info->id]);
            audio_route_reset_path(mixer_card->audio_route, snd_device_name);
            if (update_mixer)
                audio_route_update_mixer(mixer_card->audio_route);
        }
    }
    return 0;
}

/* must be called with hw device mutex locked */
static int read_hdmi_channel_masks(struct stream_out *out)
{
    int ret = 0;
    int channels = edid_get_max_channels(out->dev);

    switch (channels) {
        /*
         * Do not handle stereo output in Multi-channel cases
         * Stereo case is handled in normal playback path
         */
    case 6:
        ALOGV("%s: HDMI supports 5.1", __func__);
        out->supported_channel_masks[0] = AUDIO_CHANNEL_OUT_5POINT1;
        break;
    case 8:
        ALOGV("%s: HDMI supports 5.1 and 7.1 channels", __func__);
        out->supported_channel_masks[0] = AUDIO_CHANNEL_OUT_5POINT1;
        out->supported_channel_masks[1] = AUDIO_CHANNEL_OUT_7POINT1;
        break;
    default:
        ALOGE("HDMI does not support multi channel playback");
        ret = -ENOSYS;
        break;
    }
    return ret;
}

static int select_devices(struct audio_device *adev,
                          audio_usecase_t uc_id)
{
    snd_device_t out_snd_device = SND_DEVICE_NONE;
    snd_device_t in_snd_device = SND_DEVICE_NONE;
    struct audio_usecase *usecase = NULL;
    struct audio_usecase *vc_usecase = NULL;
    struct listnode *node;
    int status = 0;
    struct stream_in *active_input = NULL;
    struct stream_out *active_out;
    struct mixer_card *mixer_card;

    usecase = get_usecase_from_type(adev, PCM_CAPTURE|VOICE_CALL);
    if (usecase != NULL) {
        active_input = (struct stream_in *)usecase->stream;
    }

    usecase = get_usecase_from_id(adev, uc_id);
    if (usecase == NULL) {
        ALOGE("%s: Could not find the usecase(%d)", __func__, uc_id);
        return -EINVAL;
    }
    active_out = (struct stream_out *)usecase->stream;

    if (usecase->type == VOICE_CALL) {
        out_snd_device = get_output_snd_device(adev, active_out->devices);
        in_snd_device = get_input_snd_device(adev, active_out->devices);
        usecase->devices = active_out->devices;
    } else {
        /*
         * If the voice call is active, use the sound devices of voice call usecase
         * so that it would not result any device switch. All the usecases will
         * be switched to new device when select_devices() is called for voice call
         * usecase.
         */
        if (adev->in_call) {
            vc_usecase = get_usecase_from_id(adev, USECASE_VOICE_CALL);
            if (usecase == NULL) {
                ALOGE("%s: Could not find the voice call usecase", __func__);
            } else {
                in_snd_device = vc_usecase->in_snd_device;
                out_snd_device = vc_usecase->out_snd_device;
            }
        }
        if (usecase->type == PCM_PLAYBACK) {
            usecase->devices = active_out->devices;
            in_snd_device = SND_DEVICE_NONE;
            if (out_snd_device == SND_DEVICE_NONE) {
                out_snd_device = get_output_snd_device(adev, active_out->devices);
                if (active_out == adev->primary_output &&
                        active_input &&
                        active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION) {
                    select_devices(adev, active_input->usecase);
                }
            }
        } else if (usecase->type == PCM_CAPTURE) {
            usecase->devices = ((struct stream_in *)usecase->stream)->devices;
            out_snd_device = SND_DEVICE_NONE;
            if (in_snd_device == SND_DEVICE_NONE) {
                if (active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION &&
                        adev->primary_output && !adev->primary_output->standby) {
                    in_snd_device = get_input_snd_device(adev, adev->primary_output->devices);
                } else {
                    in_snd_device = get_input_snd_device(adev, AUDIO_DEVICE_NONE);
                }
            }
        }
    }

    if (out_snd_device == usecase->out_snd_device &&
        in_snd_device == usecase->in_snd_device) {
        return 0;
    }

    ALOGD("%s: out_snd_device(%d: %s) in_snd_device(%d: %s)", __func__,
          out_snd_device, get_snd_device_display_name(out_snd_device),
          in_snd_device,  get_snd_device_display_name(in_snd_device));


    /* Disable current sound devices */
    if (usecase->out_snd_device != SND_DEVICE_NONE) {
        disable_snd_device(adev, usecase, usecase->out_snd_device, false);
    }

    if (usecase->in_snd_device != SND_DEVICE_NONE) {
        disable_snd_device(adev, usecase, usecase->in_snd_device, false);
    }

    /* Enable new sound devices */
    if (out_snd_device != SND_DEVICE_NONE) {
        enable_snd_device(adev, usecase, out_snd_device, false);
    }

    if (in_snd_device != SND_DEVICE_NONE) {
        enable_snd_device(adev, usecase, in_snd_device, false);
    }

    list_for_each(node, &usecase->mixer_list) {
         mixer_card = node_to_item(node, struct mixer_card, uc_list_node[usecase->id]);
         audio_route_update_mixer(mixer_card->audio_route);
    }

    usecase->in_snd_device = in_snd_device;
    usecase->out_snd_device = out_snd_device;

    if (out_snd_device != SND_DEVICE_NONE)
        if (usecase->devices & (AUDIO_DEVICE_OUT_WIRED_HEADSET | AUDIO_DEVICE_OUT_WIRED_HEADPHONE))
            if (adev->htc_acoustic_set_rt5506_amp != NULL)
                adev->htc_acoustic_set_rt5506_amp(adev->mode, usecase->devices);

    return status;
}

static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                                   struct resampler_buffer* buffer)
{
    struct stream_in *in;
    struct pcm_device *pcm_device;

    if (buffer_provider == NULL || buffer == NULL)
        return -EINVAL;

    in = (struct stream_in *)((char *)buffer_provider -
                                   offsetof(struct stream_in, buf_provider));

    if (list_empty(&in->pcm_dev_list)) {
        buffer->raw = NULL;
        buffer->frame_count = 0;
        in->read_status = -ENODEV;
        return -ENODEV;
    }

    pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                              struct pcm_device, stream_list_node);

    if (in->frames_in == 0) {
        in->read_status = pcm_read(pcm_device->pcm,
                                   (void*)in->buffer,
                                   in->buffer_size);
        if (in->read_status != 0) {
            ALOGE("get_next_buffer() pcm_read error %d", in->read_status);
            buffer->raw = NULL;
            buffer->frame_count = 0;
            return in->read_status;
        }
        in->frames_in = in->config.period_size;
        if (popcount(in->channel_mask) == 1) {
            unsigned int i;

            /* Discard right channel */
            for (i = 1; i < in->frames_in; i++)
                in->buffer[i] = in->buffer[i * 2];
        }
    }

    buffer->frame_count = (buffer->frame_count > in->frames_in) ?
                                in->frames_in : buffer->frame_count;
    buffer->i16 = in->buffer + (in->config.period_size - in->frames_in);

    return in->read_status;

}

static void release_buffer(struct resampler_buffer_provider *buffer_provider,
                                  struct resampler_buffer* buffer)
{
    struct stream_in *in;

    if (buffer_provider == NULL || buffer == NULL)
        return;

    in = (struct stream_in *)((char *)buffer_provider -
                                   offsetof(struct stream_in, buf_provider));

    in->frames_in -= buffer->frame_count;
}

/* read_frames() reads frames from kernel driver, down samples to capture rate
 * if necessary and output the number of frames requested to the buffer specified */
static ssize_t read_frames(struct stream_in *in, void *buffer, ssize_t frames)
{
    ssize_t frames_wr = 0;

    while (frames_wr < frames) {
        size_t frames_rd = frames - frames_wr;
        if (in->resampler != NULL) {
            in->resampler->resample_from_provider(in->resampler,
                    (int16_t *)((char *)buffer +
                            frames_wr * audio_stream_frame_size(&in->stream.common)),
                    &frames_rd);
        } else {
            struct resampler_buffer buf = {
                    { raw : NULL, },
                    frame_count : frames_rd,
            };
            get_next_buffer(&in->buf_provider, &buf);
            if (buf.raw != NULL) {
                memcpy((char *)buffer +
                           frames_wr * audio_stream_frame_size(&in->stream.common),
                        buf.raw,
                        buf.frame_count * audio_stream_frame_size(&in->stream.common));
                frames_rd = buf.frame_count;
            }
            release_buffer(&in->buf_provider, &buf);
        }
        /* in->read_status is updated by getNextBuffer() also called by
         * in->resampler->resample_from_provider() */
        if (in->read_status != 0)
            return in->read_status;

        frames_wr += frames_rd;
    }
    return frames_wr;
}


static int stop_input_stream(struct stream_in *in)
{
    struct audio_usecase *uc_info;
    struct audio_device *adev = in->dev;
    struct pcm_device *pcm_device;

    ALOGV("%s: enter: usecase(%d: %s)", __func__,
          in->usecase, use_case_table[in->usecase]);
    uc_info = get_usecase_from_id(adev, in->usecase);
    if (uc_info == NULL) {
        ALOGE("%s: Could not find the usecase (%d) in the list",
              __func__, in->usecase);
        return -EINVAL;
    }

    /* 2. Disable the tx device */
    disable_snd_device(adev, uc_info, uc_info->in_snd_device, true);

    list_remove(&uc_info->adev_list_node);
    free(uc_info);

    if (list_empty(&in->pcm_dev_list)) {
        ALOGE("%s: pcm device list empty", __func__);
        return -EINVAL;
    }

    pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                              struct pcm_device, stream_list_node);
    free(pcm_device);
    list_init(&in->pcm_dev_list);

    ALOGV("%s: exit", __func__);
    return 0;
}

int start_input_stream(struct stream_in *in)
{
    /* 1. Enable output device and stream routing controls */
    int ret = 0;
    struct audio_usecase *uc_info;
    struct audio_device *adev = in->dev;
    struct pcm_device_profile *pcm_profile;
    struct pcm_device *pcm_device;
    ALOGV("%s: enter: usecase(%d)", __func__, in->usecase);
    pcm_profile = get_pcm_device(PCM_CAPTURE, in->devices);
    if (pcm_profile == NULL) {
        ALOGE("%s: Could not find PCM device id for the usecase(%d)",
              __func__, in->usecase);
        ret = -EINVAL;
        goto error_config;
    }


    uc_info = (struct audio_usecase *)calloc(1, sizeof(struct audio_usecase));
    uc_info->id = in->usecase;
    uc_info->type = PCM_CAPTURE;
    uc_info->stream = (struct audio_stream *)in;
    uc_info->devices = in->devices;
    uc_info->in_snd_device = SND_DEVICE_NONE;
    uc_info->out_snd_device = SND_DEVICE_NONE;

    pcm_device = (struct pcm_device *)calloc(1, sizeof(struct pcm_device));
    pcm_device->pcm_profile = pcm_profile;
    list_init(&in->pcm_dev_list);
    list_add_tail(&in->pcm_dev_list, &pcm_device->stream_list_node);

    list_init(&uc_info->mixer_list);
    list_add_tail(&uc_info->mixer_list,
                  &adev_get_mixer_for_card(adev,
                                       pcm_device->pcm_profile->card)->uc_list_node[uc_info->id]);

    list_add_tail(&adev->usecase_list, &uc_info->adev_list_node);

    select_devices(adev, in->usecase);

    ALOGV("%s: Opening PCM device card_id(%d) device_id(%d), channels %d, smp rate %d format %d",
          __func__, pcm_device->pcm_profile->card, pcm_device->pcm_profile->id, in->config.channels,
          in->config.rate, in->config.format);
    pcm_device->pcm = pcm_open(pcm_device->pcm_profile->card, pcm_device->pcm_profile->id,
                       PCM_IN, &pcm_device->pcm_profile->config);

    if (pcm_device->pcm && !pcm_is_ready(pcm_device->pcm)) {
        ALOGE("%s: %s", __func__, pcm_get_error(pcm_device->pcm));
        pcm_close(pcm_device->pcm);
        pcm_device->pcm = NULL;
        ret = -EIO;
        goto error_open;
    }

    /*
     * If the stream rate differs from the PCM rate, we need to
     * create a resampler.
     */
    if (in->requested_rate != in->config.rate) {
        in->buf_provider.get_next_buffer = get_next_buffer;
        in->buf_provider.release_buffer = release_buffer;

        ret = create_resampler(pcm_device->pcm_profile->config.rate,
                               in->requested_rate,
                               popcount(in->channel_mask),
                               RESAMPLER_QUALITY_DEFAULT,
                               &in->buf_provider,
                               &in->resampler);
    }
    in->buffer_size = pcm_frames_to_bytes(pcm_device->pcm,
                                          pcm_device->pcm_profile->config.period_size);
    in->buffer = malloc(in->buffer_size);
    in->frames_in = 0;

    ALOGV("%s: exit", __func__);
    return ret;

error_open:
    stop_input_stream(in);

error_config:
    ALOGD("%s: exit: status(%d)", __func__, ret);

    return ret;
}

/* must be called with out->lock locked */
static int send_offload_cmd_l(struct stream_out* out, int command)
{
    struct offload_cmd *cmd = (struct offload_cmd *)calloc(1, sizeof(struct offload_cmd));

    ALOGVV("%s %d", __func__, command);

    cmd->cmd = command;
    list_add_tail(&out->offload_cmd_list, &cmd->node);
    pthread_cond_signal(&out->offload_cond);
    return 0;
}

/* must be called iwth out->lock locked */
static void stop_compressed_output_l(struct stream_out *out)
{
    out->offload_state = OFFLOAD_STATE_IDLE;
    out->playback_started = 0;
    out->send_new_metadata = 1;
    if (out->compr != NULL) {
        compress_stop(out->compr);
        while (out->offload_thread_blocked) {
            pthread_cond_wait(&out->cond, &out->lock);
        }
    }
}

static void *offload_thread_loop(void *context)
{
    struct stream_out *out = (struct stream_out *) context;
    struct listnode *item;

    out->offload_state = OFFLOAD_STATE_IDLE;
    out->playback_started = 0;

    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_AUDIO);
    set_sched_policy(0, SP_FOREGROUND);
    prctl(PR_SET_NAME, (unsigned long)"Offload Callback", 0, 0, 0);

    ALOGV("%s", __func__);
    pthread_mutex_lock(&out->lock);
    for (;;) {
        struct offload_cmd *cmd = NULL;
        stream_callback_event_t event;
        bool send_callback = false;

        ALOGVV("%s offload_cmd_list %d out->offload_state %d",
              __func__, list_empty(&out->offload_cmd_list),
              out->offload_state);
        if (list_empty(&out->offload_cmd_list)) {
            ALOGV("%s SLEEPING", __func__);
            pthread_cond_wait(&out->offload_cond, &out->lock);
            ALOGV("%s RUNNING", __func__);
            continue;
        }

        item = list_head(&out->offload_cmd_list);
        cmd = node_to_item(item, struct offload_cmd, node);
        list_remove(item);

        ALOGVV("%s STATE %d CMD %d out->compr %p",
               __func__, out->offload_state, cmd->cmd, out->compr);

        if (cmd->cmd == OFFLOAD_CMD_EXIT) {
            free(cmd);
            break;
        }

        if (out->compr == NULL) {
            ALOGE("%s: Compress handle is NULL", __func__);
            pthread_cond_signal(&out->cond);
            continue;
        }
        out->offload_thread_blocked = true;
        pthread_mutex_unlock(&out->lock);
        send_callback = false;
        switch(cmd->cmd) {
        case OFFLOAD_CMD_WAIT_FOR_BUFFER:
            compress_wait(out->compr, -1);
            send_callback = true;
            event = STREAM_CBK_EVENT_WRITE_READY;
            break;
        case OFFLOAD_CMD_PARTIAL_DRAIN:
            compress_next_track(out->compr);
            compress_partial_drain(out->compr);
            send_callback = true;
            event = STREAM_CBK_EVENT_DRAIN_READY;
            break;
        case OFFLOAD_CMD_DRAIN:
            compress_drain(out->compr);
            send_callback = true;
            event = STREAM_CBK_EVENT_DRAIN_READY;
            break;
        default:
            ALOGE("%s unknown command received: %d", __func__, cmd->cmd);
            break;
        }
        pthread_mutex_lock(&out->lock);
        out->offload_thread_blocked = false;
        pthread_cond_signal(&out->cond);
        if (send_callback) {
            out->offload_callback(event, NULL, out->offload_cookie);
        }
        free(cmd);
    }

    pthread_cond_signal(&out->cond);
    while (!list_empty(&out->offload_cmd_list)) {
        item = list_head(&out->offload_cmd_list);
        list_remove(item);
        free(node_to_item(item, struct offload_cmd, node));
    }
    pthread_mutex_unlock(&out->lock);

    return NULL;
}

static int create_offload_callback_thread(struct stream_out *out)
{
    pthread_cond_init(&out->offload_cond, (const pthread_condattr_t *) NULL);
    list_init(&out->offload_cmd_list);
    pthread_create(&out->offload_thread, (const pthread_attr_t *) NULL,
                    offload_thread_loop, out);
    return 0;
}

static int destroy_offload_callback_thread(struct stream_out *out)
{
    pthread_mutex_lock(&out->lock);
    stop_compressed_output_l(out);
    send_offload_cmd_l(out, OFFLOAD_CMD_EXIT);

    pthread_mutex_unlock(&out->lock);
    pthread_join(out->offload_thread, (void **) NULL);
    pthread_cond_destroy(&out->offload_cond);

    return 0;
}

static bool allow_hdmi_channel_config(struct audio_device *adev)
{
    struct listnode *node;
    struct audio_usecase *usecase;
    bool ret = true;

    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->devices & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
            /*
             * If voice call is already existing, do not proceed further to avoid
             * disabling/enabling both RX and TX devices, CSD calls, etc.
             * Once the voice call done, the HDMI channels can be configured to
             * max channels of remaining use cases.
             */
            if (usecase->id == USECASE_VOICE_CALL) {
                ALOGD("%s: voice call is active, no change in HDMI channels",
                      __func__);
                ret = false;
                break;
            } else if (usecase->id == USECASE_AUDIO_PLAYBACK_MULTI_CH) {
                ALOGD("%s: multi channel playback is active, "
                      "no change in HDMI channels", __func__);
                ret = false;
                break;
            }
        }
    }
    return ret;
}

static int check_and_set_hdmi_channels(struct audio_device *adev,
                                       unsigned int channels)
{
    struct listnode *node;
    struct audio_usecase *usecase;

    /* Check if change in HDMI channel config is allowed */
    if (!allow_hdmi_channel_config(adev))
        return 0;

    if (channels == adev->cur_hdmi_channels) {
        ALOGD("%s: Requested channels are same as current", __func__);
        return 0;
    }

    set_hdmi_channels(adev, channels);
    adev->cur_hdmi_channels = channels;

    return 0;
}

static int uc_release_pcm_devices(struct audio_usecase *usecase)
{
    struct stream_out *out = (struct stream_out *)usecase->stream;
    struct pcm_device *pcm_device;
    struct listnode *node;
    struct listnode *next;

    list_for_each_safe(node, next, &out->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        list_remove(node);
        free(pcm_device);
    }
    list_init(&usecase->mixer_list);

    return 0;
}

static int uc_select_pcm_devices(struct audio_usecase *usecase)

{
    struct stream_out *out = (struct stream_out *)usecase->stream;
    struct pcm_device *pcm_device;
    struct pcm_device_profile *pcm_profile;
    struct mixer_card *mixer_card;
    audio_devices_t devices = usecase->devices;

    list_init(&usecase->mixer_list);
    list_init(&out->pcm_dev_list);

    while ((pcm_profile = get_pcm_device(usecase->type, devices)) != NULL) {
        pcm_device = calloc(1, sizeof(struct pcm_device));
        pcm_device->pcm_profile = pcm_profile;
        list_add_tail(&out->pcm_dev_list, &pcm_device->stream_list_node);
        mixer_card = uc_get_mixer_for_card(usecase, pcm_profile->card);
        if (mixer_card == NULL) {
            mixer_card = adev_get_mixer_for_card(out->dev, pcm_profile->card);
            list_add_tail(&usecase->mixer_list, &mixer_card->uc_list_node[usecase->id]);
        }
        devices &= ~pcm_profile->devices;
    }

    return 0;
}

static int out_close_pcm_devices(struct stream_out *out)
{
    struct pcm_device *pcm_device;
    struct listnode *node;

    list_for_each(node, &out->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        if (pcm_device->pcm) {
            pcm_close(pcm_device->pcm);
        }
    }

    return 0;
}

static int out_open_pcm_devices(struct stream_out *out)
{
    struct pcm_device *pcm_device;
    struct listnode *node;
    int ret = 0;

    list_for_each(node, &out->pcm_dev_list) {
        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
        ALOGV("%s: Opening PCM device card_id(%d) device_id(%d)",
              __func__, pcm_device->pcm_profile->card, pcm_device->pcm_profile->id);
        pcm_device->pcm = pcm_open(pcm_device->pcm_profile->card, pcm_device->pcm_profile->id,
                               PCM_OUT | PCM_MONOTONIC, &pcm_device->pcm_profile->config);
        if (pcm_device->pcm && !pcm_is_ready(pcm_device->pcm)) {
            ALOGE("%s: %s", __func__, pcm_get_error(pcm_device->pcm));
            pcm_device->pcm = NULL;
            ret = -EIO;
            goto error_open;
        }
    }
    return ret;

error_open:
    out_close_pcm_devices(out);
    return ret;
}

static int stop_output_stream(struct stream_out *out)
{
    int i, ret = 0;
    struct audio_usecase *uc_info;
    struct audio_device *adev = out->dev;

    ALOGV("%s: enter: usecase(%d: %s)", __func__,
          out->usecase, use_case_table[out->usecase]);
    uc_info = get_usecase_from_id(adev, out->usecase);
    if (uc_info == NULL) {
        ALOGE("%s: Could not find the usecase (%d) in the list",
              __func__, out->usecase);
        return -EINVAL;
    }

    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD &&
            adev->offload_fx_stop_output != NULL)
        adev->offload_fx_stop_output(out->handle);

    disable_snd_device(adev, uc_info, uc_info->out_snd_device, true);

    uc_release_pcm_devices(uc_info);
    list_remove(&uc_info->adev_list_node);
    free(uc_info);

    /* Must be called after removing the usecase from list */
    if (out->devices & AUDIO_DEVICE_OUT_AUX_DIGITAL)
        check_and_set_hdmi_channels(adev, PLAYBACK_HDMI_DEFAULT_CHANNEL_COUNT);

    ALOGV("%s: exit: status(%d)", __func__, ret);
    return ret;
}

int start_output_stream(struct stream_out *out)
{
    int ret = 0;
    struct audio_usecase *uc_info;
    struct audio_device *adev = out->dev;

    ALOGV("%s: enter: usecase(%d: %s) devices(%#x)",
          __func__, out->usecase, use_case_table[out->usecase], out->devices);

    uc_info = (struct audio_usecase *)calloc(1, sizeof(struct audio_usecase));
    uc_info->id = out->usecase;
    uc_info->type = PCM_PLAYBACK;
    uc_info->stream = (struct audio_stream *)out;
    uc_info->devices = out->devices;
    uc_info->in_snd_device = SND_DEVICE_NONE;
    uc_info->out_snd_device = SND_DEVICE_NONE;
    uc_select_pcm_devices(uc_info);

    /* This must be called before adding this usecase to the list */
    if (out->devices & AUDIO_DEVICE_OUT_AUX_DIGITAL)
        check_and_set_hdmi_channels(adev, out->config.channels);

    list_add_tail(&adev->usecase_list, &uc_info->adev_list_node);

    select_devices(adev, out->usecase);

    if (out->usecase != USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        out->compr = NULL;
        ret = out_open_pcm_devices(out);
        if (ret != 0)
            goto error_open;
    } else {
        out->compr = compress_open(COMPRESS_CARD, COMPRESS_DEVICE,
                                   COMPRESS_IN, &out->compr_config);
        if (out->compr && !is_compress_ready(out->compr)) {
            ALOGE("%s: %s", __func__, compress_get_error(out->compr));
            compress_close(out->compr);
            out->compr = NULL;
            ret = -EIO;
            goto error_open;
        }
        if (out->offload_callback)
            compress_nonblock(out->compr, out->non_blocking);

        if (adev->offload_fx_start_output != NULL)
            adev->offload_fx_start_output(out->handle);
    }
    ALOGV("%s: exit", __func__);
    return 0;
error_open:
    stop_output_stream(out);
error_config:
    return ret;
}

static int stop_voice_call(struct audio_device *adev)
{
    struct audio_usecase *uc_info;

    ALOGV("%s: enter", __func__);
    adev->in_call = false;

    /* TODO: implement voice call stop */

    uc_info = get_usecase_from_id(adev, USECASE_VOICE_CALL);
    if (uc_info == NULL) {
        ALOGE("%s: Could not find the usecase (%d) in the list",
              __func__, USECASE_VOICE_CALL);
        return -EINVAL;
    }

    disable_snd_device(adev, uc_info, uc_info->out_snd_device, false);
    disable_snd_device(adev, uc_info, uc_info->in_snd_device, true);

    uc_release_pcm_devices(uc_info);
    list_remove(&uc_info->adev_list_node);
    free(uc_info);

    ALOGV("%s: exit", __func__);
    return 0;
}

static int start_voice_call(struct audio_device *adev)
{
    struct audio_usecase *uc_info;

    ALOGV("%s: enter", __func__);

    uc_info = (struct audio_usecase *)calloc(1, sizeof(struct audio_usecase));
    uc_info->id = USECASE_VOICE_CALL;
    uc_info->type = VOICE_CALL;
    uc_info->stream = (struct audio_stream *)adev->primary_output;
    uc_info->devices = adev->primary_output->devices;
    uc_info->in_snd_device = SND_DEVICE_NONE;
    uc_info->out_snd_device = SND_DEVICE_NONE;

    uc_select_pcm_devices(uc_info);

    list_add_tail(&adev->usecase_list, &uc_info->adev_list_node);

    select_devices(adev, USECASE_VOICE_CALL);


    /* TODO: implement voice call start */

    /* set cached volume */
    set_voice_volume_l(adev, adev->voice_volume);

    adev->in_call = true;
    ALOGV("%s: exit", __func__);
    return 0;
}

static int check_input_parameters(uint32_t sample_rate,
                                  audio_format_t format,
                                  int channel_count)
{
    if (format != AUDIO_FORMAT_PCM_16_BIT) return -EINVAL;

    if ((channel_count < 1) || (channel_count > 2)) return -EINVAL;

    switch (sample_rate) {
    case 8000:
    case 11025:
    case 12000:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static size_t get_input_buffer_size(uint32_t sample_rate,
                                    audio_format_t format,
                                    int channel_count,
                                    audio_devices_t devices)
{
    size_t size = 0;
    struct pcm_device_profile *pcm_profile;

    if (check_input_parameters(sample_rate, format, channel_count) != 0)
        return 0;

    pcm_profile = get_pcm_device(PCM_CAPTURE, devices);
    if (pcm_profile == NULL)
        return 0;

    /*
     * take resampling into account and return the closest majoring
     * multiple of 16 frames, as audioflinger expects audio buffers to
     * be a multiple of 16 frames
     */
    size = (pcm_profile->config.period_size * sample_rate) / pcm_profile->config.rate;
    size = ((size + 15) / 16) * 16;

    return (size * popcount(channel_count) *
                audio_bytes_per_sample(format));

}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->sample_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    (void)stream;
    (void)rate;
    return -ENOSYS;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        return out->compr_config.fragment_size;
    }

    return out->config.period_size * audio_stream_frame_size(stream);
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    (void)stream;
    (void)format;
    return -ENOSYS;
}

static int do_out_standby_l(struct stream_out *out)
{
    int status = 0;

    out->standby = true;
    if (out->usecase != USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        out_close_pcm_devices(out);
    } else {
        stop_compressed_output_l(out);
        out->gapless_mdata.encoder_delay = 0;
        out->gapless_mdata.encoder_padding = 0;
        if (out->compr != NULL) {
            compress_close(out->compr);
            out->compr = NULL;
        }
    }
    status = stop_output_stream(out);

    return status;
}

static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;

    ALOGV("%s: enter: usecase(%d: %s)", __func__,
          out->usecase, use_case_table[out->usecase]);

    pthread_mutex_lock(&out->lock);
    if (!out->standby) {
        pthread_mutex_lock(&adev->lock);
        do_out_standby_l(out);
        pthread_mutex_unlock(&adev->lock);
    }
    pthread_mutex_unlock(&out->lock);
    ALOGV("%s: exit", __func__);
    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    (void)stream;
    (void)fd;

    return 0;
}

static int parse_compress_metadata(struct stream_out *out, struct str_parms *parms)
{
    int ret = 0;
    char value[32];
    struct compr_gapless_mdata tmp_mdata;

    if (!out || !parms) {
        return -EINVAL;
    }

    ret = str_parms_get_str(parms, AUDIO_OFFLOAD_CODEC_DELAY_SAMPLES, value, sizeof(value));
    if (ret >= 0) {
        tmp_mdata.encoder_delay = atoi(value); /* what is a good limit check? */
    } else {
        return -EINVAL;
    }

    ret = str_parms_get_str(parms, AUDIO_OFFLOAD_CODEC_PADDING_SAMPLES, value, sizeof(value));
    if (ret >= 0) {
        tmp_mdata.encoder_padding = atoi(value);
    } else {
        return -EINVAL;
    }

    out->gapless_mdata = tmp_mdata;
    out->send_new_metadata = 1;
    ALOGV("%s new encoder delay %u and padding %u", __func__,
          out->gapless_mdata.encoder_delay, out->gapless_mdata.encoder_padding);

    return 0;
}


static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    struct audio_usecase *usecase;
    struct listnode *node;
    struct str_parms *parms;
    char value[32];
    int ret, val = 0;
    struct audio_usecase *uc_info;
    bool do_standby = false;
    struct pcm_device *pcm_device;
    struct pcm_device_profile *pcm_profile;


    ALOGD("%s: enter: usecase(%d: %s) kvpairs: %s",
          __func__, out->usecase, use_case_table[out->usecase], kvpairs);
    parms = str_parms_create_str(kvpairs);
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        pthread_mutex_lock(&out->lock);
        pthread_mutex_lock(&adev->lock);

        /*
         * When HDMI cable is unplugged the music playback is paused and
         * the policy manager sends routing=0. But the audioflinger
         * continues to write data until standby time (3sec).
         * As the HDMI core is turned off, the write gets blocked.
         * Avoid this by routing audio to speaker until standby.
         */
        if (out->devices == AUDIO_DEVICE_OUT_AUX_DIGITAL &&
                val == AUDIO_DEVICE_NONE) {
            val = AUDIO_DEVICE_OUT_SPEAKER;
        }

        if (val != 0) {
            out->devices = val;

            if (!out->standby) {
                uc_info = get_usecase_from_id(adev, out->usecase);
                if (uc_info == NULL) {
                    ALOGE("%s: Could not find the usecase (%d) in the list",
                          __func__, out->usecase);
                } else {
                    list_for_each(node, &out->pcm_dev_list) {
                        pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
                        if ((pcm_device->pcm_profile->devices & val) == 0)
                            do_standby = true;
                        val &= ~pcm_device->pcm_profile->devices;
                    }
                    if (val != 0)
                        do_standby = true;
                }
                if (do_standby)
                    do_out_standby_l(out);
                else
                    select_devices(adev, out->usecase);
            }

            if ((adev->mode == AUDIO_MODE_IN_CALL) && !adev->in_call &&
                    (out == adev->primary_output)) {
                start_voice_call(adev);
            } else if ((adev->mode == AUDIO_MODE_IN_CALL) && adev->in_call &&
                       (out == adev->primary_output)) {
                select_devices(adev, USECASE_VOICE_CALL);
            }
        }

        if ((adev->mode == AUDIO_MODE_NORMAL) && adev->in_call &&
                (out == adev->primary_output)) {
            stop_voice_call(adev);
        }

        pthread_mutex_unlock(&adev->lock);
        pthread_mutex_unlock(&out->lock);
    }

    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        parse_compress_metadata(out, parms);
    }

    str_parms_destroy(parms);
    ALOGV("%s: exit: code(%d)", __func__, ret);
    return ret;
}

static char* out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct str_parms *query = str_parms_create_str(keys);
    char *str;
    char value[256];
    struct str_parms *reply = str_parms_create();
    size_t i, j;
    int ret;
    bool first = true;
    ALOGV("%s: enter: keys - %s", __func__, keys);
    ret = str_parms_get_str(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value, sizeof(value));
    if (ret >= 0) {
        value[0] = '\0';
        i = 0;
        while (out->supported_channel_masks[i] != 0) {
            for (j = 0; j < ARRAY_SIZE(out_channels_name_to_enum_table); j++) {
                if (out_channels_name_to_enum_table[j].value == out->supported_channel_masks[i]) {
                    if (!first) {
                        strcat(value, "|");
                    }
                    strcat(value, out_channels_name_to_enum_table[j].name);
                    first = false;
                    break;
                }
            }
            i++;
        }
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value);
        str = str_parms_to_str(reply);
    } else {
        str = strdup(keys);
    }
    str_parms_destroy(query);
    str_parms_destroy(reply);
    ALOGV("%s: exit: returns - %s", __func__, str);
    return str;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD)
        return COMPRESS_OFFLOAD_PLAYBACK_LATENCY;

    return (out->config.period_count * out->config.period_size * 1000) /
           (out->config.rate);
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    struct stream_out *out = (struct stream_out *)stream;
    int volume[2];

    if (out->usecase == USECASE_AUDIO_PLAYBACK_MULTI_CH) {
        /* only take left channel into account: the API is for stereo anyway */
        out->muted = (left == 0.0f);
        return 0;
    } else if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        (void)right;
        /* TODO */
        return 0;
    }

    return -ENOSYS;
}

static ssize_t out_write(struct audio_stream_out *stream, const void *buffer,
                         size_t bytes)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    ssize_t ret = 0;
    struct pcm_device *pcm_device;
    struct listnode *node;

    pthread_mutex_lock(&out->lock);
    if (out->standby) {
        out->standby = false;
        pthread_mutex_lock(&adev->lock);
        ret = start_output_stream(out);
        pthread_mutex_unlock(&adev->lock);
        /* ToDo: If use case is compress offload should return 0 */
        if (ret != 0) {
            out->standby = true;
            goto exit;
        }
    }

    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        ALOGVV("%s: writing buffer (%d bytes) to compress device", __func__, bytes);
        if (out->send_new_metadata) {
            ALOGVV("send new gapless metadata");
            compress_set_gapless_metadata(out->compr, &out->gapless_mdata);
            out->send_new_metadata = 0;
        }

        ret = compress_write(out->compr, buffer, bytes);
        ALOGVV("%s: writing buffer (%d bytes) to compress device returned %d", __func__, bytes, ret);
        if (ret >= 0 && ret < (ssize_t)bytes) {
            send_offload_cmd_l(out, OFFLOAD_CMD_WAIT_FOR_BUFFER);
        }
        if (!out->playback_started) {
            compress_start(out->compr);
            out->playback_started = 1;
            out->offload_state = OFFLOAD_STATE_PLAYING;
        }
        pthread_mutex_unlock(&out->lock);
        return ret;
    } else {
        if (out->muted)
            memset((void *)buffer, 0, bytes);
        list_for_each(node, &out->pcm_dev_list) {
            pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
            if (pcm_device->pcm) {
                ALOGVV("%s: writing buffer (%d bytes) to pcm device", __func__, bytes);
                pcm_device->status = pcm_write(pcm_device->pcm, (void *)buffer, bytes);
                if (pcm_device->status != 0)
                    ret = pcm_device->status;
            }
        }
        if (ret == 0)
            out->written += bytes / (out->config.channels * sizeof(short));
    }

exit:
    pthread_mutex_unlock(&out->lock);

    if (ret != 0) {
        list_for_each(node, &out->pcm_dev_list) {
            pcm_device = node_to_item(node, struct pcm_device, stream_list_node);
            if (pcm_device->pcm && pcm_device->status != 0)
                ALOGE("%s: error %d - %s", __func__, ret, pcm_get_error(pcm_device->pcm));
        }
        out_standby(&out->stream.common);
        usleep(bytes * 1000000 / audio_stream_frame_size(&out->stream.common) /
               out_get_sample_rate(&out->stream.common));
    }
    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    struct stream_out *out = (struct stream_out *)stream;
    *dsp_frames = 0;
    if ((out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) && (dsp_frames != NULL)) {
        pthread_mutex_lock(&out->lock);
        if (out->compr != NULL) {
            compress_get_tstamp(out->compr, (unsigned long *)dsp_frames,
                    &out->sample_rate);
            ALOGVV("%s rendered frames %d sample_rate %d",
                   __func__, *dsp_frames, out->sample_rate);
        }
        pthread_mutex_unlock(&out->lock);
        return 0;
    } else
        return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    (void)stream;
    (void)effect;
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    (void)stream;
    (void)effect;
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
    (void)stream;
    (void)timestamp;
    return -EINVAL;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
                                   uint64_t *frames, struct timespec *timestamp)
{
    struct stream_out *out = (struct stream_out *)stream;
    int ret = -1;
    unsigned long dsp_frames;

    pthread_mutex_lock(&out->lock);

    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        if (out->compr != NULL) {
            compress_get_tstamp(out->compr, &dsp_frames,
                    &out->sample_rate);
            ALOGVV("%s rendered frames %ld sample_rate %d",
                   __func__, dsp_frames, out->sample_rate);
            *frames = dsp_frames;
            ret = 0;
            /* this is the best we can do */
            clock_gettime(CLOCK_MONOTONIC, timestamp);
        }
    } else {
        /* FIXME: which device to read from? */
        if (!list_empty(&out->pcm_dev_list)) {
            size_t avail;
            struct pcm_device *pcm_device = node_to_item(list_head(&out->pcm_dev_list),
                                                   struct pcm_device, stream_list_node);

            if (pcm_get_htimestamp(pcm_device->pcm, &avail, timestamp) == 0) {
                size_t kernel_buffer_size = out->config.period_size * out->config.period_count;
                int64_t signed_frames = out->written - kernel_buffer_size + avail;
                /* This adjustment accounts for buffering after app processor.
                   It is based on estimated DSP latency per use case, rather than exact. */
                signed_frames -=
                    (render_latency(out->usecase) * out->sample_rate / 1000000LL);

                /* It would be unusual for this value to be negative, but check just in case ... */
                if (signed_frames >= 0) {
                    *frames = signed_frames;
                    ret = 0;
                }
            }
        }
    }

    pthread_mutex_unlock(&out->lock);

    return ret;
}

static int out_set_callback(struct audio_stream_out *stream,
            stream_callback_t callback, void *cookie)
{
    struct stream_out *out = (struct stream_out *)stream;

    ALOGV("%s", __func__);
    pthread_mutex_lock(&out->lock);
    out->offload_callback = callback;
    out->offload_cookie = cookie;
    pthread_mutex_unlock(&out->lock);
    return 0;
}

static int out_pause(struct audio_stream_out* stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    int status = -ENOSYS;
    ALOGV("%s", __func__);
    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        pthread_mutex_lock(&out->lock);
        if (out->compr != NULL && out->offload_state == OFFLOAD_STATE_PLAYING) {
            status = compress_pause(out->compr);
            out->offload_state = OFFLOAD_STATE_PAUSED;
        }
        pthread_mutex_unlock(&out->lock);
    }
    return status;
}

static int out_resume(struct audio_stream_out* stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    int status = -ENOSYS;
    ALOGV("%s", __func__);
    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        status = 0;
        pthread_mutex_lock(&out->lock);
        if (out->compr != NULL && out->offload_state == OFFLOAD_STATE_PAUSED) {
            status = compress_resume(out->compr);
            out->offload_state = OFFLOAD_STATE_PLAYING;
        }
        pthread_mutex_unlock(&out->lock);
    }
    return status;
}

static int out_drain(struct audio_stream_out* stream, audio_drain_type_t type )
{
    struct stream_out *out = (struct stream_out *)stream;
    int status = -ENOSYS;
    ALOGV("%s", __func__);
    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        pthread_mutex_lock(&out->lock);
        if (type == AUDIO_DRAIN_EARLY_NOTIFY)
            status = send_offload_cmd_l(out, OFFLOAD_CMD_PARTIAL_DRAIN);
        else
            status = send_offload_cmd_l(out, OFFLOAD_CMD_DRAIN);
        pthread_mutex_unlock(&out->lock);
    }
    return status;
}

static int out_flush(struct audio_stream_out* stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    ALOGV("%s", __func__);
    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        pthread_mutex_lock(&out->lock);
        stop_compressed_output_l(out);
        pthread_mutex_unlock(&out->lock);
        return 0;
    }
    return -ENOSYS;
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return in->requested_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    (void)stream;
    (void)rate;
    return -ENOSYS;
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return in->channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    (void)stream;
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    (void)stream;
    (void)format;

    return -ENOSYS;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return get_input_buffer_size(in->requested_rate,
                                 in_get_format(stream),
                                 popcount(in->channel_mask),
                                 in->devices);
}

static int do_in_standby_l(struct stream_in *in)
{
    int status = 0;
    struct pcm_device *pcm_device;

    in->standby = true;

    if (list_empty(&in->pcm_dev_list)) {
        ALOGE("%s: pcm device list empty", __func__);
    } else {
        pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                                  struct pcm_device, stream_list_node);
        if (pcm_device->pcm) {
            pcm_close(pcm_device->pcm);
            pcm_device->pcm = NULL;
        }
    }
    status = stop_input_stream(in);
    if (in->resampler) {
         release_resampler(in->resampler);
         in->resampler = NULL;
    }
    if (in->buffer) {
        free(in->buffer);
        in->buffer = NULL;
    }
    return 0;
}

static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    int status = 0;
    ALOGV("%s: enter", __func__);
    pthread_mutex_lock(&in->lock);
    if (!in->standby) {
        pthread_mutex_lock(&adev->lock);
        status = do_in_standby_l(in);
        pthread_mutex_unlock(&adev->lock);
    }
    pthread_mutex_unlock(&in->lock);
    ALOGV("%s: exit:  status(%d)", __func__, status);
    return status;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    (void)stream;
    (void)fd;

    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    struct str_parms *parms;
    char *str;
    char value[32];
    int ret, val = 0;
    struct audio_usecase *uc_info;
    bool do_standby = false;
    struct listnode *node;
    struct pcm_device *pcm_device;
    struct pcm_device_profile *pcm_profile;

    ALOGV("%s: enter: kvpairs=%s", __func__, kvpairs);
    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_INPUT_SOURCE, value, sizeof(value));

    pthread_mutex_lock(&in->lock);
    pthread_mutex_lock(&adev->lock);
    if (ret >= 0) {
        val = atoi(value);
        /* no audio source uses val == 0 */
        if (((int)in->source != val) && (val != 0)) {
            in->source = val;
        }
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        if (((int)in->devices != val) && (val != 0)) {
            in->devices = val;
            /* If recording is in progress, change the tx device to new device */
            if (!in->standby) {
                uc_info = get_usecase_from_id(adev, in->usecase);
                if (uc_info == NULL) {
                    ALOGE("%s: Could not find the usecase (%d) in the list",
                          __func__, in->usecase);
                } else {
                    if (list_empty(&in->pcm_dev_list))
                        ALOGE("%s: pcm device list empty", __func__);
                    else {
                        pcm_device = node_to_item(list_head(&in->pcm_dev_list),
                                                  struct pcm_device, stream_list_node);
                        if ((pcm_device->pcm_profile->devices & val & ~AUDIO_DEVICE_BIT_IN) == 0) {
                            do_standby = true;
                        }
                    }
                }
                if (do_standby) {
                    ret = do_in_standby_l(in);
                } else
                    ret = select_devices(adev, in->usecase);
            }
        }
    }

    pthread_mutex_unlock(&adev->lock);
    pthread_mutex_unlock(&in->lock);

    str_parms_destroy(parms);
    ALOGV("%s: exit: status(%d)", __func__, ret);
    return ret;
}

static char* in_get_parameters(const struct audio_stream *stream,
                               const char *keys)
{
    (void)stream;
    (void)keys;

    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    (void)stream;
    (void)gain;

    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void *buffer,
                       size_t bytes)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    int i, ret = -1;
    size_t frames_rq = bytes / audio_stream_frame_size(&stream->common);

    pthread_mutex_lock(&in->lock);
    if (in->standby) {
        pthread_mutex_lock(&adev->lock);
        ret = start_input_stream(in);
        pthread_mutex_unlock(&adev->lock);
        if (ret != 0) {
            goto exit;
        }
        in->standby = 0;
    }
    if (!list_empty(&in->pcm_dev_list)) {
        ret = read_frames(in, buffer, frames_rq);
        if (ret > 0)
            ret = 0;

        /*
         * Instead of writing zeroes here, we could trust the hardware
         * to always provide zeroes when muted.
         */
        if (ret == 0 && adev->mic_mute)
            memset(buffer, 0, bytes);
    }

    /*
     * Instead of writing zeroes here, we could trust the hardware
     * to always provide zeroes when muted.
     */
    if (ret == 0 && adev->mic_mute)
        memset(buffer, 0, bytes);

exit:
    pthread_mutex_unlock(&in->lock);

    if (ret != 0) {
        in_standby(&in->stream.common);
        ALOGV("%s: read failed - sleeping for buffer duration", __func__);
        usleep(bytes * 1000000 / audio_stream_frame_size(&in->stream.common) /
               in->requested_rate);
    }
    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    (void)stream;

    return 0;
}

static int add_remove_audio_effect(const struct audio_stream *stream,
                                   effect_handle_t effect,
                                   bool enable)
{
    struct stream_in *in = (struct stream_in *)stream;
    int status = 0;
    effect_descriptor_t desc;

    status = (*effect)->get_descriptor(effect, &desc);
    if (status != 0)
        return status;

    pthread_mutex_lock(&in->lock);
    pthread_mutex_lock(&in->dev->lock);
    if ((in->source == AUDIO_SOURCE_VOICE_COMMUNICATION) &&
            in->enable_aec != enable &&
            (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0)) {
        in->enable_aec = enable;
        if (!in->standby)
            select_devices(in->dev, in->usecase);
    }
    pthread_mutex_unlock(&in->dev->lock);
    pthread_mutex_unlock(&in->lock);

    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream,
                               effect_handle_t effect)
{
    ALOGV("%s: effect %p", __func__, effect);
    return add_remove_audio_effect(stream, effect, true);
}

static int in_remove_audio_effect(const struct audio_stream *stream,
                                  effect_handle_t effect)
{
    ALOGV("%s: effect %p", __func__, effect);
    return add_remove_audio_effect(stream, effect, false);
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_out *out;
    int i, ret;
    struct pcm_device_profile *pcm_profile;

    ALOGV("%s: enter: sample_rate(%d) channel_mask(%#x) devices(%#x) flags(%#x)",
          __func__, config->sample_rate, config->channel_mask, devices, flags);
    *stream_out = NULL;
    out = (struct stream_out *)calloc(1, sizeof(struct stream_out));

    if (devices == AUDIO_DEVICE_NONE)
        devices = AUDIO_DEVICE_OUT_SPEAKER;

    out->flags = flags;
    out->devices = devices;
    out->dev = adev;
    out->format = config->format;
    out->sample_rate = config->sample_rate;
    out->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    out->supported_channel_masks[0] = AUDIO_CHANNEL_OUT_STEREO;
    out->handle = handle;

    pcm_profile = get_pcm_device(PCM_PLAYBACK, devices);
    if (pcm_profile == NULL) {
        ret = -EINVAL;
        goto error_open;
    }
    out->config = pcm_profile->config;

    /* Init use case and pcm_config */
    if (out->flags & AUDIO_OUTPUT_FLAG_DIRECT &&
            !(out->flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) &&
        out->devices & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
        pthread_mutex_lock(&adev->lock);
        ret = read_hdmi_channel_masks(out);
        pthread_mutex_unlock(&adev->lock);
        if (ret != 0)
            goto error_open;

        if (config->sample_rate == 0)
            config->sample_rate = PLAYBACK_DEFAULT_SAMPLING_RATE;
        if (config->channel_mask == 0)
            config->channel_mask = AUDIO_CHANNEL_OUT_5POINT1;

        out->channel_mask = config->channel_mask;
        out->sample_rate = config->sample_rate;
        out->usecase = USECASE_AUDIO_PLAYBACK_MULTI_CH;
        out->config.rate = config->sample_rate;
        out->config.channels = popcount(out->channel_mask);
    } else if (out->flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
        if (config->offload_info.version != AUDIO_INFO_INITIALIZER.version ||
            config->offload_info.size != AUDIO_INFO_INITIALIZER.size) {
            ALOGE("%s: Unsupported Offload information", __func__);
            ret = -EINVAL;
            goto error_open;
        }
        if (!is_supported_format(config->offload_info.format)) {
            ALOGE("%s: Unsupported audio format", __func__);
            ret = -EINVAL;
            goto error_open;
        }

        out->compr_config.codec = (struct snd_codec *)
                                    calloc(1, sizeof(struct snd_codec));

        out->usecase = USECASE_AUDIO_PLAYBACK_OFFLOAD;
        if (config->offload_info.channel_mask)
            out->channel_mask = config->offload_info.channel_mask;
        else if (config->channel_mask)
            out->channel_mask = config->channel_mask;
        out->format = config->offload_info.format;
        out->sample_rate = config->offload_info.sample_rate;

        out->stream.set_callback = out_set_callback;
        out->stream.pause = out_pause;
        out->stream.resume = out_resume;
        out->stream.drain = out_drain;
        out->stream.flush = out_flush;

        out->compr_config.codec->id =
                get_snd_codec_id(config->offload_info.format);
        out->compr_config.fragment_size = COMPRESS_OFFLOAD_FRAGMENT_SIZE;
        out->compr_config.fragments = COMPRESS_OFFLOAD_NUM_FRAGMENTS;
        out->compr_config.codec->sample_rate =
                    compress_get_alsa_rate(config->offload_info.sample_rate);
        out->compr_config.codec->bit_rate =
                    config->offload_info.bit_rate;
        out->compr_config.codec->ch_in =
                    popcount(config->channel_mask);
        out->compr_config.codec->ch_out = out->compr_config.codec->ch_in;

        if (flags & AUDIO_OUTPUT_FLAG_NON_BLOCKING)
            out->non_blocking = 1;

        out->send_new_metadata = 1;
        create_offload_callback_thread(out);
        ALOGV("%s: offloaded output offload_info version %04x bit rate %d",
                __func__, config->offload_info.version,
                config->offload_info.bit_rate);
    } else if (out->flags & (AUDIO_OUTPUT_FLAG_DEEP_BUFFER)) {
        out->usecase = USECASE_AUDIO_PLAYBACK_DEEP_BUFFER;
        out->config = pcm_config_deep_buffer;
        out->sample_rate = out->config.rate;
        ALOGD("%s: use AUDIO_PLAYBACK_DEEP_BUFFER",__func__);
    } else {
        out->usecase = USECASE_AUDIO_PLAYBACK;
        out->sample_rate = out->config.rate;
    }

    if (flags & AUDIO_OUTPUT_FLAG_PRIMARY) {
        if(adev->primary_output == NULL)
            adev->primary_output = out;
        else {
            ALOGE("%s: Primary output is already opened", __func__);
            ret = -EEXIST;
            goto error_open;
        }
    }

    /* Check if this usecase is already existing */
    pthread_mutex_lock(&adev->lock);
    if (get_usecase_from_id(adev, out->usecase) != NULL) {
        ALOGE("%s: Usecase (%d) is already present", __func__, out->usecase);
        pthread_mutex_unlock(&adev->lock);
        ret = -EEXIST;
        goto error_open;
    }
    pthread_mutex_unlock(&adev->lock);

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;
    out->stream.get_presentation_position = out_get_presentation_position;

    out->standby = 1;
    /* out->muted = false; by calloc() */
    /* out->written = 0; by calloc() */

    pthread_mutex_init(&out->lock, (const pthread_mutexattr_t *) NULL);
    pthread_cond_init(&out->cond, (const pthread_condattr_t *) NULL);

    config->format = out->stream.common.get_format(&out->stream.common);
    config->channel_mask = out->stream.common.get_channels(&out->stream.common);
    config->sample_rate = out->stream.common.get_sample_rate(&out->stream.common);

    *stream_out = &out->stream;
    ALOGV("%s: exit", __func__);
    return 0;

error_open:
    free(out);
    *stream_out = NULL;
    ALOGD("%s: exit: ret %d", __func__, ret);
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    (void)dev;

    ALOGV("%s: enter", __func__);
    out_standby(&stream->common);
    if (out->usecase == USECASE_AUDIO_PLAYBACK_OFFLOAD) {
        destroy_offload_callback_thread(out);

        if (out->compr_config.codec != NULL)
            free(out->compr_config.codec);
    }
    pthread_cond_destroy(&out->cond);
    pthread_mutex_destroy(&out->lock);
    free(stream);
    ALOGV("%s: exit", __func__);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct str_parms *parms;
    char *str;
    char value[32];
    int val;
    int ret;

    ALOGV("%s: enter: %s", __func__, kvpairs);

    parms = str_parms_create_str(kvpairs);
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_TTY_MODE, value, sizeof(value));
    if (ret >= 0) {
        int tty_mode;

        if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_OFF) == 0)
            tty_mode = TTY_MODE_OFF;
        else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_VCO) == 0)
            tty_mode = TTY_MODE_VCO;
        else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_HCO) == 0)
            tty_mode = TTY_MODE_HCO;
        else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_FULL) == 0)
            tty_mode = TTY_MODE_FULL;
        else
            return -EINVAL;

        pthread_mutex_lock(&adev->lock);
        if (tty_mode != adev->tty_mode) {
            adev->tty_mode = tty_mode;
            if (adev->in_call)
                select_devices(adev, USECASE_VOICE_CALL);
        }
        pthread_mutex_unlock(&adev->lock);
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_BT_NREC, value, sizeof(value));
    if (ret >= 0) {
        /* When set to false, HAL should disable EC and NS
         * But it is currently not supported.
         */
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
            adev->bluetooth_nrec = true;
        else
            adev->bluetooth_nrec = false;
    }

    ret = str_parms_get_str(parms, "screen_state", value, sizeof(value));
    if (ret >= 0) {
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
            adev->screen_off = false;
        else
            adev->screen_off = true;
    }

    ret = str_parms_get_int(parms, "rotation", &val);
    if (ret >= 0) {
        bool reverse_speakers = false;
        switch(val) {
        /* FIXME: note that the code below assumes that the speakers are in the correct placement
             relative to the user when the device is rotated 90deg from its default rotation. This
             assumption is device-specific, not platform-specific like this code. */
        case 270:
            reverse_speakers = true;
            break;
        case 0:
        case 90:
        case 180:
            break;
        default:
            ALOGE("%s: unexpected rotation of %d", __func__, val);
        }
        pthread_mutex_lock(&adev->lock);
        if (adev->speaker_lr_swap != reverse_speakers) {
            adev->speaker_lr_swap = reverse_speakers;
            /* only update the selected device if there is active pcm playback */
            struct audio_usecase *usecase;
            struct listnode *node;
            list_for_each(node, &adev->usecase_list) {
                usecase = node_to_item(node, struct audio_usecase, adev_list_node);
                if (usecase->type == PCM_PLAYBACK) {
                    select_devices(adev, usecase->id);
                    break;
                }
            }
        }
        pthread_mutex_unlock(&adev->lock);
    }

    str_parms_destroy(parms);
    ALOGV("%s: exit with code(%d)", __func__, ret);
    return ret;
}

static char* adev_get_parameters(const struct audio_hw_device *dev,
                                 const char *keys)
{
    (void)dev;
    (void)keys;

    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    (void)dev;

    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    int ret = 0;
    struct audio_device *adev = (struct audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    /* cache volume */
    adev->voice_volume = volume;
    ret = set_voice_volume_l(adev, adev->voice_volume);
    pthread_mutex_unlock(&adev->lock);
    return ret;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    (void)dev;
    (void)volume;

    return -ENOSYS;
}

static int adev_get_master_volume(struct audio_hw_device *dev,
                                  float *volume)
{
    (void)dev;
    (void)volume;

    return -ENOSYS;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool muted)
{
    (void)dev;
    (void)muted;

    return -ENOSYS;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *muted)
{
    (void)dev;
    (void)muted;

    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    struct audio_device *adev = (struct audio_device *)dev;

    pthread_mutex_lock(&adev->lock);
    if (adev->mode != mode) {
        adev->mode = mode;
    }
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    struct audio_device *adev = (struct audio_device *)dev;
    int err = 0;

    pthread_mutex_lock(&adev->lock);
    adev->mic_mute = state;

    if (adev->mode == AUDIO_MODE_IN_CALL) {
        /* TODO */
    }

    pthread_mutex_unlock(&adev->lock);
    return err;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    *state = adev->mic_mute;

    return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    (void)dev;

    /* NOTE: we default to built in mic which may cause a mismatch between what we
     * report here and the actual buffer size
     */
    return get_input_buffer_size(config->sample_rate,
                                 config->format,
                                 popcount(config->channel_mask),
                                 AUDIO_DEVICE_IN_BUILTIN_MIC);
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_in *in;
    (void)handle;
    struct pcm_device_profile *pcm_profile;

    ALOGV("%s: enter", __func__);
    *stream_in = NULL;
    if (check_input_parameters(config->sample_rate, config->format,
                               popcount(config->channel_mask)) != 0)
        return -EINVAL;

    pcm_profile = get_pcm_device(PCM_CAPTURE, devices);
    if (pcm_profile == NULL)
        return -EINVAL;

    in = (struct stream_in *)calloc(1, sizeof(struct stream_in));

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;

    in->devices = devices;
    in->source = AUDIO_SOURCE_DEFAULT;
    in->dev = adev;
    in->standby = 1;
    in->channel_mask = config->channel_mask;
    in->requested_rate = config->sample_rate;

    in->config = pcm_profile->config;

    /* Update config params with the requested sample rate and channels */
    in->usecase = USECASE_AUDIO_CAPTURE;

    *stream_in = &in->stream;
    ALOGV("%s: exit", __func__);
    return 0;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                    struct audio_stream_in *stream)
{
    (void)dev;
    ALOGV("%s", __func__);

    in_standby(&stream->common);
    free(stream);

    return;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    (void)device;
    (void)fd;

    return 0;
}

static int adev_close(hw_device_t *device)
{
    struct audio_device *adev = (struct audio_device *)device;
    audio_device_ref_count--;
    free(adev->snd_dev_ref_cnt);
    free_mixer_list(adev);
    free(device);
    return 0;
}

static void *deepbuf_thread(void *context)
{
	ALOGD("%s: enter", __func__);
	struct audio_device *adev = (struct audio_device *)context;
	struct audio_config config;
	struct audio_stream_out *out = NULL;
	unsigned char data[DEEP_BUFFER_OUTPUT_PERIOD_SIZE*2] = {0};
	memset(&config, 0 , sizeof(struct audio_config));

	while (1) {
		pthread_mutex_lock(&adev->deepbuf_thread_lock);

		if (out == NULL)
			adev->device.open_output_stream(&adev->device, 0, AUDIO_DEVICE_OUT_SPEAKER,
			    AUDIO_OUTPUT_FLAG_DEEP_BUFFER, &config, &out);
		if (out) {
			out->write(out, data, DEEP_BUFFER_OUTPUT_PERIOD_SIZE*2);
			adev->deepbuf_thread_active = 1;
		} else
			ALOGD("%s: cant open a output deep stream", __func__);

		if (adev->deepbuf_thread_cancel || adev->deepbuf_thread_timeout-- <= 0) {
			adev->deepbuf_thread_active = 0;
			adev->deepbuf_thread_cancel = 0;
			if (out)
				adev->device.close_output_stream(&adev->device, out);
			pthread_mutex_unlock(&adev->deepbuf_thread_lock);

			break;
		}

		pthread_mutex_unlock(&adev->deepbuf_thread_lock);
		usleep(1000);
	}

	return NULL;
}

static void deepbuf_thread_open(struct audio_device *adev)
{
	adev->deepbuf_thread_timeout = 6000;
	adev->deepbuf_thread_cancel = 0;
	adev->deepbuf_thread_active = 0;
	pthread_mutex_init(&adev->deepbuf_thread_lock, (const pthread_mutexattr_t *) NULL);
	if (!adev->deepbuf_thread)
		pthread_create(&adev->deepbuf_thread, (const pthread_attr_t *) NULL, deepbuf_thread, adev);
}

static void deepbuf_thread_close(struct audio_device *adev)
{
	int retry_cnt = 20;
	pthread_mutex_lock(&adev->deepbuf_thread_lock);
	adev->deepbuf_thread_cancel = 1;
	pthread_mutex_unlock(&adev->deepbuf_thread_lock);

	while (retry_cnt > 0) {
		if (adev->deepbuf_thread_active == 0)
			break;

		retry_cnt--;
		usleep(1000);
	}

	if (adev->deepbuf_thread_active)
		ALOGD("audio path may out of order");
	else
		pthread_mutex_destroy(&adev->deepbuf_thread_lock);
}

static int adev_open(const hw_module_t *module, const char *name,
                     hw_device_t **device)
{
    struct audio_device *adev;
    int i, ret, retry_count;

    ALOGD("%s: enter", __func__);
    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) return -EINVAL;

    adev = calloc(1, sizeof(struct audio_device));

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->device.common.module = (struct hw_module_t *)module;
    adev->device.common.close = adev_close;

    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.get_master_volume = adev_get_master_volume;
    adev->device.set_master_mute = adev_set_master_mute;
    adev->device.get_master_mute = adev_get_master_mute;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    /* Set the default route before the PCM stream is opened */
    pthread_mutex_lock(&adev->lock);
    adev->mode = AUDIO_MODE_NORMAL;
    adev->primary_output = NULL;
    adev->voice_volume = 1.0f;
    adev->tty_mode = TTY_MODE_OFF;
    adev->bluetooth_nrec = true;
    adev->in_call = false;
    /* adev->cur_hdmi_channels = 0;  by calloc() */
    adev->snd_dev_ref_cnt = calloc(SND_DEVICE_MAX, sizeof(int));

    adev->dualmic_config = DUALMIC_CONFIG_NONE;
    adev->ns_in_voice_rec = false;

    list_init(&adev->usecase_list);
    pthread_mutex_unlock(&adev->lock);

    if (mixer_init(adev) != 0) {
        free(adev->snd_dev_ref_cnt);
        free(adev);
        ALOGE("%s: Failed to init, aborting.", __func__);
        *device = NULL;
        return -EINVAL;
    }

    if (access(OFFLOAD_FX_LIBRARY_PATH, R_OK) == 0) {
        adev->offload_fx_lib = dlopen(OFFLOAD_FX_LIBRARY_PATH, RTLD_NOW);
        if (adev->offload_fx_lib == NULL) {
            ALOGE("%s: DLOPEN failed for %s", __func__, OFFLOAD_FX_LIBRARY_PATH);
        } else {
            ALOGV("%s: DLOPEN successful for %s", __func__, OFFLOAD_FX_LIBRARY_PATH);
            adev->offload_fx_start_output =
                        (int (*)(audio_io_handle_t))dlsym(adev->offload_fx_lib,
                                                        "offload_fx_hal_start_output");
            adev->offload_fx_stop_output =
                        (int (*)(audio_io_handle_t))dlsym(adev->offload_fx_lib,
                                                        "offload_fx_hal_stop_output");
        }
    }

    if (access(HTC_ACOUSTIC_LIBRARY_PATH, R_OK) == 0) {
        adev->htc_acoustic_lib = dlopen(HTC_ACOUSTIC_LIBRARY_PATH, RTLD_NOW);
        if (adev->htc_acoustic_lib == NULL) {
            ALOGE("%s: DLOPEN failed for %s", __func__, HTC_ACOUSTIC_LIBRARY_PATH);
        } else {
            ALOGV("%s: DLOPEN successful for %s", __func__, HTC_ACOUSTIC_LIBRARY_PATH);
            adev->htc_acoustic_init_rt5506 =
                        (int (*)())dlsym(adev->htc_acoustic_lib,
                                                        "init_rt5506");
            adev->htc_acoustic_set_rt5506_amp =
                        (int (*)(int, int))dlsym(adev->htc_acoustic_lib,
                                                        "set_rt5506_amp");
            adev->htc_acoustic_set_amp_mode =
                        (int (*)(int , int, int, int, bool))dlsym(adev->htc_acoustic_lib,
                                                        "set_amp_mode");
        }
    }

    *device = &adev->device.common;

    if (adev->htc_acoustic_init_rt5506 != NULL)
        adev->htc_acoustic_init_rt5506();

    if (adev->htc_acoustic_set_amp_mode && audio_device_ref_count == 0) {
        deepbuf_thread_open(adev);
        retry_count = RETRY_NUMBER;
        while (!adev->deepbuf_thread_active && retry_count-- > 0)
            usleep(10000);
        if(adev->deepbuf_thread_active)
            adev->tfa9895_init = adev->htc_acoustic_set_amp_mode(0, AUDIO_DEVICE_OUT_SPEAKER, 0, 0, false);
        deepbuf_thread_close(adev);
    }

    audio_device_ref_count++;

    ALOGV("%s: exit", __func__);
    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "NVIDIA Tegra Audio HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};