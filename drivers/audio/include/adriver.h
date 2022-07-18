#ifndef _AUDRIVER_H
#define _AUDRIVER_H

#include "fluidbean.h"
//#include "audio.h"
#include "synth.h"

#ifdef __cplusplus
extern "C" {
#endif

//******************
//******************
// ENUMS
//******************
//******************

/* Available audio drivers, listed in order of preference */
// NOTE: It's critical this stays in the same order as the array in Adriver.c.
typedef enum _AuDriverId {
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
} AuDriverId;

typedef struct _AudriverDefinitionT AudriverDefinitionT;

typedef struct _AudioDriverT {
    const AudriverDefinitionT *define;
} AudioDriverT;

typedef int (*AudioFuncT)(void *data, int len,
                                  int nfx, float *fx[],
                                  int nout, float *out[]);

/** @startlifecycle{Audio Driver} */
AudioDriverT *newAudioDriver(AuDriverId auDriverId,
        Synthesizer *synth);

AudioDriverT *newAudioDriver2(AuDriverId auDriverId,
        AudioFuncT func,
        void *data);

void deleteAudioDriver(AudioDriverT *driver);
/** @endlifecycle */

int AudioDriverRegister(const char **adrivers);
/* @} */

typedef struct _AudioSettings {
  Setting audioSampleFormat;
  Setting audioPeriodSize;
  Setting audioPeriods;
  Setting audioRealTimePrio;
  Setting audioDriver;
} AudioSettings;




#if PULSE_SUPPORT
AudioDriverT *newPulseAudioDriver(Settings *settings,
        Synthesizer *synth);
AudioDriverT *newPulseAudioDriver2(Settings *settings,
        AudioFuncT func, void *data);
void deletePulseAudioDriver(AudioDriverT *p);
void PulseAudioDriverSettings(Settings *settings);
#endif

#if ALSA_SUPPORT
AudioDriverT *newAlsaAudioDriver(Settings *settings,
        Synthesizer *synth);
AudioDriverT *newAlsaAudioDriver2(Settings *settings,
        AudioFuncT func, void *data);
void deleteAlsaAudioDriver(AudioDriverT *p);
void AlsaAudioDriverSettings(Settings *settings);
#endif

#if OSS_SUPPORT
AudioDriverT *newOssAudioDriver(Settings *settings,
        Synthesizer *synth);
AudioDriverT *newOssAudioDriver2(Settings *settings,
        AudioFuncT func, void *data);
void deleteOssAudioDriver(AudioDriverT *p);
void OssAudioDriverSettings(Settings *settings);
#endif

#if OPENSLES_SUPPORT
AudioDriverT*
newOpenslesAudioDriver(Settings* settings,
		Synthesizer* synth);
void deleteOpenslesAudioDriver(AudioDriverT* p);
void OpenslesAudioDriverSettings(Settings* settings);
#endif

#if OBOE_SUPPORT
AudioDriverT*
newOboeAudioDriver(Settings* settings,
		Synthesizer* synth);
void deleteOboeAudioDriver(AudioDriverT* p);
void OboeAudioDriverSettings(Settings* settings);
#endif

#if COREAUDIO_SUPPORT
AudioDriverT *newCoreAudioDriver(Settings *settings,
        Synthesizer *synth);
AudioDriverT *newCoreAudioDriver2(Settings *settings,
        AudioFuncT func,
        void *data);
void deleteCoreAudioDriver(AudioDriverT *p);
void CoreAudioDriverSettings(Settings *settings);
#endif

#if DSOUND_SUPPORT
AudioDriverT *newDsoundAudioDriver(Settings *settings,
        Synthesizer *synth);
AudioDriverT *newDsoundAudioDriver2(Settings *settings,
        AudioFuncT func,
        void *data);
void deleteDsoundAudioDriver(AudioDriverT *p);
void DsoundAudioDriverSettings(Settings *settings);
#endif

#if WASAPI_SUPPORT
AudioDriverT *newWasapiAudioDriver(Settings *settings,
        Synthesizer *synth);
AudioDriverT *newWasapiAudioDriver2(Settings *settings,
        AudioFuncT func,
        void *data);
void deleteWasapiAudioDriver(AudioDriverT *p);
void WasapiAudioDriverSettings(Settings *settings);
#endif

#if WAVEOUT_SUPPORT
AudioDriverT *newWaveoutAudioDriver(Settings *settings,
        Synthesizer *synth);
AudioDriverT *newWaveoutAudioDriver2(Settings *settings,
        AudioFuncT func,
        void *data);
void deleteWaveoutAudioDriver(AudioDriverT *p);
void WaveoutAudioDriverSettings(Settings *settings);
#endif

#if PORTAUDIO_SUPPORT
void PortaudioDriverSettings(Settings *settings);
AudioDriverT *newPortaudioDriver(Settings *settings,
        Synthesizer *synth);
void deletePortaudioDriver(AudioDriverT *p);
#endif

#if JACK_SUPPORT
AudioDriverT *newJackAudioDriver(Settings *settings, Synthesizer *synth);
AudioDriverT *newJackAudioDriver2(Settings *settings,
        AudioFuncT func, void *data);
void deleteJackAudioDriver(AudioDriverT *p);
void JackAudioDriverSettings(Settings *settings);
int JackObtainSynth(Settings *settings, Synthesizer **synth);
#endif

#if PIPEWIRE_SUPPORT
AudioDriverT *newPipewireAudioDriver(Settings *settings, Synthesizer *synth);
AudioDriverT *newPipewireAudioDriver2(Settings *settings,
        AudioFuncT func, void *data);
void deletePipewireAudioDriver(AudioDriverT *p);
void PipewireAudioDriverSettings(Settings *settings);
#endif

#if SNDMAN_SUPPORT
AudioDriverT *newSndmgrAudioDriver(Settings *settings,
        Synthesizer *synth);
AudioDriverT *newSndmgrAudioDriver2(Settings *settings,
        AudioFuncT func,
        void *data);
void deleteSndmgrAudioDriver(AudioDriverT *p);
#endif

#if DART_SUPPORT
AudioDriverT *newDartAudioDriver(Settings *settings,
        Synthesizer *synth);
void deleteDartAudioDriver(AudioDriverT *p);
void DartAudioDriverSettings(Settings *settings);
#endif

#if SDL2_SUPPORT
AudioDriverT *newSdl2_audioDriver(Settings *settings,
        Synthesizer *synth);
void deleteSdl2_audioDriver(AudioDriverT *p);
void Sdl2_audioDriverSettings(Settings *settings);
#endif

#if AUFILE_SUPPORT
AudioDriverT *newFileAudioDriver(Settings *settings,
        Synthesizer *synth);
void deleteFileAudioDriver(AudioDriverT *p);
#endif

#ifdef __cplusplus
}
#endif

#endif  /* _AUDRIVER_H */
