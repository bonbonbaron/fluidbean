/* Synth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#ifndef _SYNTH_AUDIO_H
#define _SYNTH_AUDIO_H

#include "adriver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup audioOutput Audio Output
 *
 * Functions for managing audio drivers and file renderers.
 *
 * The file renderer is used for fast rendering of MIDI files to
 * audio files. The audio drivers are a high-level interface to
 * connect the synthesizer with external audio sinks or to render
 * real-time audio to files.
 */

/**
 * @defgroup audioDriver Audio Driver
 * @ingroup audioOutput
 *
 * Functions for managing audio drivers.
 *
 * Defines functions for creating audio driver output.  Use
 * newAudioDriver() to create a new audio driver for a given synth
 * and configuration settings.
 *
 * The function newAudioDriver2() can be
 * used if custom audio processing is desired before the audio is sent to the
 * audio driver (although it is not as efficient).
 *
 * @sa @ref CreatingAudioDriver
 *
 * @{
 */

/**
 * Callback function type used with newAudioDriver2() to allow for
 * custom user audio processing before the audio is sent to the driver.
 *
 * @param data The user data parameter as passed to newAudioDriver2().
 * @param len Count of audio frames to synthesize.
 * @param nfx Count of arrays in \c fx.
 * @param fx Array of buffers to store effects audio to. Buffers may alias with buffers of \c out.
 * @param nout Count of arrays in \c out.
 * @param out Array of buffers to store (dry) audio to. Buffers may alias with buffers of \c fx.
 * @return Should return #OK on success, #FAILED if an error occurred.
 *
 * This function is responsible for rendering audio to the buffers.
 * The buffers passed to this function are allocated and owned by the respective
 * audio driver and are only valid during that specific call (do not cache them).
 * The buffers have already been zeroed-out.
 * For further details please refer to SynthProcess().
 *
 * @parblock
 * @note Whereas SynthProcess() allows aliasing buffers, there is the guarantee that @p out
 * and @p fx buffers provided by synth's audio drivers never alias. This prevents downstream
 * applications from e.g. applying a custom effect accidentally to the same buffer multiple times.
 * @endparblock
 *
 * @parblock
 * @note Also note that the Jack driver is currently the only driver that has dedicated @p fx buffers
 * (but only if \setting{audioJackMulti} is true). All other drivers do not provide @p fx buffers.
 * In this case, users are encouraged to mix the effects into the provided dry buffers when calling
 * SynthProcess().
 * @code{.cpp}
int myCallback(void *, int len, int nfx, float *fx[], int nout, float *out[])
{
    int ret;
    if(nfx == 0)
    {
        float *fxb[4] = {out[0], out[1], out[0], out[1]};
        ret = SynthProcess(synth, len, sizeof(fxb) / sizeof(fxb[0]), fxb, nout, out);
    }
    else
    {
        ret = SynthProcess(synth, len, nfx, fx, nout, out);
    }
    // ... client-code ...
    return ret;
}
 * @endcode
 * For other possible use-cases refer to \ref synthProcess.c .
 * @endparblock
 */
typedef int (*AudioFuncT)(void *data, int len,
                                  int nfx, float *fx[],
                                  int nout, float *out[]);

/** @startlifecycle{Audio Driver} */
AudioDriverT *newAudioDriver(AuDriverId auDriverId,
        SynthT *synth);

AudioDriverT *newAudioDriver2(AuDriverId auDriverId,
        AudioFuncT func,
        void *data);

void deleteAudioDriver(AudioDriverT *driver);
/** @endlifecycle */

int AudioDriverRegister(const char **adrivers);
/* @} */

#if 0
/**
 * @defgroup fileRenderer File Renderer
 * @ingroup audioOutput
 *
 * Functions for managing file renderers and triggering the rendering.
 *
 * The file renderer is only used to render a MIDI file to audio as fast
 * as possible. Please see \ref FileRenderer for a full example.
 *
 * If you are looking for a way to write audio generated
 * from real-time events (for example from an external sequencer or a MIDI controller) to a file,
 * please have a look at the \c file \ref audioDriver instead.
 *
 *
 * @{
 */

/** @startlifecycle{File Renderer} */
FileRendererT *newFileRenderer(SynthT *synth);
void deleteFileRenderer(FileRendererT *dev);
/** @endlifecycle */

int FileRendererProcessBlock(FileRendererT *dev);
int FileSetEncodingQuality(FileRendererT *dev, double q);
/* @} */

#ifdef __cplusplus
}
#endif
#endif

#endif /* _SYNTH_AUDIO_H */
