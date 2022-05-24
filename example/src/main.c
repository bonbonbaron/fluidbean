#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "fluidlite.h"

#define SAMPLE_RATE 44100
#define SAMPLE_SIZE sizeof(float)
#define NUM_FRAMES SAMPLE_RATE
#define NUM_CHANNELS 2
#define NUM_SAMPLES (NUM_FRAMES * NUM_CHANNELS)

int main(int argc, char *argv[]) {
    if (argc < 2) {
      printf("Usage: %s <soundfont> [<output>]\n", argv[0]);
      return 1;
    }

    fluid_settings_t* settings = new_fluid_settings();
    fluid_settings_setstr(settings, "audio.driver", "alsa");
    fluid_settings_setnum(settings, "synth.gain", 2);
    fluid_synth_t* synth = new_fluid_synth(settings);
    fluid_synth_sfload(synth, argv[1], 1);

    //float* buffer = calloc(SAMPLE_SIZE, NUM_SAMPLES);

    //FILE* file = argc > 2 ? fopen(argv[2], "wb") : stdout;

    printf("playing\n");
    fluid_synth_noteon(synth, 0, 60, 127);
    //fluid_synth_write_float(synth, NUM_FRAMES, buffer, 0, NUM_CHANNELS, buffer, 1, NUM_CHANNELS);
    //fwrite(buffer, SAMPLE_SIZE, NUM_SAMPLES, file);
    sleep(1);
    fluid_synth_noteoff(synth, 0, 60);
    printf("done\n");
    //fluid_synth_write_float(synth, NUM_FRAMES, buffer, 0, NUM_CHANNELS, buffer, 1, NUM_CHANNELS);
    //fwrite(buffer, SAMPLE_SIZE, NUM_SAMPLES, file);

    //fclose(file);

    //free(buffer);

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}