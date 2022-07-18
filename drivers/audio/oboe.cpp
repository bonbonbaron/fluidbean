/* Synth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

/* oboe.c
 *
 * Audio driver for Android Oboe.
 *
 * This file may make use of C++14, because it's required by oboe anyway.
 */

#include "adriver.h"
#include "settings.h"

#if OBOE_SUPPORT

#include <oboe/Oboe.h>
#include <sstream>
#include <stdexcept>

using namespace oboe;

constexpr int NUM_CHANNELS = 2;

class OboeAudioStreamCallback;
class OboeAudioStreamErrorCallback;

/** oboe_audio_driver_t
 *
 * This structure should not be accessed directly. Use audio port
 * functions instead.
 */
typedef struct
{
    audio_driver_t driver;
    synth_t *synth = nullptr;
    bool cont = false;
    std::unique_ptr<OboeAudioStreamCallback> oboe_callback;
    std::unique_ptr<OboeAudioStreamErrorCallback> oboe_error_callback;
    std::shared_ptr<AudioStream> stream;

    double sample_rate;
    int is_sample_format_float;
    int device_id;
    int sharing_mode; // 0: Shared, 1: Exclusive
    int performance_mode; // 0: None, 1: PowerSaving, 2: LowLatency
    oboe::SampleRateConversionQuality srate_conversion_quality;
    int error_recovery_mode; // 0: Reconnect, 1: Stop
} oboe_audio_driver_t;


class OboeAudioStreamCallback : public AudioStreamCallback
{
public:

    OboeAudioStreamCallback(void *userData)
        : user_data(userData)
    {
    }

    DataCallbackResult onAudioReady(AudioStream *stream, void *audioData, int32_t numFrames)
    {
        oboe_audio_driver_t *dev = static_cast<oboe_audio_driver_t *>(this->user_data);

        if(!dev->cont)
        {
            return DataCallbackResult::Stop;
        }

        if(stream->getFormat() == AudioFormat::Float)
        {
            synth_write_float(dev->synth, numFrames, static_cast<float *>(audioData), 0, 2, static_cast<float *>(audioData), 1, 2);
        }
        else
        {
            synth_write_s16(dev->synth, numFrames, static_cast<short *>(audioData), 0, 2, static_cast<short *>(audioData), 1, 2);
        }

        return DataCallbackResult::Continue;
    }

private:
    void *user_data;
};

class OboeAudioStreamErrorCallback : public AudioStreamErrorCallback
{
    oboe_audio_driver_t *dev;

public:
    OboeAudioStreamErrorCallback(oboe_audio_driver_t *dev) : dev(dev) {}

    void onErrorAfterClose(AudioStream *stream, Result result);
};

constexpr char OBOE_ID[] = "audio.oboe.id";
constexpr char SHARING_MODE[] = "audio.oboe.sharing-mode";
constexpr char PERF_MODE[] = "audio.oboe.performance-mode";
constexpr char SRCQ_SET[] = "audio.oboe.sample-rate-conversion-quality";
constexpr char RECOVERY_MODE[] = "audio.oboe.error-recovery-mode";

void oboe_audio_driver_settings(Settings *settings)
{
    settings_register_int(settings, OBOE_ID, 0, 0, 0x7FFFFFFF, 0);

    settings_register_str(settings, SHARING_MODE, "Shared", 0);
    settings_add_option(settings,   SHARING_MODE, "Shared");
    settings_add_option(settings,   SHARING_MODE, "Exclusive");

    settings_register_str(settings, PERF_MODE, "None", 0);
    settings_add_option(settings,   PERF_MODE, "None");
    settings_add_option(settings,   PERF_MODE, "PowerSaving");
    settings_add_option(settings,   PERF_MODE, "LowLatency");

    settings_register_str(settings, SRCQ_SET, "Medium", 0);
    settings_add_option(settings,   SRCQ_SET, "None");
    settings_add_option(settings,   SRCQ_SET, "Fastest");
    settings_add_option(settings,   SRCQ_SET, "Low");
    settings_add_option(settings,   SRCQ_SET, "Medium");
    settings_add_option(settings,   SRCQ_SET, "High");
    settings_add_option(settings,   SRCQ_SET, "Best");

    settings_register_str(settings, RECOVERY_MODE, "Reconnect", 0);
    settings_add_option(settings, RECOVERY_MODE, "Reconnect");
    settings_add_option(settings, RECOVERY_MODE, "Stop");
}

static oboe::SampleRateConversionQuality get_srate_conversion_quality(Settings *settings)
{
    oboe::SampleRateConversionQuality q;

    if(settings_str_equal(settings, SRCQ_SET, "None"))
    {
        q = oboe::SampleRateConversionQuality::None;
    }
    else if(settings_str_equal(settings, SRCQ_SET, "Fastest"))
    {
        q = oboe::SampleRateConversionQuality::Fastest;
    }
    else if(settings_str_equal(settings, SRCQ_SET, "Low"))
    {
        q = oboe::SampleRateConversionQuality::Low;
    }
    else if(settings_str_equal(settings, SRCQ_SET, "Medium"))
    {
        q = oboe::SampleRateConversionQuality::Medium;
    }
    else if(settings_str_equal(settings, SRCQ_SET, "High"))
    {
        q = oboe::SampleRateConversionQuality::High;
    }
    else if(settings_str_equal(settings, SRCQ_SET, "Best"))
    {
        q = oboe::SampleRateConversionQuality::Best;
    }
    else
    {
        char buf[256];
        settings_copystr(settings, SRCQ_SET, buf, sizeof(buf));
        std::stringstream ss;
        ss << "'" << SRCQ_SET << "' has unexpected value '" << buf << "'";
        throw std::runtime_error(ss.str());
    }

    return q;
}

Result
oboe_connect_or_reconnect(oboe_audio_driver_t *dev)
{
    AudioStreamBuilder builder;
    builder.setDeviceId(dev->device_id)
    ->setDirection(Direction::Output)
    ->setChannelCount(NUM_CHANNELS)
    ->setSampleRate(dev->sample_rate)
    ->setFormat(dev->is_sample_format_float ? AudioFormat::Float : AudioFormat::I16)
    ->setSharingMode(dev->sharing_mode == 1 ? SharingMode::Exclusive : SharingMode::Shared)
    ->setPerformanceMode(
        dev->performance_mode == 1 ? PerformanceMode::PowerSaving :
        dev->performance_mode == 2 ? PerformanceMode::LowLatency : PerformanceMode::None)
    ->setUsage(Usage::Media)
    ->setContentType(ContentType::Music)
    ->setCallback(dev->oboe_callback.get())
    ->setErrorCallback(dev->oboe_error_callback.get())
    ->setSampleRateConversionQuality(dev->srate_conversion_quality);

    return builder.openStream(dev->stream);
}

/*
 * new_oboe_audio_driver
 */
audio_driver_t *
new_oboe_audio_driver(Settings *settings, synth_t *synth)
{
    oboe_audio_driver_t *dev = nullptr;

    try
    {
        Result result;
        dev = new oboe_audio_driver_t();

        dev->synth = synth;
        dev->oboe_callback = std::make_unique<OboeAudioStreamCallback>(dev);
        dev->oboe_error_callback = std::make_unique<OboeAudioStreamErrorCallback>(dev);

        settings_getnum(settings, "synth.sample-rate", &dev->sample_rate);
        dev->is_sample_format_float = settings_str_equal(settings, "audio.sample-format", "float");
        settings_getint(settings, OBOE_ID, &dev->device_id);
        dev->sharing_mode =
            settings_str_equal(settings, SHARING_MODE, "Exclusive") ? 1 : 0;
        dev->performance_mode =
            settings_str_equal(settings, PERF_MODE, "PowerSaving") ? 1 :
            settings_str_equal(settings, PERF_MODE, "LowLatency") ? 2 : 0;
        dev->srate_conversion_quality = get_srate_conversion_quality(settings);
        dev->error_recovery_mode = settings_str_equal(settings, RECOVERY_MODE, "Stop") ? 1 : 0;

        result = oboe_connect_or_reconnect(dev);

        if(result != Result::OK)
        {
            LOG(ERR, "Unable to open Oboe audio stream");
            goto error_recovery;
        }

        dev->cont = true;

        LOG(INFO, "Using Oboe driver");

        result = dev->stream->start();

        if(result != Result::OK)
        {
            LOG(ERR, "Unable to start Oboe audio stream");
            goto error_recovery;
        }

        return &dev->driver;
    }
    catch(const std::bad_alloc &)
    {
        LOG(ERR, "oboe: std::bad_alloc caught: Out of memory");
    }
    catch(const std::exception &e)
    {
        LOG(ERR, "oboe: std::exception caught: %s", e.what());
    }
    catch(...)
    {
        LOG(ERR, "Unexpected Oboe driver initialization error");
    }

error_recovery:
    delete_oboe_audio_driver(reinterpret_cast<audio_driver_t *>(dev));
    return nullptr;
}

void delete_oboe_audio_driver(audio_driver_t *p)
{
    oboe_audio_driver_t *dev = reinterpret_cast<oboe_audio_driver_t *>(p);

    return_if_fail(dev != nullptr);

    try
    {
        dev->cont = false;

        if(dev->stream != nullptr)
        {
            dev->stream->stop();
            dev->stream->close();
        }
    }
    catch(...)
    {
        LOG(ERR, "Exception caught while stopping and closing Oboe stream.");
    }

    delete dev;
}


void
OboeAudioStreamErrorCallback::onErrorAfterClose(AudioStream *stream, Result result)
{
    if(dev->error_recovery_mode == 1)    // Stop
    {
        LOG(ERR, "Oboe driver encountered an error (such as earphone unplugged). Stopped.");
        dev->stream.reset();
        return;
    }
    else
    {
        LOG(WARN, "Oboe driver encountered an error (such as earphone unplugged). Recovering...");
    }

    result = oboe_connect_or_reconnect(dev);

    if(result != Result::OK)
    {
        LOG(ERR, "Unable to reconnect Oboe audio stream");
        return; // cannot do anything further
    }

    // start the new stream.
    result = dev->stream->start();

    if(result != Result::OK)
    {
        LOG(ERR, "Unable to restart Oboe audio stream");
        return; // cannot do anything further
    }
}

#endif // OBOE_SUPPORT

