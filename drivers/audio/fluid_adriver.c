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

#include "include/adriver.h"
#include "sys.h"
#include "synth.h"

/* adriverDefinitionT */
struct _fluidAudriverDefinitionT {
  audioDriverT *(*new)(synthT *synth);
  audioDriverT *(*new2)(enum _AuDriverId, audioFuncT func, void *data);
  void (*free)(audioDriverT *driver);
  void (*settings)(FluidSettings *settings);
};

// Maybe case-switch returning a pointer to a structure that only exists if supported.
// Access this array using the enums defined in the header file.
static audriverDefinitionT auDriverA[] = {
#if ALSA_SUPPORT
   { newFluidAlsaAudioDriver, newFluidAlsaAudioDriver2, deleteFluidAlsaAudioDriver, alsaAudioDriverSettings },
#endif
#if JACK_SUPPORT
   { newFluidJackAudioDriver, newFluidJackAudioDriver2, deleteFluidJackAudioDriver, jackAudioDriverSettings },
#endif
#if PULSE_SUPPORT
   { newFluidPulseAudioDriver, newFluidPulseAudioDriver2, deleteFluidPulseAudioDriver, pulseAudioDriverSettings },
#endif
#if PIPEWIRE_SUPPORT
   { newFluidPipewireAudioDriver, newFluidPipewireAudioDriver2, deleteFluidPipewireAudioDriver, pipewireAudioDriverSettings },
#endif
#if OSS_SUPPORT
   { newFluidOssAudioDriver, newFluidOssAudioDriver2, deleteFluidOssAudioDriver, ossAudioDriverSettings },
#endif
#if OBOE_SUPPORT
   { newFluidOboeAudioDriver, NULL, deleteFluidOboeAudioDriver, oboeAudioDriverSettings },
#endif
#if OPENSLES_SUPPORT
   { newFluidOpenslesAudioDriver, NULL, deleteFluidOpenslesAudioDriver, openslesAudioDriverSettings },
#endif
#if COREAUDIO_SUPPORT
   { newFluidCoreAudioDriver, newFluidCoreAudioDriver2, deleteFluidCoreAudioDriver, coreAudioDriverSettings },
#endif
#if DSOUND_SUPPORT
   { newFluidDsoundAudioDriver, newFluidDsoundAudioDriver2, deleteFluidDsoundAudioDriver, dsoundAudioDriverSettings },
#endif
#if WASAPI_SUPPORT
   { newFluidWasapiAudioDriver, newFluidWasapiAudioDriver2, deleteFluidWasapiAudioDriver, wasapiAudioDriverSettings },
#endif
#if WAVEOUT_SUPPORT
   { newFluidWaveoutAudioDriver, newFluidWaveoutAudioDriver2, deleteFluidWaveoutAudioDriver, waveoutAudioDriverSettings },
#endif
#if SNDMAN_SUPPORT
   { newFluidSndmgrAudioDriver, newFluidSndmgrAudioDriver2, deleteFluidSndmgrAudioDriver, NULL }
#endif
#if PORTAUDIO_SUPPORT
   {newFluidPortaudioDriver, NULL, deleteFluidPortaudioDriver, portaudioDriverSettings},
#endif
#if DART_SUPPORT
   { newFluidDartAudioDriver, NULL, deleteFluidDartAudioDriver, dartAudioDriverSettings },
#endif
#if SDL2_SUPPORT
   { newFluidSdl2_audioDriver, NULL, deleteFluidSdl2_audioDriver, sdl2_audioDriverSettings },
#endif
#if AUFILE_SUPPORT
   { newFluidFileAudioDriver, NULL, deleteFluidFileAudioDriver, NULL }
#endif
};

struct _AudioSettings audioSettings = {
#if defined(WIN32)
  {512, 64, 8192},             // period size
  {8, 2, 64},                  // periods
#elif defined(MACOS9)
  {64, 64, 8192},              // period size
  {8, 2, 64},                  // periods
#else
  {64, 64, 8192},              // period size
  {16, 2, 64},                 // periods
#endif
  {0, 99, 0},                  // real-time priority
  {0, 0, N_AUDIO_DRIVER_DEFS}  // driver selection (select first one by default)
};

audioDriverT* newFluidAudioDriver(enum _AuDriverId adId, synthT *synth) {
  const audriverDefinitionT *def = &auDriverA[adId];

  if(def) {
    audioDriverT *driver = (*def->new)(synth);
    if(driver) 
      driver->define = def;
    return driver;
  }

  return NULL;
}

audioDriverT * newFluidAudioDriver2(enum _AuDriverId adId, audioFuncT func, void *data) {
  const audriverDefinitionT *def = &auDriverA[adId];

  if(def) {
    audioDriverT *driver = NULL;

    if(def->new2 != NULL) {
      driver = (*def->new2)(adId, func, data);

      if(driver)
        driver->define = def;

      return driver;
    }
  }

  return NULL;
}

void deleteFluidAudioDriver(audioDriverT *driver) {
  returnIfFail(driver != NULL);
  driver->define->free(driver);
}
