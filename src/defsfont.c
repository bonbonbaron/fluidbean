#include "fluidbean.h"
#include "stb_vorbis.c"
#include "synth.h"
#include "gen.h"
#include "soundfont.h"

#define keyVelAreInRange_(zoneP, key, vel) \
  (key) >= zoneP->keylo && (key) <= zoneP->keyhi && \
  (vel) >= zoneP->vello && (vel) <= zoneP->velhi

int fluid_defpreset_noteon (Preset *presetP, Synthesizer * synth, int chan, int key, int vel) {
  Modulator *modP, *modEndP;
  Zone *pzoneP, *global_preset_zone;
  Instrument *instP;
  Zone *izoneP, *globalIzoneP;
  Sample *sampleP;
  Voice *voiceP;
  Modulator *modP;
  Modulator *mod_list[FLUID_NUM_MOD]; /* list for 'sorting' preset modulators */
  Generator *genP, *genEndP;
  int mod_list_count;
  int i;

  global_preset_zone = presetP->globalZoneP;

  /* run thru all the zones of this preset */
  Zone *pzoneEndP = presetP->zoneA + presetP->nZones;
  for (Zone *pzoneP = presetP->zoneA; pzoneP < pzoneEndP; ++pzoneP) {
    /* check if the note falls into the key and velocity range of this preset */
    if (keyVelAreInRange_(pzoneP, key, vel)) {
      instP = pzoneP->u.instP;
      globalIzoneP = instP->globalZoneP;

      /* run thru all the zones of this instrument */
      Zone *izoneEndP = instP->zoneA + instP->nZones;
      for (izoneP = instP->zoneA; izoneP < izoneEndP; ++izoneP) {
        /* make sure this instrument zone has a valid sample */
        sampleP = izoneP->u.sampleP;
        if (fluid_sample_in_rom (sampleP) || (sampleP == NULL)) 
          continue;
        /* check if the note falls into the key and velocity range of this instrument */
        if (keyVelAreInRange_(izoneP, key, vel) && sampleP != NULL) {
          /* this is a good zone. allocate a new synthesis process and initialize it */
          voiceP = fluid_synth_alloc_voice (synth, sampleP, chan, key, vel);
          if (voiceP == NULL) 
            return FLUID_FAILED;
          /* Instrument level, generators */
          genEndP = izoneP->genA + izoneP->nGens;
          for (genP = izoneP->genA; genP < genEndP; ++genP) {
            /* SF 2.01 section 9.4 'bullet' 4:
             *
             *A generator in a local instrument zone supersedes a
             *global instrument zone generator.  Both cases supersede
             *the default generator -> voice_gen_set */
            if (genP->flags) 
              fluid_voice_gen_set (voiceP, i, izoneP->gen[i].val);
            else if ((globalIzoneP != NULL) && (globalIzoneP->gen[i].flags)) 
              fluid_voice_gen_set (voiceP, i, globalIzoneP->gen[i].val);
          }                     /* for all generators */

          /* global instrument zone, modulators: Put them all into a
           *list. */

          mod_list_count = 0;

          if (globalIzoneP) {
            modEndP = globalIzoneP->modA + globalIzoneP->nMods;
              mod_list[mod_list_count++] = mod;
          }

          /* local instrument zone, modulators.
           *Replace modulators with the same definition in the list:
           *SF 2.01 page 69, 'bullet' 8
           */
          mod = izoneP->mod;

          while (mod) {

            /* 'Identical' modulators will be deleted by setting their
             * list entry to NULL.  The list length is known, NULL
             * entries will be ignored later.  SF2.01 section 9.5.1
             * page 69, 'bullet' 3 defines 'identical'.  */

            // MB test for identity another, faster way.
            for (i = 0; i < mod_list_count; i++) {
              if (mod_list[i] && Modulatorest_identity (mod, mod_list[i])) {
                mod_list[i] = NULL;
              }
            }

            /* Finally add the new modulator to to the list. */
            mod_list[mod_list_count++] = mod;
            mod = mod->next;
          }

          /* Add instrument modulators (global / local) to the voiceP. */
          for (i = 0; i < mod_list_count; i++) {

            mod = mod_list[i];

            if (mod != NULL) {  /* disabled modulators CANNOT be skipped. */

              /* Instrument modulators -supersede- existing (default)
               *modulators.  SF 2.01 page 69, 'bullet' 6 */
              fluid_voice_add_mod (voiceP, mod, FLUID_VOICE_OVERWRITE);
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

              if (pzoneP->gen[i].flags) {
                fluid_voice_gen_incr (voiceP, i, pzoneP->gen[i].val);
              } else if ((global_preset_zone != NULL)
                         && global_preset_zone->gen[i].flags) {
                fluid_voice_gen_incr (voiceP, i,
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

          mod = pzoneP->mod;
          while (mod) {
            for (i = 0; i < mod_list_count; i++) {
              if (mod_list[i] && Modulatorest_identity (mod, mod_list[i])) {
                mod_list[i] = NULL;
              }
            }

            /* Finally add the new modulator to the list. */
            mod_list[mod_list_count++] = mod;
            mod = mod->next;
          }

          /* Add preset modulators (global / local) to the voiceP. */
          for (i = 0; i < mod_list_count; i++) {
            mod = mod_list[i];
            if ((mod != NULL) && (mod->amount != 0)) {  /* disabled modulators can be skipped. */

              /* Preset modulators -add- to existing instrument /
               *default modulators.  SF2.01 page 70 first bullet on
               *page */
              fluid_voice_add_mod (voiceP, mod, FLUID_VOICE_ADD);
            }
          }

          /* add the synthesis process to the synthesis loop. */
          fluid_synth_start_voice (synth, voiceP);

          /* Store the ID of the first voiceP that was created by this noteon event.
           *Exclusive class may only terminate older voices.
           *That avoids killing voices, which have just been created.
           *(a noteon event can create several voice processes with the same exclusive
           *class - for example when using stereo samples)
           */
        }
      }  // if note/vel falls in izone's range
    }  // if note/vel falls in pzone's range
    pzoneP = fluid_preset_zone_next (pzoneP);
  }

  return FLUID_OK;
}
