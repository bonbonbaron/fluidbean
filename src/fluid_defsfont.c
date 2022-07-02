#include "include/fluid_defsfont.h"
#include "data.h"
#include "include/fluid_sfont.h"
#include "include/fluid_sys.h"
#include "stb_vorbis.c"

int fluid_defpreset_noteon (fluid_defpreset_t * preset, fluid_synth_t * synth, int chan, int key, int vel) {
  fluid_preset_zone_t *preset_zone, *global_preset_zone;
  fluid_inst_t *inst;
  fluid_inst_zone_t *inst_zone, *global_inst_zone, *z;
  fluid_sample_t *sample;
  fluid_voice_t *voice;
  fluid_mod_t *mod;
  fluid_mod_t *mod_list[FLUID_NUM_MOD]; /* list for 'sorting' preset modulators */
  int mod_list_count;
  int i;

  global_preset_zone = fluid_defpreset_get_global_zone (preset);

  /* run thru all the zones of this preset */
  preset_zone = fluid_defpreset_get_zone (preset);
  while (preset_zone != NULL) {

    /* check if the note falls into the key and velocity range of this
       preset */
    if (fluid_preset_zone_inside_range (preset_zone, key, vel)) {

      inst = fluid_preset_zone_get_inst (preset_zone);
      global_inst_zone = fluid_inst_get_global_zone (inst);

      /* run thru all the zones of this instrument */
      inst_zone = fluid_inst_get_zone (inst);
      while (inst_zone != NULL) {

        /* make sure this instrument zone has a valid sample */
        sample = fluid_inst_zone_get_sample (inst_zone);
        if (fluid_sample_in_rom (sample) || (sample == NULL)) {
          inst_zone = fluid_inst_zone_next (inst_zone);
          continue;
        }

        /* check if the note falls into the key and velocity range of this
           instrument */

        if (fluid_inst_zone_inside_range (inst_zone, key, vel)
            && (sample != NULL)) {

          /* this is a good zone. allocate a new synthesis process and
             initialize it */

          voice = fluid_synth_alloc_voice (synth, sample, chan, key, vel);
          if (voice == NULL) {
            return FLUID_FAILED;
          }


          z = inst_zone;

          /* Instrument level, generators */

          for (i = 0; i < GEN_LAST; i++) {

            /* SF 2.01 section 9.4 'bullet' 4:
             *
             *A generator in a local instrument zone supersedes a
             *global instrument zone generator.  Both cases supersede
             *the default generator -> voice_gen_set */

            if (inst_zone->gen[i].flags) {
              fluid_voice_gen_set (voice, i, inst_zone->gen[i].val);

            } else if ((global_inst_zone != NULL)
                       && (global_inst_zone->gen[i].flags)) {
              fluid_voice_gen_set (voice, i, global_inst_zone->gen[i].val);

            } else {
              /* The generator has not been defined in this instrument.
               *Do nothing, leave it at the default.
               */
            }

          }                     /* for all generators */

          /* global instrument zone, modulators: Put them all into a
           *list. */

          mod_list_count = 0;

          if (global_inst_zone) {
            mod = global_inst_zone->mod;
            while (mod) {
              mod_list[mod_list_count++] = mod;
              mod = mod->next;
            }
          }

          /* local instrument zone, modulators.
           *Replace modulators with the same definition in the list:
           *SF 2.01 page 69, 'bullet' 8
           */
          mod = inst_zone->mod;

          while (mod) {

            /* 'Identical' modulators will be deleted by setting their
             * list entry to NULL.  The list length is known, NULL
             * entries will be ignored later.  SF2.01 section 9.5.1
             * page 69, 'bullet' 3 defines 'identical'.  */

            for (i = 0; i < mod_list_count; i++) {
              if (mod_list[i] && fluid_mod_test_identity (mod, mod_list[i])) {
                mod_list[i] = NULL;
              }
            }

            /* Finally add the new modulator to to the list. */
            mod_list[mod_list_count++] = mod;
            mod = mod->next;
          }

          /* Add instrument modulators (global / local) to the voice. */
          for (i = 0; i < mod_list_count; i++) {

            mod = mod_list[i];

            if (mod != NULL) {  /* disabled modulators CANNOT be skipped. */

              /* Instrument modulators -supersede- existing (default)
               *modulators.  SF 2.01 page 69, 'bullet' 6 */
              fluid_voice_add_mod (voice, mod, FLUID_VOICE_OVERWRITE);
            }
          }

          /* Preset level, generators */

          for (i = 0; i < GEN_LAST; i++) {

            /* SF 2.01 section 8.5 page 58: If some generators are
             *encountered at preset level, they should be ignored */
            if ((i != GEN_STARTADDROFS)
                && (i != GEN_ENDADDROFS)
                && (i != GEN_STARTLOOPADDROFS)
                && (i != GEN_ENDLOOPADDROFS)
                && (i != GEN_STARTADDRCOARSEOFS)
                && (i != GEN_ENDADDRCOARSEOFS)
                && (i != GEN_STARTLOOPADDRCOARSEOFS)
                && (i != GEN_KEYNUM)
                && (i != GEN_VELOCITY)
                && (i != GEN_ENDLOOPADDRCOARSEOFS)
                && (i != GEN_SAMPLEMODE)
                && (i != GEN_EXCLUSIVECLASS)
                && (i != GEN_OVERRIDEROOTKEY)) {

              /* SF 2.01 section 9.4 'bullet' 9: A generator in a
               *local preset zone supersedes a global preset zone
               *generator.  The effect is -added- to the destination
               *summing node -> voice_gen_incr */

              if (preset_zone->gen[i].flags) {
                fluid_voice_gen_incr (voice, i, preset_zone->gen[i].val);
              } else if ((global_preset_zone != NULL)
                         && global_preset_zone->gen[i].flags) {
                fluid_voice_gen_incr (voice, i,
                                      global_preset_zone->gen[i].val);
              } else {
                /* The generator has not been defined in this preset
                 *Do nothing, leave it unchanged.
                 */
              }
            }                   /* if available at preset level */
          }                     /* for all generators */


          /* Global preset zone, modulators: put them all into a
           *list. */
          mod_list_count = 0;
          if (global_preset_zone) {
            mod = global_preset_zone->mod;
            while (mod) {
              mod_list[mod_list_count++] = mod;
              mod = mod->next;
            }
          }

          /* Process the modulators of the local preset zone.  Kick
           *out all identical modulators from the global preset zone
           *(SF 2.01 page 69, second-last bullet) */

          mod = preset_zone->mod;
          while (mod) {
            for (i = 0; i < mod_list_count; i++) {
              if (mod_list[i] && fluid_mod_test_identity (mod, mod_list[i])) {
                mod_list[i] = NULL;
              }
            }

            /* Finally add the new modulator to the list. */
            mod_list[mod_list_count++] = mod;
            mod = mod->next;
          }

          /* Add preset modulators (global / local) to the voice. */
          for (i = 0; i < mod_list_count; i++) {
            mod = mod_list[i];
            if ((mod != NULL) && (mod->amount != 0)) {  /* disabled modulators can be skipped. */

              /* Preset modulators -add- to existing instrument /
               *default modulators.  SF2.01 page 70 first bullet on
               *page */
              fluid_voice_add_mod (voice, mod, FLUID_VOICE_ADD);
            }
          }

          /* add the synthesis process to the synthesis loop. */
          fluid_synth_start_voice (synth, voice);

          /* Store the ID of the first voice that was created by this noteon event.
           *Exclusive class may only terminate older voices.
           *That avoids killing voices, which have just been created.
           *(a noteon event can create several voice processes with the same exclusive
           *class - for example when using stereo samples)
           */
        }

        inst_zone = fluid_inst_zone_next (inst_zone);
      }
    }
    preset_zone = fluid_preset_zone_next (preset_zone);
  }

  return FLUID_OK;
}
