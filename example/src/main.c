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
    FluidSettings* settings = new_fluid_settings();
    fluid_settings_setstr(settings, "audio.driver", "alsa");
    fluid_settings_setnum(settings, "synth.gain", 2);
    fluid_synth_t* synth = new_fluid_synth(settings);
    //fluid_synth_sfload(synth, 
    Inflatable *sf3InfP = &sf3Inf;
    Error e = inflate(&sf3Inf);
    int sfId = -1;
    if (!e) {
      sfId = fluid_synth_sfload(synth, sf3Inf.inflatedDataP, sf3Inf.inflatedLen, 0);
      printf("returned sf id is %d.\n", sfId);
    }

    //float* buffer = calloc(SAMPLE_SIZE, NUM_SAMPLES);

    //FILE* file = argc > 2 ? fopen(argv[2], "wb") : stdout;

    if (sfId >= 0) {
      printf("playing\n");
      fluid_synth_noteon(synth, 0, 60, 127);
      //fluid_synth_write_float(synth, NUM_FRAMES, buffer, 0, NUM_CHANNELS, buffer, 1, NUM_CHANNELS);
      //fwrite(buffer, SAMPLE_SIZE, NUM_SAMPLES, file);
      sleep(1);
      fluid_synth_noteoff(synth, 0, 60);
      printf("done\n");
    }
    //fluid_synth_write_float(synth, NUM_FRAMES, buffer, 0, NUM_CHANNELS, buffer, 1, NUM_CHANNELS);
    //fwrite(buffer, SAMPLE_SIZE, NUM_SAMPLES, file);

    //fclose(file);

    //free(buffer);

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}
