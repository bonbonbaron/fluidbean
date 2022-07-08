#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "botox/data.h"

#include "fluidbean.h"

#define SAMPLE_RATE 44100
#define SAMPLE_SIZE sizeof(float)
#define NUM_FRAMES SAMPLE_RATE
#define NUM_CHANNELS 2
#define NUM_SAMPLES (NUM_FRAMES * NUM_CHANNELS)

extern Inflatable sf3Inf;
int main(int argc, char *argv[]) {
    FluidSettings* settings = newFluidSettings();
    settingsSetstr(settings, "audio.driver", "alsa");
    settingsSetnum(settings, "synth.gain", 2);
    synthT* synth = newFluidSynth(settings);
    //synthSfload(synth, 
    Inflatable *sf3InfP = &sf3Inf;
    Error e = inflate(&sf3Inf);
    int sfId = -1;
    if (!e) {
      sfId = synthSfload(synth, sf3Inf.inflatedDataP, sf3Inf.inflatedLen, 0);
      printf("returned sf id is %d.\n", sfId);
    }

    //float* buffer = calloc(SAMPLE_SIZE, NUM_SAMPLES);

    //FILE* file = argc > 2 ? fopen(argv[2], "wb") : stdout;

    if (sfId >= 0) {
      printf("playing\n");
      synthNoteon(synth, 0, 60, 127);
      //synthWriteFloat(synth, NUM_FRAMES, buffer, 0, NUM_CHANNELS, buffer, 1, NUM_CHANNELS);
      //fwrite(buffer, SAMPLE_SIZE, NUM_SAMPLES, file);
      sleep(1);
      synthNoteoff(synth, 0, 60);
      printf("done\n");
    }
    //synthWriteFloat(synth, NUM_FRAMES, buffer, 0, NUM_CHANNELS, buffer, 1, NUM_CHANNELS);
    //fwrite(buffer, SAMPLE_SIZE, NUM_SAMPLES, file);

    //fclose(file);

    //free(buffer);

    deleteFluidSynth(synth);
    deleteFluidSettings(settings);
}
