#include "adriver.h"
#include "sys.h"
#include "synth.h"

/* adriverDefinitionT */
struct _AudriverDefinitionT {
  AudioDriverT *(*new)(Synthesizer *synth);
  AudioDriverT *(*new2)(enum _AuDriverId, AudioFuncT func, void *data);
  void (*free)(AudioDriverT *driver);
  void (*settings)(AudioSettings *settings);
};

// Maybe case-switch returning a pointer to a structure that only exists if supported.
// Access this array using the enums defined in the header file.
static AudriverDefinitionT auDriverA[] = {
#if ALSA_SUPPORT
   { newAlsaAudioDriver, newAlsaAudioDriver2, deleteAlsaAudioDriver, alsaAudioDriverSettings },
#endif
#if JACK_SUPPORT
   { newJackAudioDriver, newJackAudioDriver2, deleteJackAudioDriver, jackAudioDriverSettings },
#endif
#if PULSE_SUPPORT
   { newPulseAudioDriver, newPulseAudioDriver2, deletePulseAudioDriver, pulseAudioDriverSettings },
#endif
#if PIPEWIRE_SUPPORT
   { newPipewireAudioDriver, newPipewireAudioDriver2, deletePipewireAudioDriver, pipewireAudioDriverSettings },
#endif
#if OSS_SUPPORT
   { newOssAudioDriver, newOssAudioDriver2, deleteOssAudioDriver, ossAudioDriverSettings },
#endif
#if OBOE_SUPPORT
   { newOboeAudioDriver, NULL, deleteOboeAudioDriver, oboeAudioDriverSettings },
#endif
#if OPENSLES_SUPPORT
   { newOpenslesAudioDriver, NULL, deleteOpenslesAudioDriver, openslesAudioDriverSettings },
#endif
#if COREAUDIO_SUPPORT
   { newCoreAudioDriver, newCoreAudioDriver2, deleteCoreAudioDriver, coreAudioDriverSettings },
#endif
#if DSOUND_SUPPORT
   { newDsoundAudioDriver, newDsoundAudioDriver2, deleteDsoundAudioDriver, dsoundAudioDriverSettings },
#endif
#if WASAPI_SUPPORT
   { newWasapiAudioDriver, newWasapiAudioDriver2, deleteWasapiAudioDriver, wasapiAudioDriverSettings },
#endif
#if WAVEOUT_SUPPORT
   { newWaveoutAudioDriver, newWaveoutAudioDriver2, deleteWaveoutAudioDriver, waveoutAudioDriverSettings },
#endif
#if SNDMAN_SUPPORT
   { newSndmgrAudioDriver, newSndmgrAudioDriver2, deleteSndmgrAudioDriver, NULL }
#endif
#if PORTAUDIO_SUPPORT
   {newPortaudioDriver, NULL, deletePortaudioDriver, portaudioDriverSettings},
#endif
#if DART_SUPPORT
   { newDartAudioDriver, NULL, deleteDartAudioDriver, dartAudioDriverSettings },
#endif
#if SDL2_SUPPORT
   { newSdl2_audioDriver, NULL, deleteSdl2_audioDriver, sdl2_audioDriverSettings },
#endif
#if AUFILE_SUPPORT
   { newFileAudioDriver, NULL, deleteFileAudioDriver, NULL }
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

AudioDriverT* newAudioDriver(enum _AuDriverId adId, Synthesizer *synth) {
  const AudriverDefinitionT *def = &auDriverA[adId];
  if(def) {
    AudioDriverT *driver = (*def->new)(synth);
    if(driver) 
      driver->define = def;
    return driver;
  }
  return NULL;
}

AudioDriverT * newAudioDriver2(enum _AuDriverId adId, AudioFuncT func, void *data) {
  const AudriverDefinitionT *def = &auDriverA[adId];

  if(def) {
    AudioDriverT *driver = NULL;

    if(def->new2 != NULL) {
      driver = (*def->new2)(adId, func, data);

      if(driver)
        driver->define = def;

      return driver;
    }
  }

  return NULL;
}

void deleteAudioDriver(AudioDriverT *driver) {
  returnIfFail(driver != NULL);
  driver->define->free(driver);
}
