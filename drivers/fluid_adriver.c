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
  fluid_audio_driver_t *(*new)(fluid_settings_t *settings, fluid_synth_t *synth);
  fluid_audio_driver_t *(*new2)(fluid_settings_t *settings, fluid_audio_func_t func, void *data);
  void (*free)(fluid_audio_driver_t *driver);
  void (*settings)(fluid_settings_t *settings);
};

// Maybe case-switch returning a pointer to a structure that only exists if supported.
//static const fluid_audriver_definition_t fluid_audio_drivers[] = {
fluid_audriver_definition_t

#if ALSA_SUPPORT
alsaDriverCbs = { new_fluid_alsa_audio_driver, new_fluid_alsa_audio_driver2, delete_fluid_alsa_audio_driver, fluid_alsa_audio_driver_settings },
#endif

#if JACK_SUPPORT
jackDriverCbs = { new_fluid_jack_audio_driver, new_fluid_jack_audio_driver2, delete_fluid_jack_audio_driver, fluid_jack_audio_driver_settings },
#endif

#if PULSE_SUPPORT
pulseAudioDriverCbs = { new_fluid_pulse_audio_driver, new_fluid_pulse_audio_driver2, delete_fluid_pulse_audio_driver, fluid_pulse_audio_driver_settings },
#endif

#if PIPEWIRE_SUPPORT
pipewireDriverCbs = { new_fluid_pipewire_audio_driver, new_fluid_pipewire_audio_driver2, delete_fluid_pipewire_audio_driver, fluid_pipewire_audio_driver_settings },
#endif

#if OSS_SUPPORT
ossDriverCbs = { new_fluid_oss_audio_driver, new_fluid_oss_audio_driver2, delete_fluid_oss_audio_driver, fluid_oss_audio_driver_settings },
#endif

#if OBOE_SUPPORT
oboeDriverCbs = { new_fluid_oboe_audio_driver, NULL, delete_fluid_oboe_audio_driver, fluid_oboe_audio_driver_settings },
#endif

#if OPENSLES_SUPPORT
openslesDriverCbs = { new_fluid_opensles_audio_driver, NULL, delete_fluid_opensles_audio_driver, fluid_opensles_audio_driver_settings },
#endif

#if COREAUDIO_SUPPORT
coreaudioDriverCbs = { new_fluid_core_audio_driver, new_fluid_core_audio_driver2, delete_fluid_core_audio_driver, fluid_core_audio_driver_settings },
#endif

#if DSOUND_SUPPORT
dsoundDriverCbs = { new_fluid_dsound_audio_driver, new_fluid_dsound_audio_driver2, delete_fluid_dsound_audio_driver, fluid_dsound_audio_driver_settings },
#endif

#if WASAPI_SUPPORT
wasapiDriverCbs = { new_fluid_wasapi_audio_driver, new_fluid_wasapi_audio_driver2, delete_fluid_wasapi_audio_driver, fluid_wasapi_audio_driver_settings },
#endif

#if WAVEOUT_SUPPORT
waveoutDriverCbs = { new_fluid_waveout_audio_driver, new_fluid_waveout_audio_driver2, delete_fluid_waveout_audio_driver, fluid_waveout_audio_driver_settings },
#endif

#if SNDMAN_SUPPORT
sndmanDriverCbs = { new_fluid_sndmgr_audio_driver, new_fluid_sndmgr_audio_driver2, delete_fluid_sndmgr_audio_driver, NULL },
#endif

#if PORTAUDIO_SUPPORT
portaudioDriverCbs = { new_fluid_portaudio_driver, NULL, delete_fluid_portaudio_driver, fluid_portaudio_driver_settings },
#endif

#if DART_SUPPORT
dartDriverCbs = { new_fluid_dart_audio_driver, NULL, delete_fluid_dart_audio_driver, fluid_dart_audio_driver_settings },
#endif

#if SDL2_SUPPORT
sdl2DriverCbs = { new_fluid_sdl2_audio_driver, NULL, delete_fluid_sdl2_audio_driver, fluid_sdl2_audio_driver_settings },
#endif

#if AUFILE_SUPPORT
fileaudioDriverCbs = { new_fluid_file_audio_driver, NULL, delete_fluid_file_audio_driver, NULL },
#endif
nullDriverCbs = { NULL, NULL, NULL, NULL}  // just to keep compiler happy
;

fluid_audriver_definition_t* getAuDriverDef(AudioDriverId driverId) {
  switch(driverId) {
#if ALSA_SUPPORT
  case ALSA: return &alsaDriverCbs;
#endif

#if JACK_SUPPORT
  case JACK: return &jackDriverCbs;
#endif

#if PULSE_SUPPORT
  case PULSEAUDIO: return &pulseAudioDriverCbs;
#endif

#if PIPEWIRE_SUPPORT
  case PIPEWIRE: return &pipewireDriverCbs;
#endif

#if OSS_SUPPORT
  case OSS: return &ossDriverCbs;
#endif

#if OBOE_SUPPORT
  case OBOE: return &oboeDriverCbs;
#endif

#if OPENSLES_SUPPORT
  case OPENSLES: return &openslesDriverCbs;
#endif

#if COREAUDIO_SUPPORT
  case COREAUDIO: return &coreaudioDriverCbs;
#endif

#if DSOUND_SUPPORT
  case DSOUND: return &dsoundDriverCbs;
#endif

#if WASAPI_SUPPORT
  case WASAPI: return &wasapiDriverCbs;
#endif

#if WAVEOUT_SUPPORT
  case WAVEOUT: return &waveoutDriverCbs;
#endif

#if SNDMAN_SUPPORT
  case SNDMAN: return &sndmanDriverCbs;
#endif

#if PORTAUDIO_SUPPORT
  case PORTAUDIO: return &portaudioDriverCbs;
#endif

#if DART_SUPPORT
  case DART: return &dartDriverCbs;
#endif

#if SDL2_SUPPORT
  case SDL2: return &sdl2DriverCbs;
#endif

#if AUFILE_SUPPORT
  case AUFILE: return &fileaudioDriverCbs;
#endif
  defaut: return NULL;
  }
}

#define ENABLE_AUDIO_DRIVER(_drv, _idx) \
  _drv[(_idx) / (sizeof(*(_drv))*8)] &= ~(1 << ((_idx) % (sizeof((*_drv))*8)))

#define IS_AUDIO_DRIVER_ENABLED(_drv, _idx) \
  (!(_drv[(_idx) / (sizeof(*(_drv))*8)] & (1 << ((_idx) % (sizeof((*_drv))*8)))))


// MB: Need FluidSettings for audio options.

void fluid_audio_driver_settings(fluid_settings_t *settings) {
  unsigned int i;
  const char *def_name = NULL;

  fluid_settings_register_str(settings, "audio.sample-format", "16bits", 0);
  fluid_settings_add_option(settings, "audio.sample-format", "16bits");
  fluid_settings_add_option(settings, "audio.sample-format", "float");

#if defined(WIN32)
  fluid_settings_register_int(settings, "audio.period-size", 512, 64, 8192, 0);
  fluid_settings_register_int(settings, "audio.periods", 8, 2, 64, 0);
#elif defined(MACOS9)
  fluid_settings_register_int(settings, "audio.period-size", 64, 64, 8192, 0);
  fluid_settings_register_int(settings, "audio.periods", 8, 2, 64, 0);
#else
  fluid_settings_register_int(settings, "audio.period-size", 64, 64, 8192, 0);
  fluid_settings_register_int(settings, "audio.periods", 16, 2, 64, 0);
#endif

  fluid_settings_register_int(settings, "audio.realtime-prio",
                FLUID_DEFAULT_AUDIO_RT_PRIO, 0, 99, 0);

  fluid_settings_register_str(settings, "audio.driver", "", 0);

  // 
  for(i = 0; i < G_N_ELEMENTS(fluid_audio_drivers) - 1; i++) {
    /* Select the default driver */
    if (def_name == NULL)
      def_name = fluid_audio_drivers[i].name;

    /* Add the driver to the list of options */
    fluid_settings_add_option(settings, "audio.driver", fluid_audio_drivers[i].name);

    if(fluid_audio_drivers[i].settings != NULL)
      fluid_audio_drivers[i].settings(settings);
  }

  /* Set the default driver, if any */
  if(def_name != NULL) 
    fluid_settings_setstr(settings, "audio.driver", def_name);
}

fluid_audio_driver_t * new_fluid_audio_driver(AudioDriverId adId, fluid_synth_t *synth) {
  const fluid_audriver_definition_t *def = getAuDriverDef(adId);

  if(def) {
    fluid_audio_driver_t *driver = (*def->new)(synth);
    if(driver) 
      driver->define = def;
    return driver;
  }

  return NULL;
}

fluid_audio_driver_t * new_fluid_audio_driver2(fluid_settings_t *settings, fluid_audio_func_t func, void *data) {
  const fluid_audriver_definition_t *def = getAuDriverDef(settings);

  if(def) {
    fluid_audio_driver_t *driver = NULL;

    if(def->new2 == NULL)
      FLUID_LOG(FLUID_DBG, "Callback mode unsupported on '%s' audio driver", def->name);
    else {
      driver = (*def->new2)(settings, func, data);

      if(driver)
        driver->define = def;
    }

    return driver;
  }

  return NULL;
}

void delete_fluid_audio_driver(fluid_audio_driver_t *driver) {
  fluid_return_if_fail(driver != NULL);
  driver->define->free(driver);
}
