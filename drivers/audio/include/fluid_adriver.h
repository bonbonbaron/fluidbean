/* FluidSynth - A Software Synthesizer
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

#ifndef _FLUID_AUDRIVER_H
#define _FLUID_AUDRIVER_H

#include "fluidsynthPriv.h"
#include "audio.h"
#include "include/fluidSynth.h"

#ifdef __cplusplus
extern "C" {
#endif

//******************
//******************
// ENUMS
//******************
//******************

/* Available audio drivers, listed in order of preference */
// NOTE: It's critical this stays in the same order as the array in fluidAdriver.c.
enum _AuDriverId {
#if ALSA_SUPPORT
  ALSA,
#endif

#if JACK_SUPPORT
  JACK,
#endif

#if PULSE_SUPPORT
  PULSEAUDIO,
#endif

#if PIPEWIRE_SUPPORT
  PIPEWIRE,
#endif

#if OSS_SUPPORT
  OSS,
#endif

#if OBOE_SUPPORT
  OBOE,
#endif

#if OPENSLES_SUPPORT
  OPENSLES,
#endif

#if COREAUDIO_SUPPORT
  COREAUDIO,
#endif

#if DSOUND_SUPPORT
  DSOUND,
#endif

#if WASAPI_SUPPORT
  WASAPI,
#endif

#if WAVEOUT_SUPPORT
  WAVEOUT,
#endif

#if SNDMAN_SUPPORT
  SNDMAN
#endif

#if PORTAUDIO_SUPPORT
    PORTAUDIO,
#endif

#if DART_SUPPORT
  DART,
#endif

#if SDL2_SUPPORT
  SDL2,
#endif

#if AUFILE_SUPPORT
  AUFILE,
#endif
  N_AUDIO_DRIVER_DEFS  // not used, just here to make compiler happy
};
  
struct _AudioSettings {
  Setting audioSampleFormat;
  Setting audioPeriodSize;
  Setting audioPeriods;
  Setting audioRealTimePrio;
  Setting audioDriver;
};


typedef struct _fluidAudriverDefinitionT fluidAudriverDefinitionT;

struct _fluidAudioDriverT {
    const fluidAudriverDefinitionT *define;
};

void fluidAudioDriverSettings(FluidSettings *settings);

/* Defined in fluidFilerenderer.c */
void fluidFileRendererSettings(FluidSettings *settings);

#if PULSE_SUPPORT
fluidAudioDriverT *newFluidPulseAudioDriver(FluidSettings *settings,
        fluidSynthT *synth);
fluidAudioDriverT *newFluidPulseAudioDriver2(FluidSettings *settings,
        fluidAudioFuncT func, void *data);
void deleteFluidPulseAudioDriver(fluidAudioDriverT *p);
void fluidPulseAudioDriverSettings(FluidSettings *settings);
#endif

#if ALSA_SUPPORT
fluidAudioDriverT *newFluidAlsaAudioDriver(FluidSettings *settings,
        fluidSynthT *synth);
fluidAudioDriverT *newFluidAlsaAudioDriver2(FluidSettings *settings,
        fluidAudioFuncT func, void *data);
void deleteFluidAlsaAudioDriver(fluidAudioDriverT *p);
void fluidAlsaAudioDriverSettings(FluidSettings *settings);
#endif

#if OSS_SUPPORT
fluidAudioDriverT *newFluidOssAudioDriver(FluidSettings *settings,
        fluidSynthT *synth);
fluidAudioDriverT *newFluidOssAudioDriver2(FluidSettings *settings,
        fluidAudioFuncT func, void *data);
void deleteFluidOssAudioDriver(fluidAudioDriverT *p);
void fluidOssAudioDriverSettings(FluidSettings *settings);
#endif

#if OPENSLES_SUPPORT
fluidAudioDriverT*
newFluidOpenslesAudioDriver(FluidSettings* settings,
		fluidSynthT* synth);
void deleteFluidOpenslesAudioDriver(fluidAudioDriverT* p);
void fluidOpenslesAudioDriverSettings(FluidSettings* settings);
#endif

#if OBOE_SUPPORT
fluidAudioDriverT*
newFluidOboeAudioDriver(FluidSettings* settings,
		fluidSynthT* synth);
void deleteFluidOboeAudioDriver(fluidAudioDriverT* p);
void fluidOboeAudioDriverSettings(FluidSettings* settings);
#endif

#if COREAUDIO_SUPPORT
fluidAudioDriverT *newFluidCoreAudioDriver(FluidSettings *settings,
        fluidSynthT *synth);
fluidAudioDriverT *newFluidCoreAudioDriver2(FluidSettings *settings,
        fluidAudioFuncT func,
        void *data);
void deleteFluidCoreAudioDriver(fluidAudioDriverT *p);
void fluidCoreAudioDriverSettings(FluidSettings *settings);
#endif

#if DSOUND_SUPPORT
fluidAudioDriverT *newFluidDsoundAudioDriver(FluidSettings *settings,
        fluidSynthT *synth);
fluidAudioDriverT *newFluidDsoundAudioDriver2(FluidSettings *settings,
        fluidAudioFuncT func,
        void *data);
void deleteFluidDsoundAudioDriver(fluidAudioDriverT *p);
void fluidDsoundAudioDriverSettings(FluidSettings *settings);
#endif

#if WASAPI_SUPPORT
fluidAudioDriverT *newFluidWasapiAudioDriver(FluidSettings *settings,
        fluidSynthT *synth);
fluidAudioDriverT *newFluidWasapiAudioDriver2(FluidSettings *settings,
        fluidAudioFuncT func,
        void *data);
void deleteFluidWasapiAudioDriver(fluidAudioDriverT *p);
void fluidWasapiAudioDriverSettings(FluidSettings *settings);
#endif

#if WAVEOUT_SUPPORT
fluidAudioDriverT *newFluidWaveoutAudioDriver(FluidSettings *settings,
        fluidSynthT *synth);
fluidAudioDriverT *newFluidWaveoutAudioDriver2(FluidSettings *settings,
        fluidAudioFuncT func,
        void *data);
void deleteFluidWaveoutAudioDriver(fluidAudioDriverT *p);
void fluidWaveoutAudioDriverSettings(FluidSettings *settings);
#endif

#if PORTAUDIO_SUPPORT
void fluidPortaudioDriverSettings(FluidSettings *settings);
fluidAudioDriverT *newFluidPortaudioDriver(FluidSettings *settings,
        fluidSynthT *synth);
void deleteFluidPortaudioDriver(fluidAudioDriverT *p);
#endif

#if JACK_SUPPORT
fluidAudioDriverT *newFluidJackAudioDriver(FluidSettings *settings, fluidSynthT *synth);
fluidAudioDriverT *newFluidJackAudioDriver2(FluidSettings *settings,
        fluidAudioFuncT func, void *data);
void deleteFluidJackAudioDriver(fluidAudioDriverT *p);
void fluidJackAudioDriverSettings(FluidSettings *settings);
int fluidJackObtainSynth(FluidSettings *settings, fluidSynthT **synth);
#endif

#if PIPEWIRE_SUPPORT
fluidAudioDriverT *newFluidPipewireAudioDriver(FluidSettings *settings, fluidSynthT *synth);
fluidAudioDriverT *newFluidPipewireAudioDriver2(FluidSettings *settings,
        fluidAudioFuncT func, void *data);
void deleteFluidPipewireAudioDriver(fluidAudioDriverT *p);
void fluidPipewireAudioDriverSettings(FluidSettings *settings);
#endif

#if SNDMAN_SUPPORT
fluidAudioDriverT *newFluidSndmgrAudioDriver(FluidSettings *settings,
        fluidSynthT *synth);
fluidAudioDriverT *newFluidSndmgrAudioDriver2(FluidSettings *settings,
        fluidAudioFuncT func,
        void *data);
void deleteFluidSndmgrAudioDriver(fluidAudioDriverT *p);
#endif

#if DART_SUPPORT
fluidAudioDriverT *newFluidDartAudioDriver(FluidSettings *settings,
        fluidSynthT *synth);
void deleteFluidDartAudioDriver(fluidAudioDriverT *p);
void fluidDartAudioDriverSettings(FluidSettings *settings);
#endif

#if SDL2_SUPPORT
fluidAudioDriverT *newFluidSdl2_audioDriver(FluidSettings *settings,
        fluidSynthT *synth);
void deleteFluidSdl2_audioDriver(fluidAudioDriverT *p);
void fluidSdl2_audioDriverSettings(FluidSettings *settings);
#endif

#if AUFILE_SUPPORT
fluidAudioDriverT *newFluidFileAudioDriver(FluidSettings *settings,
        fluidSynthT *synth);
void deleteFluidFileAudioDriver(fluidAudioDriverT *p);
#endif

#ifdef __cplusplus
}
#endif

#endif  /* _FLUID_AUDRIVER_H */
