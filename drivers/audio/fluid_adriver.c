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

#include "include/fluid_adriver.h"
#include "fluid_sys.h"
#include "fluid_synth.h"

/* fluid_adriver_definition_t */
struct _fluid_audriver_definition_t {
  fluid_audio_driver_t *(*new)(fluid_synth_t *synth);
  fluid_audio_driver_t *(*new2)(enum _AuDriverId, fluid_audio_func_t func, void *data);
  void (*free)(fluid_audio_driver_t *driver);
  void (*settings)(FluidSettings *settings);
};

// Maybe case-switch returning a pointer to a structure that only exists if supported.
// Access this array using the enums defined in the header file.
static fluid_audriver_definition_t auDriverA[] = {
#if ALSA_SUPPORT
   { new_fluid_alsa_audio_driver, new_fluid_alsa_audio_driver2, delete_fluid_alsa_audio_driver, fluid_alsa_audio_driver_settings },
#endif
#if JACK_SUPPORT
   { new_fluid_jack_audio_driver, new_fluid_jack_audio_driver2, delete_fluid_jack_audio_driver, fluid_jack_audio_driver_settings },
#endif
#if PULSE_SUPPORT
   { new_fluid_pulse_audio_driver, new_fluid_pulse_audio_driver2, delete_fluid_pulse_audio_driver, fluid_pulse_audio_driver_settings },
#endif
#if PIPEWIRE_SUPPORT
   { new_fluid_pipewire_audio_driver, new_fluid_pipewire_audio_driver2, delete_fluid_pipewire_audio_driver, fluid_pipewire_audio_driver_settings },
#endif
#if OSS_SUPPORT
   { new_fluid_oss_audio_driver, new_fluid_oss_audio_driver2, delete_fluid_oss_audio_driver, fluid_oss_audio_driver_settings },
#endif
#if OBOE_SUPPORT
   { new_fluid_oboe_audio_driver, NULL, delete_fluid_oboe_audio_driver, fluid_oboe_audio_driver_settings },
#endif
#if OPENSLES_SUPPORT
   { new_fluid_opensles_audio_driver, NULL, delete_fluid_opensles_audio_driver, fluid_opensles_audio_driver_settings },
#endif
#if COREAUDIO_SUPPORT
   { new_fluid_core_audio_driver, new_fluid_core_audio_driver2, delete_fluid_core_audio_driver, fluid_core_audio_driver_settings },
#endif
#if DSOUND_SUPPORT
   { new_fluid_dsound_audio_driver, new_fluid_dsound_audio_driver2, delete_fluid_dsound_audio_driver, fluid_dsound_audio_driver_settings },
#endif
#if WASAPI_SUPPORT
   { new_fluid_wasapi_audio_driver, new_fluid_wasapi_audio_driver2, delete_fluid_wasapi_audio_driver, fluid_wasapi_audio_driver_settings },
#endif
#if WAVEOUT_SUPPORT
   { new_fluid_waveout_audio_driver, new_fluid_waveout_audio_driver2, delete_fluid_waveout_audio_driver, fluid_waveout_audio_driver_settings },
#endif
#if SNDMAN_SUPPORT
   { new_fluid_sndmgr_audio_driver, new_fluid_sndmgr_audio_driver2, delete_fluid_sndmgr_audio_driver, NULL }
#endif
#if PORTAUDIO_SUPPORT
   {new_fluid_portaudio_driver, NULL, delete_fluid_portaudio_driver, fluid_portaudio_driver_settings},
#endif
#if DART_SUPPORT
   { new_fluid_dart_audio_driver, NULL, delete_fluid_dart_audio_driver, fluid_dart_audio_driver_settings },
#endif
#if SDL2_SUPPORT
   { new_fluid_sdl2_audio_driver, NULL, delete_fluid_sdl2_audio_driver, fluid_sdl2_audio_driver_settings },
#endif
#if AUFILE_SUPPORT
   { new_fluid_file_audio_driver, NULL, delete_fluid_file_audio_driver, NULL }
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

fluid_audio_driver_t* new_fluid_audio_driver(enum _AuDriverId adId, fluid_synth_t *synth) {
  const fluid_audriver_definition_t *def = &auDriverA[adId];

  if(def) {
    fluid_audio_driver_t *driver = (*def->new)(synth);
    if(driver) 
      driver->define = def;
    return driver;
  }

  return NULL;
}

fluid_audio_driver_t * new_fluid_audio_driver2(enum _AuDriverId adId, fluid_audio_func_t func, void *data) {
  const fluid_audriver_definition_t *def = &auDriverA[adId];

  if(def) {
    fluid_audio_driver_t *driver = NULL;

    if(def->new2 != NULL) {
      driver = (*def->new2)(adId, func, data);

      if(driver)
        driver->define = def;

      return driver;
    }
  }

  return NULL;
}

void delete_fluid_audio_driver(fluid_audio_driver_t *driver) {
  fluid_return_if_fail(driver != NULL);
  driver->define->free(driver);
}
