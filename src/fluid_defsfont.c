/* FluidSynth - A Software Synthesizer
 *
 *Copyright (C) 2003  Peter Hanappe and others.
 *
 *SoundFont file loading code borrowed from Smurf SoundFont Editor
 *Copyright (C) 1999-2001 Josh Green
 *
 *This library is free software; you can redistribute it and/or
 *modify it under the terms of the GNU Library General Public License
 *as published by the Free Software Foundation; either version 2 of
 *the License, or (at your option) any later version.
 *
 *This library is distributed in the hope that it will be useful, but
 *WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *Library General Public License for more details.
 *
 *You should have received a copy of the GNU Library General Public
 *License along with this library; if not, write to the Free
 *Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *02111-1307, USA
 */


#include "include/fluid_defsfont.h"
#include "data.h"
#include "include/fluid_sfont.h"
#include "include/fluid_sys.h"
#include "stb_vorbis.c"
// We only use SF3s, and we only decode them. Hence STB_Vorbis.

#ifdef WORDS_BIGENDIAN
#define readChunk_(dstP_, srcP_) G_STMT_START {      \
  readNBytes_(srcP_, dstP_, 8); \
  ((SFChunk *)(dstP_))->size = GUINT32_FROM_BE(((SFChunk *)(dstP_))->size);  \
} G_STMT_END
#else
#define readChunk_(dstP_, srcP_) G_STMT_START {      \
  readNBytes_(srcP_, dstP_, 8); \
  ((SFChunk *)(dstP_))->size = GUINT32_FROM_LE(((SFChunk *)(dstP_))->size);  \
} G_STMT_END
#endif

#define readId_(dstP_, srcP_)    G_STMT_START {        \
  readNBytes_(srcP_, dstP_, 4); \
} G_STMT_END

#define readStr_(dstP_, srcP_)   G_STMT_START {      \
  readNBytes_(srcP_, dstP_, 20); \
  (dstP_)[20] = '\0';          \
} G_STMT_END

#ifdef WORDS_BIGENDIAN
#define readU32_(dst_, srcP_)    G_STMT_START {        \
  U32 _temp;         \
  readNBytes_(srcP_, &_temp, 4); \
  dst_ = GINT32_FROM_BE(_temp);     \
} G_STMT_END
#else
#define readU32_(dst_, srcP_)    G_STMT_START {        \
  U32 _temp;         \
  readNBytes_(srcP_, &_temp, 4); \
  dst_ = GINT32_FROM_LE(_temp);     \
} G_STMT_END
#endif

#ifdef WORDS_BIGENDIAN
// MB: I know, I know... I goofed up the argument ordering. Couldn't make up my mind!
#define readU16_(srcP_, dst_)    G_STMT_START {        \
  unsigned short _temp;         \
  readNBytes_(srcP_, &_temp, 2); \
  dst_ = GINT16_FROM_BE(_temp);     \
} G_STMT_END
#else
#define readU16_(srcP_, dst_)    G_STMT_START {        \
  unsigned short _temp;         \
  readNBytes_(srcP_, &_temp, 2); \
  dst_ = GINT16_FROM_LE(_temp);     \
} G_STMT_END
#endif

#define readU8_(dstP_, srcP_)   G_STMT_START {        \
  readNBytes_(srcP_, dstP_, 1); \
} G_STMT_END

#define skip_(srcP_, nBytes_)   G_STMT_START {    \
  srcP_ += nBytes_; \
} G_STMT_END

#define skipU16_(srcP_)   G_STMT_START {      \
  srcP_ += 2; \
} G_STMT_END

#define readNBytes_(srcP_, dstP_, nBytes_) \
  memcpy(dstP_, srcP_, nBytes_); \
  srcP_ += nBytes_;

/* removes and advances a fluid_list_t pointer */
#define SLADVREM(list, item)  G_STMT_START {    \
    fluid_list_t *_temp = item;       \
    item = fluid_list_next(item);       \
    list = fluid_list_remove_link(list, _temp);   \
    delete1_fluid_list(_temp);        \
} G_STMT_END

static int load_body (void *sfDataP, int dataLen, U8 **srcPP, SFData * sfP);
static int read_listchunk (SFChunk **chunkPP, U8 **srcPP);
static int process_info (int size, SFData *sfP, U8 **srcPP);
static int process_sdta (int size, SFData * sf, void *sfDataP, U8 **srcPP);
static int pdtahelper (U8 ** srcPP, U32 expid, U32 reclen,
                       SFChunk * chunk, int *size);
static int process_pdta (int size, SFData * sf, U8 **srcPP);
static int load_phdr (U8 **srcPP, int size, SFData *sf);
static int load_pbag (U8 **srcPP, int size, SFData *sf);
static int load_pmod (U8 **srcPP, int size, SFData *sf);
static int load_pgen (U8 **srcPP, int size, SFData *sf);
static int load_ihdr (U8 **srcPP, int size, SFData *sf);
static int load_ibag (U8 **srcPP, int size, SFData *sf);
static int load_imod (U8 **srcPP, int size, SFData *sf);
static int load_igen (U8 **srcPP, int size, SFData *sf);
static int load_shdr (U8 **srcPP, U32 size, SFData *sf);
static int fixup_pgen (SFData *sfP);
static int fixup_igen (SFData *sfP);
static int fixup_sample (SFData *sfP);

/***************************************************************
 *
 *                          SFONT LOADER
 */

fluid_sfont_t *fluid_defsfloader_load(void *sfontPrevDataP, void *sfDataP, int sfDataLen) {
  fluid_defsfont_t *defsfont;
  fluid_sfont_t *sfont;

  defsfont = new_fluid_defsfont ();
  if (defsfont == NULL) return NULL;

  sfont = sfontPrevDataP ? (fluid_sfont_t *) sfontPrevDataP : FLUID_NEW (fluid_sfont_t);
  if (sfont == NULL) return NULL;

  sfont->data = defsfont;
  sfont->free = fluid_defsfont_sfont_delete;
  sfont->get_preset = fluid_defsfont_sfont_get_preset;
  sfont->iteration_start = fluid_defsfont_sfont_iteration_start;
  sfont->iteration_next = fluid_defsfont_sfont_iteration_next;

  if (fluid_defsfont_load(defsfont, sfDataP, sfDataLen) == FLUID_FAILED) {
    delete_fluid_defsfont (defsfont);
    return NULL;
  }

  return sfont;
}



/* PUBLIC INTERFACE */

int fluid_defsfont_sfont_delete (fluid_sfont_t * sfont) {
  if (delete_fluid_defsfont (sfont->data) != 0) {
    return -1;
  }
  FLUID_FREE (sfont);
  return 0;
}

fluid_preset_t *fluid_defsfont_sfont_get_preset (fluid_sfont_t * sfont,
                                                 U32 bank,
                                                 U32 prenum) {
  fluid_preset_t *preset;
  fluid_defpreset_t *defpreset;

  defpreset = fluid_defsfont_get_preset ((fluid_defsfont_t *) sfont->data, bank, prenum);

  if (defpreset == NULL) 
    return NULL;

  preset = FLUID_NEW (fluid_preset_t);
  if (preset == NULL) 
    return NULL;

  preset->sfont = sfont;
  preset->data = defpreset;
  preset->free = fluid_defpreset_preset_delete;
  preset->get_banknum = fluid_defpreset_preset_get_banknum;
  preset->get_num = fluid_defpreset_preset_get_num;
  preset->noteon = fluid_defpreset_preset_noteon;
  preset->notify = NULL;

  return preset;
}

void fluid_defsfont_sfont_iteration_start (fluid_sfont_t * sfont) {
  fluid_defsfont_iteration_start ((fluid_defsfont_t *) sfont->data);
}

int
fluid_defsfont_sfont_iteration_next (fluid_sfont_t * sfont,
                                     fluid_preset_t * preset) {
  preset->free = fluid_defpreset_preset_delete;
  preset->get_banknum = fluid_defpreset_preset_get_banknum;
  preset->get_num = fluid_defpreset_preset_get_num;
  preset->noteon = fluid_defpreset_preset_noteon;
  preset->notify = NULL;

  return fluid_defsfont_iteration_next ((fluid_defsfont_t *) sfont->data,
                                        preset);
}

int fluid_defpreset_preset_delete (fluid_preset_t * preset) {
  FLUID_FREE (preset);
  /* TODO: free modulators */
  return 0;
}

int fluid_defpreset_preset_get_banknum (fluid_preset_t * preset) {
  return fluid_defpreset_get_banknum ((fluid_defpreset_t *) preset->data);
}

int fluid_defpreset_preset_get_num (fluid_preset_t * preset) {
  return fluid_defpreset_get_num ((fluid_defpreset_t *) preset->data);
}

int
fluid_defpreset_preset_noteon (fluid_preset_t * preset, fluid_synth_t * synth,
                               int chan, int key, int vel) {
  return fluid_defpreset_noteon ((fluid_defpreset_t *) preset->data, synth,
                                 chan, key, vel);
}

// SFONT
fluid_defsfont_t *new_fluid_defsfont () {
  fluid_defsfont_t *sfont;

  sfont = FLUID_NEW (fluid_defsfont_t);
  if (sfont == NULL) {
    FLUID_LOG (FLUID_ERR, "Out of memory");
    return NULL;
  }

  sfont->samplepos = 0;
  sfont->samplesize = 0;
  sfont->sample = NULL;
  sfont->sampledata = NULL;
  sfont->preset = NULL;

  return sfont;
}

/*
 *delete_fluid_defsfont
 */
int delete_fluid_defsfont (fluid_defsfont_t * sfont) {
  fluid_list_t *list;
  fluid_defpreset_t *preset;
  fluid_sample_t *sample;

  /* Check that no samples are currently used */
  for (list = sfont->sample; list; list = fluid_list_next (list)) {
    sample = (fluid_sample_t *) fluid_list_get (list);
    if (fluid_sample_refcount (sample) != 0) {
      return -1;
    }
  }

  for (list = sfont->sample; list; list = fluid_list_next (list)) {
    delete_fluid_sample ((fluid_sample_t *) fluid_list_get (list));
  }

  if (sfont->sample) {
    delete_fluid_list (sfont->sample);
  }

  if (sfont->sampledata != NULL) {
    FLUID_FREE (sfont->sampledata);
  }

  preset = sfont->preset;
  while (preset != NULL) {
    sfont->preset = preset->next;
    delete_fluid_defpreset (preset);
    preset = sfont->preset;
  }

  FLUID_FREE (sfont);
  return FLUID_OK;
}

void (*preset_callback) (U32 bank, U32 num, char *name) =
  NULL;
void fluid_synth_set_preset_callback (void *callback) {
  preset_callback = callback;
}

int fluid_defsfont_load (fluid_defsfont_t * sfont, void *sfDataP, int sfDataLen) {
  SFData *sfdata;
  fluid_list_t *p;
  SFPreset *sfpreset;
  SFSample *sfsample;
  fluid_sample_t *sample;
  fluid_defpreset_t *preset;
  U8 *inP = (U8*) sfDataP;

  /* The actual loading is done in the sfont and sffile files */
  sfdata = sfLoadMem(sfDataP, sfDataLen, &inP);
  if (sfdata == NULL) 
    return FLUID_FAILED;

  /* Keep track of the position and size of the sample data because
     it's loaded separately (and might be unoaded/reloaded in future) */
  sfont->samplepos = sfdata->samplepos;
  sfont->samplesize = sfdata->samplesize;

  /* load sample data in one block */
  if (fluid_defsfont_load_sampledata(sfont, sfDataP) != FLUID_OK)
    goto err_exit;

  /* Create all the sample headers */
  p = sfdata->sample;
  while (p != NULL) {
    sfsample = (SFSample *) p->data;

    sample = new_fluid_sample ();
    if (sample == NULL)
      goto err_exit;

    if (fluid_sample_import_sfont (sample, sfsample, sfont) != FLUID_OK)
      goto err_exit;

    fluid_defsfont_add_sample (sfont, sample);
    fluid_voice_optimize_sample (sample);
    p = fluid_list_next (p);
  }

  /* Load all the presets */
  p = sfdata->preset;
  while (p != NULL) {
    sfpreset = (SFPreset *) p->data;
    preset = new_fluid_defpreset (sfont);
    if (preset == NULL)
      goto err_exit;

    if (fluid_defpreset_import_sfont (preset, sfpreset, sfont) != FLUID_OK)
      goto err_exit;

    fluid_defsfont_add_preset (sfont, preset);
    if (preset_callback)
      preset_callback (preset->bank, preset->num, preset->name);
    p = fluid_list_next (p);
  }
  sfont_close (sfdata);

  return FLUID_OK;

err_exit:
  sfont_close (sfdata);
  return FLUID_FAILED;
}

/* fluid_defsfont_add_sample
 *
 *Add a sample to the SoundFont
 */
int fluid_defsfont_add_sample (fluid_defsfont_t * sfont, fluid_sample_t * sample) {
  sfont->sample = fluid_list_append (sfont->sample, sample);
  return FLUID_OK;
}

/* fluid_defsfont_add_preset
 *
 *Add a preset to the SoundFont
 */
int fluid_defsfont_add_preset (fluid_defsfont_t * sfont, fluid_defpreset_t * preset) {
  fluid_defpreset_t *cur, *prev;
  if (sfont->preset == NULL) {
    preset->next = NULL;
    sfont->preset = preset;
  } else {
    /* sort them as we go along. very basic sorting trick. */
    cur = sfont->preset;
    prev = NULL;
    while (cur != NULL) {
      if ((preset->bank < cur->bank)
          || ((preset->bank == cur->bank) && (preset->num < cur->num))) {
        if (prev == NULL) {
          preset->next = cur;
          sfont->preset = preset;
        } else {
          preset->next = cur;
          prev->next = preset;
        }
        return FLUID_OK;
      }
      prev = cur;
      cur = cur->next;
    }
    preset->next = NULL;
    prev->next = preset;
  }
  return FLUID_OK;
}

/*
 *fluid_defsfont_load_sampledata
 */
int fluid_defsfont_load_sampledata(fluid_defsfont_t *sfont, void *sfDataP) {
  unsigned short endian;
  // Put data at sample of interest.
  U8 *dataP = ((U8*) sfDataP) + sfont->samplepos;
  sfont->sampledata = (short *) FLUID_MALLOC (sfont->samplesize);
  if (sfont->sampledata == NULL) 
    return FLUID_FAILED;
  // Read sample into sfont.
  memcpy(sfont->sampledata, dataP, sfont->samplesize);

  /* I'm not sure this endian test is waterproof...  */
  endian = 0x0100;

  // MB TODO just improved performance of below, but need to change above.
  /* If this machine is big endian, the sample have to byte swapped  */
  if (((char *) &endian)[0]) {
    short *sP = &sfont->sampledata[0];
    short *sEndP = sP + (sfont->samplesize >> 1);
    for (; sP < sEndP; ++sP)
      *sP = ((*sP & 0x00ff) << 8) | ((*sP & 0xff00) >> 8);
  }
  return FLUID_OK;
}

/*
 *fluid_defsfont_get_sample
 */
fluid_sample_t *fluid_defsfont_get_sample (fluid_defsfont_t * sfont, char *s) {
  fluid_list_t *list;
  fluid_sample_t *sample;

  for (list = sfont->sample; list; list = fluid_list_next (list)) {

    sample = (fluid_sample_t *) fluid_list_get (list);

    if (FLUID_STRCMP (sample->name, s) == 0) {

#if SF3_SUPPORT
      if (sample->sampletype & FLUID_SAMPLETYPE_OGG_VORBIS) {
        short *sampledata = NULL;
        int sampleframes = 0;

#if SF3_SUPPORT == SF3_XIPH_VORBIS
        int sampledata_size = 0;
        OggVorbis_File vf;

        vorbisData.pos = 0;
        vorbisData.data = (char *) sample->data + sample->start;
        vorbisData.datasize = sample->end + 1 - sample->start;

        if (ov_open_callbacks (&vorbisData, &vf, 0, 0, ovCallbacks) == 0) {
#define BUFFER_SIZE 4096
          int bytes_read = 0;
          int section = 0;
          for (;;) {
            // allocate additional memory for samples
            sampledata = realloc (sampledata, sampledata_size + BUFFER_SIZE);
            bytes_read =
              ov_read (&vf, (char *) sampledata + sampledata_size,
                       BUFFER_SIZE, 0, sizeof (short), 1, &section);
            if (bytes_read > 0) {
              sampledata_size += bytes_read;
            } else {
              // shrink sampledata to actual size
              sampledata = realloc (sampledata, sampledata_size);
              break;
            }
          }

          ov_clear (&vf);
        }
        // because we actually need num of frames so we should divide num of bytes to frame size
        sampleframes = sampledata_size / sizeof (short);
#endif

#if SF3_SUPPORT == SF3_STB_VORBIS
        const uint8 *data = (uint8 *) sample->data + sample->start;
        const int datasize = sample->end + 1 - sample->start;

        int channels;
        sampleframes =
          stb_vorbis_decode_memory (data, datasize, &channels, NULL,
                                    &sampledata);
#endif
        // point sample data to uncompressed data stream
        sample->data = sampledata;
        sample->start = 0;
        sample->end = sampleframes - 1;

        /* loop is fowled?? (cluck cluck :) */
        if (sample->loopend > sample->end ||
            sample->loopstart >= sample->loopend ||
            sample->loopstart <= sample->start) {
          /* can pad loop by 8 samples and ensure at least 4 for loop (2*8+4) */
          if ((sample->end - sample->start) >= 20) {
            sample->loopstart = sample->start + 8;
            sample->loopend = sample->end - 8;
          } else {              /* loop is fowled, sample is tiny (can't pad 8 samples) */
            sample->loopstart = sample->start + 1;
            sample->loopend = sample->end - 1;
          }
        }

        sample->sampletype &= ~FLUID_SAMPLETYPE_OGG_VORBIS;
        sample->sampletype |= FLUID_SAMPLETYPE_OGG_VORBIS_UNPACKED;

        fluid_voice_optimize_sample (sample);
      }
#endif

      return sample;
    }
  }

  return NULL;
}

/*
 *fluid_defsfont_get_preset
 */
fluid_defpreset_t *fluid_defsfont_get_preset (fluid_defsfont_t * sfont,
                                              U32 bank,
                                              U32 num) {
  fluid_defpreset_t *preset = sfont->preset;
  while (preset != NULL) {
    if ((preset->bank == bank) && ((preset->num == num))) {
      return preset;
    }
    preset = preset->next;
  }
  return NULL;
}

/*
 *fluid_defsfont_iteration_start
 */
void fluid_defsfont_iteration_start (fluid_defsfont_t * sfont) {
  sfont->iter_cur = sfont->preset;
}

/*
 *fluid_defsfont_iteration_next
 */
int
fluid_defsfont_iteration_next (fluid_defsfont_t * sfont,
                               fluid_preset_t * preset) {
  if (sfont->iter_cur == NULL) {
    return 0;
  }

  preset->data = (void *) sfont->iter_cur;
  sfont->iter_cur = fluid_defpreset_next (sfont->iter_cur);
  return 1;
}

/***************************************************************
 *
 *                          PRESET
 */

/*
 *new_fluid_defpreset
 */
fluid_defpreset_t *new_fluid_defpreset (fluid_defsfont_t * sfont) {
  fluid_defpreset_t *preset = FLUID_NEW (fluid_defpreset_t);
  if (preset == NULL) {
    FLUID_LOG (FLUID_ERR, "Out of memory");
    return NULL;
  }
  preset->next = NULL;
  preset->sfont = sfont;
  preset->name[0] = 0;
  preset->bank = 0;
  preset->num = 0;
  preset->global_zone = NULL;
  preset->zone = NULL;
  return preset;
}

/*
 *delete_fluid_defpreset
 */
int delete_fluid_defpreset (fluid_defpreset_t * preset) {
  int err = FLUID_OK;
  fluid_preset_zone_t *zone;
  if (preset->global_zone != NULL) {
    if (delete_fluid_preset_zone (preset->global_zone) != FLUID_OK) {
      err = FLUID_FAILED;
    }
    preset->global_zone = NULL;
  }
  zone = preset->zone;
  while (zone != NULL) {
    preset->zone = zone->next;
    if (delete_fluid_preset_zone (zone) != FLUID_OK) {
      err = FLUID_FAILED;
    }
    zone = preset->zone;
  }
  FLUID_FREE (preset);
  return err;
}

int fluid_defpreset_get_banknum (fluid_defpreset_t * preset) {
  return preset->bank;
}

int fluid_defpreset_get_num (fluid_defpreset_t * preset) {
  return preset->num;
}

/*
 *fluid_defpreset_next
 */
fluid_defpreset_t *fluid_defpreset_next (fluid_defpreset_t * preset) {
  return preset->next;
}


/*
 *fluid_defpreset_noteon
 */
int
fluid_defpreset_noteon (fluid_defpreset_t * preset, fluid_synth_t * synth,
                        int chan, int key, int vel) {
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

/*
 *fluid_defpreset_set_global_zone
 */
int
fluid_defpreset_set_global_zone (fluid_defpreset_t * preset,
                                 fluid_preset_zone_t * zone) {
  preset->global_zone = zone;
  return FLUID_OK;
}

/*
 *fluid_defpreset_import_sfont
 */
int
fluid_defpreset_import_sfont (fluid_defpreset_t * preset,
                              SFPreset * sfpreset, fluid_defsfont_t * sfont) {
  fluid_list_t *p;
  SFZone *sfzone;
  fluid_preset_zone_t *zone;
  int count;
  char zone_name[256];
  if (FLUID_STRLEN (sfpreset->name) > 0) {
    FLUID_STRCPY (preset->name, sfpreset->name);
  } else {
    FLUID_SPRINTF (preset->name, "Bank%d,Preset%d", sfpreset->bank,
                   sfpreset->prenum);
  }
  preset->bank = sfpreset->bank;
  preset->num = sfpreset->prenum;
  p = sfpreset->zone;
  count = 0;
  while (p != NULL) {
    sfzone = (SFZone *) p->data;
    FLUID_SPRINTF (zone_name, "%s/%d", preset->name, count);
    zone = new_fluid_preset_zone (zone_name);
    if (zone == NULL) {
      return FLUID_FAILED;
    }
    if (fluid_preset_zone_import_sfont (zone, sfzone, sfont) != FLUID_OK) {
      return FLUID_FAILED;
    }
    if ((count == 0) && (fluid_preset_zone_get_inst (zone) == NULL)) {
      fluid_defpreset_set_global_zone (preset, zone);
    } else if (fluid_defpreset_add_zone (preset, zone) != FLUID_OK) {
      return FLUID_FAILED;
    }
    p = fluid_list_next (p);
    count++;
  }
  return FLUID_OK;
}

/*
 *fluid_defpreset_add_zone
 */
int
fluid_defpreset_add_zone (fluid_defpreset_t * preset,
                          fluid_preset_zone_t * zone) {
  if (preset->zone == NULL) {
    zone->next = NULL;
    preset->zone = zone;
  } else {
    zone->next = preset->zone;
    preset->zone = zone;
  }
  return FLUID_OK;
}

/*
 *fluid_defpreset_get_zone
 */
fluid_preset_zone_t *fluid_defpreset_get_zone (fluid_defpreset_t * preset) {
  return preset->zone;
}

/*
 *fluid_defpreset_get_global_zone
 */
fluid_preset_zone_t *fluid_defpreset_get_global_zone (fluid_defpreset_t *
                                                      preset) {
  return preset->global_zone;
}

/*
 *fluid_preset_zone_next
 */
fluid_preset_zone_t *fluid_preset_zone_next (fluid_preset_zone_t * preset) {
  return preset->next;
}

/*
 *new_fluid_preset_zone
 */
fluid_preset_zone_t *new_fluid_preset_zone (char *name) {
  int size;
  fluid_preset_zone_t *zone = NULL;
  zone = FLUID_NEW (fluid_preset_zone_t);
  if (zone == NULL) {
    FLUID_LOG (FLUID_ERR, "Out of memory");
    return NULL;
  }
  zone->next = NULL;
  size = 1 + FLUID_STRLEN (name);
  zone->name = FLUID_MALLOC (size);
  if (zone->name == NULL) {
    FLUID_LOG (FLUID_ERR, "Out of memory");
    FLUID_FREE (zone);
    return NULL;
  }
  FLUID_STRCPY (zone->name, name);
  zone->inst = NULL;
  zone->keylo = 0;
  zone->keyhi = 128;
  zone->vello = 0;
  zone->velhi = 128;

  /* Flag all generators as unused (default, they will be set when they are found
   *in the sound font).
   *This also sets the generator values to default, but that is of no concern here.*/
  fluid_gen_set_default_values (&zone->gen[0]);
  zone->mod = NULL;             /* list of modulators */
  return zone;
}

/***************************************************************
 *
 *                          PRESET_ZONE
 */

/*
 *delete_fluid_preset_zone
 */
int delete_fluid_preset_zone (fluid_preset_zone_t * zone) {
  fluid_mod_t *mod, *tmp;

  mod = zone->mod;
  while (mod) {                 /* delete the modulators */
    tmp = mod;
    mod = mod->next;
    fluid_mod_delete (tmp);
  }

  if (zone->name)
    FLUID_FREE (zone->name);
  if (zone->inst)
    delete_fluid_inst (zone->inst);
  FLUID_FREE (zone);
  return FLUID_OK;
}

/*
 *fluid_preset_zone_import_sfont
 */
int
fluid_preset_zone_import_sfont (fluid_preset_zone_t * zone, SFZone * sfzone,
                                fluid_defsfont_t * sfont) {
  fluid_list_t *r;
  SFGen *sfgen;
  int count;
  for (count = 0, r = sfzone->gen; r != NULL; count++) {
    sfgen = (SFGen *) r->data;
    switch (sfgen->id) {
    case GEN_KEYRANGE:
      zone->keylo = (int) sfgen->amount.range.lo;
      zone->keyhi = (int) sfgen->amount.range.hi;
      break;
    case GEN_VELRANGE:
      zone->vello = (int) sfgen->amount.range.lo;
      zone->velhi = (int) sfgen->amount.range.hi;
      break;
    default:
      /* FIXME: some generators have an unsigne word amount value but i don't know which ones */
      zone->gen[sfgen->id].val = (fluid_real_t) sfgen->amount.sword;
      zone->gen[sfgen->id].flags = GEN_SET;
      break;
    }
    r = fluid_list_next (r);
  }
  if ((sfzone->instsamp != NULL) && (sfzone->instsamp->data != NULL)) {
    zone->inst = (fluid_inst_t *) new_fluid_inst ();
    if (zone->inst == NULL) {
      FLUID_LOG (FLUID_ERR, "Out of memory");
      return FLUID_FAILED;
    }
    if (fluid_inst_import_sfont
        (zone->inst, (SFInst *) sfzone->instsamp->data, sfont) != FLUID_OK) {
      return FLUID_FAILED;
    }
  }

  /* Import the modulators (only SF2.1 and higher) */
  for (count = 0, r = sfzone->mod; r != NULL; count++) {

    SFMod *mod_src = (SFMod *) r->data;
    fluid_mod_t *mod_dest = fluid_mod_new ();
    int type;

    if (mod_dest == NULL) {
      return FLUID_FAILED;
    }
    mod_dest->next = NULL;      /* pointer to next modulator, this is the end of the list now. */

    /* ***Amount *** */
    mod_dest->amount = mod_src->amount;

    /* ***Source *** */
    mod_dest->src1 = mod_src->src & 127;  /* index of source 1, seven-bit value, SF2.01 section 8.2, page 50 */
    mod_dest->flags1 = 0;

    /* Bit 7: CC flag SF 2.01 section 8.2.1 page 50 */
    if (mod_src->src & (1 << 7)) {
      mod_dest->flags1 |= FLUID_MOD_CC;
    } else {
      mod_dest->flags1 |= FLUID_MOD_GC;
    }

    /* Bit 8: D flag SF 2.01 section 8.2.2 page 51 */
    if (mod_src->src & (1 << 8)) {
      mod_dest->flags1 |= FLUID_MOD_NEGATIVE;
    } else {
      mod_dest->flags1 |= FLUID_MOD_POSITIVE;
    }

    /* Bit 9: P flag SF 2.01 section 8.2.3 page 51 */
    if (mod_src->src & (1 << 9)) {
      mod_dest->flags1 |= FLUID_MOD_BIPOLAR;
    } else {
      mod_dest->flags1 |= FLUID_MOD_UNIPOLAR;
    }

    /* modulator source types: SF2.01 section 8.2.1 page 52 */
    type = (mod_src->src) >> 10;
    type &= 63;                 /* type is a 6-bit value */
    if (type == 0) {
      mod_dest->flags1 |= FLUID_MOD_LINEAR;
    } else if (type == 1) {
      mod_dest->flags1 |= FLUID_MOD_CONCAVE;
    } else if (type == 2) {
      mod_dest->flags1 |= FLUID_MOD_CONVEX;
    } else if (type == 3) {
      mod_dest->flags1 |= FLUID_MOD_SWITCH;
    } else {
      /* This shouldn't happen - unknown type!
       *Deactivate the modulator by setting the amount to 0. */
      mod_dest->amount = 0;
    }

    /* ***Dest *** */
    mod_dest->dest = mod_src->dest; /* index of controlled generator */

    /* ***Amount source *** */
    mod_dest->src2 = mod_src->amtsrc & 127; /* index of source 2, seven-bit value, SF2.01 section 8.2, p.50 */
    mod_dest->flags2 = 0;

    /* Bit 7: CC flag SF 2.01 section 8.2.1 page 50 */
    if (mod_src->amtsrc & (1 << 7)) {
      mod_dest->flags2 |= FLUID_MOD_CC;
    } else {
      mod_dest->flags2 |= FLUID_MOD_GC;
    }

    /* Bit 8: D flag SF 2.01 section 8.2.2 page 51 */
    if (mod_src->amtsrc & (1 << 8)) {
      mod_dest->flags2 |= FLUID_MOD_NEGATIVE;
    } else {
      mod_dest->flags2 |= FLUID_MOD_POSITIVE;
    }

    /* Bit 9: P flag SF 2.01 section 8.2.3 page 51 */
    if (mod_src->amtsrc & (1 << 9)) {
      mod_dest->flags2 |= FLUID_MOD_BIPOLAR;
    } else {
      mod_dest->flags2 |= FLUID_MOD_UNIPOLAR;
    }

    /* modulator source types: SF2.01 section 8.2.1 page 52 */
    type = (mod_src->amtsrc) >> 10;
    type &= 63;                 /* type is a 6-bit value */
    if (type == 0) {
      mod_dest->flags2 |= FLUID_MOD_LINEAR;
    } else if (type == 1) {
      mod_dest->flags2 |= FLUID_MOD_CONCAVE;
    } else if (type == 2) {
      mod_dest->flags2 |= FLUID_MOD_CONVEX;
    } else if (type == 3) {
      mod_dest->flags2 |= FLUID_MOD_SWITCH;
    } else {
      /* This shouldn't happen - unknown type!
       *Deactivate the modulator by setting the amount to 0. */
      mod_dest->amount = 0;
    }

    /* ***Transform *** */
    /* SF2.01 only uses the 'linear' transform (0).
     *Deactivate the modulator by setting the amount to 0 in any other case.
     */
    if (mod_src->trans != 0) {
      mod_dest->amount = 0;
    }

    /* Store the new modulator in the zone The order of modulators
     *will make a difference, at least in an instrument context: The
     *second modulator overwrites the first one, if they only differ
     *in amount. */
    if (count == 0) {
      zone->mod = mod_dest;
    } else {
      fluid_mod_t *last_mod = zone->mod;

      /* Find the end of the list */
      while (last_mod->next != NULL) {
        last_mod = last_mod->next;
      }

      last_mod->next = mod_dest;
    }

    r = fluid_list_next (r);
  }                             /* foreach modulator */

  return FLUID_OK;
}

/*
 *fluid_preset_zone_get_inst
 */
fluid_inst_t *fluid_preset_zone_get_inst (fluid_preset_zone_t * zone) {
  return zone->inst;
}

/*
 *fluid_preset_zone_inside_range
 */
int
fluid_preset_zone_inside_range (fluid_preset_zone_t * zone, int key,
                                int vel) {
  return ((zone->keylo <= key) && (zone->keyhi >= key) && (zone->vello <= vel)
          && (zone->velhi >= vel));
}

/***************************************************************
 *
 *                          INST
 */

/*
 *new_fluid_inst
 */
fluid_inst_t *new_fluid_inst () {
  fluid_inst_t *inst = FLUID_NEW (fluid_inst_t);
  if (inst == NULL) {
    FLUID_LOG (FLUID_ERR, "Out of memory");
    return NULL;
  }
  inst->name[0] = 0;
  inst->global_zone = NULL;
  inst->zone = NULL;
  return inst;
}

/*
 *delete_fluid_inst
 */
int delete_fluid_inst (fluid_inst_t * inst) {
  fluid_inst_zone_t *zone;
  int err = FLUID_OK;
  if (inst->global_zone != NULL) {
    if (delete_fluid_inst_zone (inst->global_zone) != FLUID_OK) {
      err = FLUID_FAILED;
    }
    inst->global_zone = NULL;
  }
  zone = inst->zone;
  while (zone != NULL) {
    inst->zone = zone->next;
    if (delete_fluid_inst_zone (zone) != FLUID_OK) {
      err = FLUID_FAILED;
    }
    zone = inst->zone;
  }
  FLUID_FREE (inst);
  return err;
}

/*
 *fluid_inst_set_global_zone
 */
int fluid_inst_set_global_zone (fluid_inst_t * inst, fluid_inst_zone_t * zone) {
  inst->global_zone = zone;
  return FLUID_OK;
}

/*
 *fluid_inst_import_sfont
 */
int
fluid_inst_import_sfont (fluid_inst_t * inst, SFInst * sfinst,
                         fluid_defsfont_t * sfont) {
  fluid_list_t *p;
  SFZone *sfzone;
  fluid_inst_zone_t *zone;
  char zone_name[256];
  int count;

  p = sfinst->zone;
  if (FLUID_STRLEN (sfinst->name) > 0) {
    FLUID_STRCPY (inst->name, sfinst->name);
  } else {
    FLUID_STRCPY (inst->name, "<untitled>");
  }

  count = 0;
  while (p != NULL) {

    sfzone = (SFZone *) p->data;
    FLUID_SPRINTF (zone_name, "%s/%d", inst->name, count);

    zone = new_fluid_inst_zone (zone_name);
    if (zone == NULL) {
      return FLUID_FAILED;
    }

    if (fluid_inst_zone_import_sfont (zone, sfzone, sfont) != FLUID_OK) {
      return FLUID_FAILED;
    }

    if ((count == 0) && (fluid_inst_zone_get_sample (zone) == NULL)) {
      fluid_inst_set_global_zone (inst, zone);

    } else if (fluid_inst_add_zone (inst, zone) != FLUID_OK) {
      return FLUID_FAILED;
    }

    p = fluid_list_next (p);
    count++;
  }
  return FLUID_OK;
}

/*
 *fluid_inst_add_zone
 */
int fluid_inst_add_zone (fluid_inst_t * inst, fluid_inst_zone_t * zone) {
  if (inst->zone == NULL) {
    zone->next = NULL;
    inst->zone = zone;
  } else {
    zone->next = inst->zone;
    inst->zone = zone;
  }
  return FLUID_OK;
}

/*
 *fluid_inst_get_zone
 */
fluid_inst_zone_t *fluid_inst_get_zone (fluid_inst_t * inst) {
  return inst->zone;
}

/*
 *fluid_inst_get_global_zone
 */
fluid_inst_zone_t *fluid_inst_get_global_zone (fluid_inst_t * inst) {
  return inst->global_zone;
}

/***************************************************************
 *
 *                          INST_ZONE
 */

/*
 *new_fluid_inst_zone
 */
fluid_inst_zone_t *new_fluid_inst_zone (char *name) {
  int size;
  fluid_inst_zone_t *zone = NULL;
  zone = FLUID_NEW (fluid_inst_zone_t);
  if (zone == NULL) {
    FLUID_LOG (FLUID_ERR, "Out of memory");
    return NULL;
  }
  zone->next = NULL;
  size = 1 + FLUID_STRLEN (name);
  zone->name = FLUID_MALLOC (size);
  if (zone->name == NULL) {
    FLUID_LOG (FLUID_ERR, "Out of memory");
    FLUID_FREE (zone);
    return NULL;
  }
  FLUID_STRCPY (zone->name, name);
  zone->sample = NULL;
  zone->keylo = 0;
  zone->keyhi = 128;
  zone->vello = 0;
  zone->velhi = 128;

  /* Flag the generators as unused.
   *This also sets the generator values to default, but they will be overwritten anyway, if used.*/
  fluid_gen_set_default_values (&zone->gen[0]);
  zone->mod = NULL;             /* list of modulators */
  return zone;
}

/*
 *delete_fluid_inst_zone
 */
int delete_fluid_inst_zone (fluid_inst_zone_t * zone) {
  fluid_mod_t *mod, *tmp;

  mod = zone->mod;
  while (mod) {                 /* delete the modulators */
    tmp = mod;
    mod = mod->next;
    fluid_mod_delete (tmp);
  }

  if (zone->name)
    FLUID_FREE (zone->name);
  FLUID_FREE (zone);
  return FLUID_OK;
}

/*
 *fluid_inst_zone_next
 */
fluid_inst_zone_t *fluid_inst_zone_next (fluid_inst_zone_t * zone) {
  return zone->next;
}

/*
 *fluid_inst_zone_import_sfont
 */
int
fluid_inst_zone_import_sfont (fluid_inst_zone_t * zone, SFZone * sfzone,
                              fluid_defsfont_t * sfont) {
  fluid_list_t *r;
  SFGen *sfgen;
  int count;

  for (count = 0, r = sfzone->gen; r != NULL; count++) {
    sfgen = (SFGen *) r->data;
    switch (sfgen->id) {
    case GEN_KEYRANGE:
      zone->keylo = (int) sfgen->amount.range.lo;
      zone->keyhi = (int) sfgen->amount.range.hi;
      break;
    case GEN_VELRANGE:
      zone->vello = (int) sfgen->amount.range.lo;
      zone->velhi = (int) sfgen->amount.range.hi;
      break;
    default:
      /* FIXME: some generators have an unsigned word amount value but
         i don't know which ones */
      zone->gen[sfgen->id].val = (fluid_real_t) sfgen->amount.sword;
      zone->gen[sfgen->id].flags = GEN_SET;
      break;
    }
    r = fluid_list_next (r);
  }

  /* FIXME */
/*    if (zone->gen[GEN_EXCLUSIVECLASS].flags == GEN_SET) { */
/*      FLUID_LOG(FLUID_DBG, "ExclusiveClass=%d\n", (int) zone->gen[GEN_EXCLUSIVECLASS].val); */
/*    } */

  if ((sfzone->instsamp != NULL) && (sfzone->instsamp->data != NULL)) {
    zone->sample =
      fluid_defsfont_get_sample (sfont,
                                 ((SFSample *) sfzone->instsamp->data)->name);
    if (zone->sample == NULL) {
      FLUID_LOG (FLUID_ERR, "Couldn't find sample name");
      return FLUID_FAILED;
    }
  }

  /* Import the modulators (only SF2.1 and higher) */
  for (count = 0, r = sfzone->mod; r != NULL; count++) {
    SFMod *mod_src = (SFMod *) r->data;
    int type;
    fluid_mod_t *mod_dest;

    mod_dest = fluid_mod_new ();
    if (mod_dest == NULL) {
      return FLUID_FAILED;
    }

    mod_dest->next = NULL;      /* pointer to next modulator, this is the end of the list now. */

    /* ***Amount *** */
    mod_dest->amount = mod_src->amount;

    /* ***Source *** */
    mod_dest->src1 = mod_src->src & 127;  /* index of source 1, seven-bit value, SF2.01 section 8.2, page 50 */
    mod_dest->flags1 = 0;

    /* Bit 7: CC flag SF 2.01 section 8.2.1 page 50 */
    if (mod_src->src & (1 << 7)) {
      mod_dest->flags1 |= FLUID_MOD_CC;
    } else {
      mod_dest->flags1 |= FLUID_MOD_GC;
    }

    /* Bit 8: D flag SF 2.01 section 8.2.2 page 51 */
    if (mod_src->src & (1 << 8)) {
      mod_dest->flags1 |= FLUID_MOD_NEGATIVE;
    } else {
      mod_dest->flags1 |= FLUID_MOD_POSITIVE;
    }

    /* Bit 9: P flag SF 2.01 section 8.2.3 page 51 */
    if (mod_src->src & (1 << 9)) {
      mod_dest->flags1 |= FLUID_MOD_BIPOLAR;
    } else {
      mod_dest->flags1 |= FLUID_MOD_UNIPOLAR;
    }

    /* modulator source types: SF2.01 section 8.2.1 page 52 */
    type = (mod_src->src) >> 10;
    type &= 63;                 /* type is a 6-bit value */
    if (type == 0) {
      mod_dest->flags1 |= FLUID_MOD_LINEAR;
    } else if (type == 1) {
      mod_dest->flags1 |= FLUID_MOD_CONCAVE;
    } else if (type == 2) {
      mod_dest->flags1 |= FLUID_MOD_CONVEX;
    } else if (type == 3) {
      mod_dest->flags1 |= FLUID_MOD_SWITCH;
    } else {
      /* This shouldn't happen - unknown type!
       *Deactivate the modulator by setting the amount to 0. */
      mod_dest->amount = 0;
    }

    /* ***Dest *** */
    mod_dest->dest = mod_src->dest; /* index of controlled generator */

    /* ***Amount source *** */
    mod_dest->src2 = mod_src->amtsrc & 127; /* index of source 2, seven-bit value, SF2.01 section 8.2, page 50 */
    mod_dest->flags2 = 0;

    /* Bit 7: CC flag SF 2.01 section 8.2.1 page 50 */
    if (mod_src->amtsrc & (1 << 7)) {
      mod_dest->flags2 |= FLUID_MOD_CC;
    } else {
      mod_dest->flags2 |= FLUID_MOD_GC;
    }

    /* Bit 8: D flag SF 2.01 section 8.2.2 page 51 */
    if (mod_src->amtsrc & (1 << 8)) {
      mod_dest->flags2 |= FLUID_MOD_NEGATIVE;
    } else {
      mod_dest->flags2 |= FLUID_MOD_POSITIVE;
    }

    /* Bit 9: P flag SF 2.01 section 8.2.3 page 51 */
    if (mod_src->amtsrc & (1 << 9)) {
      mod_dest->flags2 |= FLUID_MOD_BIPOLAR;
    } else {
      mod_dest->flags2 |= FLUID_MOD_UNIPOLAR;
    }

    /* modulator source types: SF2.01 section 8.2.1 page 52 */
    type = (mod_src->amtsrc) >> 10;
    type &= 63;                 /* type is a 6-bit value */
    if (type == 0) {
      mod_dest->flags2 |= FLUID_MOD_LINEAR;
    } else if (type == 1) {
      mod_dest->flags2 |= FLUID_MOD_CONCAVE;
    } else if (type == 2) {
      mod_dest->flags2 |= FLUID_MOD_CONVEX;
    } else if (type == 3) {
      mod_dest->flags2 |= FLUID_MOD_SWITCH;
    } else {
      /* This shouldn't happen - unknown type!
       *Deactivate the modulator by setting the amount to 0. */
      mod_dest->amount = 0;
    }

    /* ***Transform *** */
    /* SF2.01 only uses the 'linear' transform (0).
     *Deactivate the modulator by setting the amount to 0 in any other case.
     */
    if (mod_src->trans != 0) {
      mod_dest->amount = 0;
    }

    /* Store the new modulator in the zone
     *The order of modulators will make a difference, at least in an instrument context:
     *The second modulator overwrites the first one, if they only differ in amount. */
    if (count == 0) {
      zone->mod = mod_dest;
    } else {
      fluid_mod_t *last_mod = zone->mod;
      /* Find the end of the list */
      while (last_mod->next != NULL) {
        last_mod = last_mod->next;
      }
      last_mod->next = mod_dest;
    }

    r = fluid_list_next (r);
  }                             /* foreach modulator */
  return FLUID_OK;
}

/*
 *fluid_inst_zone_get_sample
 */
fluid_sample_t *fluid_inst_zone_get_sample (fluid_inst_zone_t * zone) {
  return zone->sample;
}

/*
 *fluid_inst_zone_inside_range
 */
int fluid_inst_zone_inside_range (fluid_inst_zone_t * zone, int key, int vel) {
  return ((zone->keylo <= key) &&
          (zone->keyhi >= key) &&
          (zone->vello <= vel) && (zone->velhi >= vel));
}

/***************************************************************
 *
 *                          SAMPLE
 */

/*
 *new_fluid_sample
 */
fluid_sample_t *new_fluid_sample () {
  fluid_sample_t *sample = NULL;

  sample = FLUID_NEW (fluid_sample_t);
  if (sample == NULL) {
    FLUID_LOG (FLUID_ERR, "Out of memory");
    return NULL;
  }

  memset (sample, 0, sizeof (fluid_sample_t));
  sample->valid = 1;

  return sample;
}

/*
 *delete_fluid_sample
 */
int delete_fluid_sample (fluid_sample_t * sample) {
#if SF3_SUPPORT
  if (sample->sampletype & FLUID_SAMPLETYPE_OGG_VORBIS_UNPACKED) {
    if (sample->data != NULL)
      FLUID_FREE (sample->data);
  }
#endif

  FLUID_FREE (sample);
  return FLUID_OK;
}

/*
 *fluid_sample_in_rom
 */
int fluid_sample_in_rom (fluid_sample_t * sample) {
  return (sample->sampletype & FLUID_SAMPLETYPE_ROM);
}

/*
 *fluid_sample_import_sfont
 */
int
fluid_sample_import_sfont (fluid_sample_t * sample, SFSample * sfsample,
                           fluid_defsfont_t * sfont) {
  FLUID_STRCPY (sample->name, sfsample->name);
  sample->data = sfont->sampledata;
  sample->start = sfsample->start;
  sample->end = sfsample->start + sfsample->end;
  sample->loopstart = sfsample->start + sfsample->loopstart;
  sample->loopend = sfsample->start + sfsample->loopend;
  sample->samplerate = sfsample->samplerate;
  sample->origpitch = sfsample->origpitch;
  sample->pitchadj = sfsample->pitchadj;
  sample->sampletype = sfsample->sampletype;

  if (sample->sampletype & FLUID_SAMPLETYPE_OGG_VORBIS) {
  }

  if (sample->sampletype & FLUID_SAMPLETYPE_ROM) {
    sample->valid = 0;
    FLUID_LOG (FLUID_WARN, "Ignoring sample %s: can't use ROM samples",
               sample->name);
  }
  if (sample->end - sample->start < 8) {
    sample->valid = 0;
    FLUID_LOG (FLUID_WARN, "Ignoring sample %s: too few sample data points",
               sample->name);
  } else {
/*      if (sample->loopstart < sample->start + 8) { */
/*        FLUID_LOG(FLUID_WARN, "Fixing sample %s: at least 8 data points required before loop start", sample->name);     */
/*        sample->loopstart = sample->start + 8; */
/*      } */
/*      if (sample->loopend > sample->end - 8) { */
/*        FLUID_LOG(FLUID_WARN, "Fixing sample %s: at least 8 data points required after loop end", sample->name);     */
/*        sample->loopend = sample->end - 8; */
/*      } */
  }
  return FLUID_OK;
}

/********************************************************************************/
/********************************************************************************/
/********************************************************************************/
/********************************************************************************/
/********************************************************************************/

/*=================================sfload.c========================
  Borrowed from Smurf SoundFont Editor by Josh Green
  =================================================================*/

/*
   functions for loading data from sfont files, with appropriate byte swapping
   on big endian machines. Sfont IDs are not swapped because the ID read is
   equivalent to the matching ID list in memory regardless of LE/BE machine
*/


static U32 sdtachunk_size;

/* sound font file load functions */

SFData *sfLoadMem (void *sfDataP, int dataLen, U8 **srcPP) {
  SFData *sfP = NULL;
  int err = FALSE;

  if (!(sfP = FLUID_NEW (SFData))) {
    FLUID_LOG (FLUID_ERR, "Out of memory");
    err = TRUE;
  }
  if (!err)
    memset(sfP, 0, sizeof (SFData));  /* zero sfdata */
  if (!err && !load_body(sfDataP, dataLen, srcPP, sfP))
    err = TRUE;                 /* load the sfont */
  if (err) {
    if (sfP)
      sfont_close (sfP);
    return NULL;
  }

  return sfP;
}

// My machine is little endian. I trust this code to handle everything correctly... 
// If it messes up, I may have to reverse all these bytes in my num.c program's output.
#define UNKN_ (0x00000000)
#define RIFF_ (0x46464952)
#define LIST_ (0x5453494c)
#define sfbk_ (0x6b626673)
#define INFO_ (0x4f464e49)
#define sdta_ (0x61746473)
#define pdta_ (0x61746470)
#define ifil_ (0x6c696669)
#define isng_ (0x676e7369)
#define INAM_ (0x4d414e49)
#define irom_ (0x6d6f7269)
#define iver_ (0x72657669)
#define ICRD_ (0x44524349)
#define IENG_ (0x474e4549)
#define IPRD_ (0x44525049)
#define ICOP_ (0x504f4349)
#define ICMT_ (0x544d4349)
#define ISFT_ (0x54465349)
#define snam_ (0x6d616e73)
#define smpl_ (0x6c706d73)
#define phdr_ (0x72646870)
#define pbag_ (0x67616270)
#define pmod_ (0x646f6d70)
#define pgen_ (0x6e656770)
#define inst_ (0x74736e69)
#define ibag_ (0x67616269)
#define imod_ (0x646f6d69)
#define igen_ (0x6e656769)
#define shdr_ (0x72646873)

static int load_body (void *sfDataP, int dataLen, U8 **srcPP, SFData * sfP) {
  SFChunk chunk;                // 8 bytes; 4 for id, 4 for size
  SFChunk *chunkP = &chunk;

  // Load RIFF chunk.
  readChunk_(&chunk, *srcPP);   // bytes 1 - 8
  if (chunk.id != RIFF_)
    return FAIL;

  // Sfbk (soundfont bank, I think?)
  readId_(&chunk.id, *srcPP);    // bytes 9 - 12 ("sfbk")
  if (chunk.id != sfbk_)
    return FAIL;
  if (chunk.size != dataLen - 8)
    return FAIL;

  /* Process INFO block */
  if (!read_listchunk(&chunkP, srcPP))  // bytes 13 - 24 (skipping ifil??)
    return FAIL;
  if (chunk.id != INFO_)           // at this point, ifil is the next ID.
    return FAIL;
  if (!process_info(chunk.size, sfP, srcPP))
    return FAIL;

  /* Process sample chunk */
  if (!read_listchunk(&chunkP, srcPP))
    return FAIL;
  if (chunk.id != sdta_)
    return FAIL;
  if (!process_sdta(chunk.size, sfP, sfDataP, srcPP))
    return FAIL;

  /* process HYDRA chunk */
  if (!read_listchunk(&chunkP, srcPP))
    return FAIL;
  if (chunk.id != pdta_)
    return FAIL;
  if (!process_pdta(chunk.size, sfP, srcPP))
    return FAIL;

  if (!fixup_pgen(sfP))
    return FAIL;
  if (!fixup_igen(sfP))
    return FAIL;
  if (!fixup_sample(sfP))
    return FAIL;

  /* sort preset list by bank, preset # */
  sfP->preset = fluid_list_sort(sfP->preset, (fluid_compare_func_t) sfont_preset_compare_func);

  return (OK);
}

static int read_listchunk(SFChunk **chunkPP, U8 **srcPP) {
  SFChunk *chunkP = *chunkPP;
  readChunk_(chunkP, *srcPP);        /* read list chunk */
  if (chunkP->id != LIST_)  /* error if ! list chunk */
    return FAIL;
  readId_(&chunkP->id, *srcPP);  /* read id string */
  chunkP->size -= 4;
  return OK;
}

static int process_info (int size, SFData *sfP, U8 **srcPP) {
  SFChunk chunk;
  U32 id;
  char *item;
  unsigned short ver;

  while (size > 0) {
    readChunk_(&chunk, *srcPP);  // read 8 bytes 
    size -= 8;

    id = chunk.id;

    if (id == ifil_) {
      if (chunk.size != 4)
        return FAIL;
      skip_(*srcPP, 4);
    }
    else if (id == iver_) {
      if (chunk.size != 4)
        return FAIL;
      skip_(*srcPP, 4);
    }
    else if (id != UNKN_) {
      if ((id != ICMT_ && chunk.size > 256) || (chunk.size > 65536) || (chunk.size & 1))
        return FAIL;
      /* alloc for chunk id and da chunk */
      if (!(item = FLUID_MALLOC(chunk.size + 1))) 
        return E_NO_MEMORY;
      /* attach to INFO list, sfont_close will cleanup if FAIL occurs */
      sfP->info = fluid_list_append (sfP->info, item);

      *(U8 *) item = id;
      readNBytes_(*srcPP, &item[1], chunk.size);
      *(item + chunk.size) = '\0';
    } else
      return FAIL;
    size -= chunk.size;
  }

  if (size < 0) return FAIL;

  return OK;
}

static int process_sdta (int size, SFData * sf, void *sfDataP, U8 **srcPP) {
  SFChunk chunk;

  if (size == 0) return (OK);  /* no sample data? */

  /* read sub chunk */
  readChunk_ (&chunk, *srcPP);
  size -= 8;

  if (chunk.id != smpl_)
    return FAIL;

  if (size != chunk.size) return FAIL;

  /* sample data follows */
  sf->samplepos = *srcPP - (U8*) sfDataP;

  /* used in fixup_sample() to check validity of sample headers */
  sdtachunk_size = chunk.size;
  sf->samplesize = chunk.size;

  skip_(*srcPP, chunk.size);

  return OK;
}

static int pdtahelper (U8 **srcPP, U32 expid, U32 reclen, SFChunk * chunkP, int *size) {
  U32 id;

  readChunk_ (chunkP, *srcPP);
  *size -= 8;

  if ((id = chunkP->id) != expid)
    return FAIL;
  if (chunkP->size % reclen)      /* valid chunk size? */
    return FAIL;
  if ((*size -= chunkP->size) < 0)
    return FAIL;

  return (OK);
}

static int process_pdta (int size, SFData * sf, U8 **srcPP) {
  SFChunk chunk;

  if (!pdtahelper (srcPP, phdr_, SFPHDRSIZE, &chunk, &size))
    return FAIL;
  if (!load_phdr (srcPP, chunk.size, sf))
    return FAIL;

  if (!pdtahelper (srcPP, pbag_, SFBAGSIZE, &chunk, &size))
    return FAIL;
  if (!load_pbag (srcPP, chunk.size, sf))
    return FAIL;

  if (!pdtahelper (srcPP, pmod_, SFMODSIZE, &chunk, &size))
    return FAIL;
  if (!load_pmod (srcPP, chunk.size, sf))
    return FAIL;

  if (!pdtahelper (srcPP, pgen_, SFGENSIZE, &chunk, &size))
    return FAIL;
  if (!load_pgen (srcPP, chunk.size, sf))
    return FAIL;

  if (!pdtahelper (srcPP, inst_, SFIHDRSIZE, &chunk, &size))
    return FAIL;
  if (!load_ihdr (srcPP, chunk.size, sf))
    return FAIL;

  if (!pdtahelper (srcPP, ibag_, SFBAGSIZE, &chunk, &size))
    return FAIL;
  if (!load_ibag (srcPP, chunk.size, sf))
    return FAIL;

  if (!pdtahelper (srcPP, imod_, SFMODSIZE, &chunk, &size))
    return FAIL;
  if (!load_imod (srcPP, chunk.size, sf))
    return FAIL;

  if (!pdtahelper (srcPP, igen_, SFGENSIZE, &chunk, &size))
    return FAIL;
  if (!load_igen (srcPP, chunk.size, sf))
    return FAIL;

  if (!pdtahelper (srcPP, shdr_, SFSHDRSIZE, &chunk, &size))
    return FAIL;
  if (!load_shdr (srcPP, chunk.size, sf))
    return FAIL;

  return (OK);
}

/* preset header loader */
static int load_phdr (U8 **srcPP, int size, SFData * sf) {
  int i, i2;
  SFPreset *p, *pr = NULL;      /* ptr to current & previous preset */
  unsigned short zndx, pzndx = 0;

  if (size % SFPHDRSIZE || size == 0)
    return FAIL;

  i = size / SFPHDRSIZE - 1;
  if (i == 0) {                 /* at least one preset + term record */
    skip_(*srcPP, SFPHDRSIZE);
    return (OK);
  }

  for (; i > 0; i--) {          /* load all preset headers */
    p = FLUID_NEW (SFPreset);
    sf->preset = fluid_list_append (sf->preset, p);
    p->zone = NULL;             /* In case of failure, sfont_close can cleanup */
    readStr_(p->name, *srcPP); /* possible read failure ^ */
    readU16_(*srcPP, p->prenum);
    readU16_(*srcPP, p->bank);
    readU16_(*srcPP, zndx);
    readU32_(p->libr, *srcPP);
    readU32_(p->genre, *srcPP);
    readU32_(p->morph, *srcPP);

    if (pr) {                   /* not first preset? */
      if (zndx < pzndx) return FAIL;
      i2 = zndx - pzndx;
      while (i2--) 
        pr->zone = fluid_list_prepend (pr->zone, NULL);
    } 
    //else if (zndx > 0)        /* 1st preset, warn if ofs >0 */
    //  FLUID_LOG (FLUID_WARN, _("%d preset zones not referenced, discarding"),
    //             zndx);
    pr = p;                     /* update preset ptr */
    pzndx = zndx;
  }

  skip_ (*srcPP, 24);
  readU16_ (*srcPP, zndx);       /* Read terminal generator index */
  skip_ (*srcPP, 12);

  if (zndx < pzndx)
    return FAIL;

  i2 = zndx - pzndx;

  while (i2--) 
    pr->zone = fluid_list_prepend(pr->zone, NULL);

  return OK;
}

/* preset bag loader */
static int load_pbag (U8 **srcPP, int size, SFData * sf) {
  fluid_list_t *p, *p2;
  SFZone *z, *pz = NULL;
  unsigned short genndx, modndx;
  unsigned short pgenndx = 0, pmodndx = 0;
  unsigned short i;

  if (size % SFBAGSIZE || size == 0)  /* size is multiple of SFBAGSIZE? */
    return FAIL;

  p = sf->preset;
  while (p) {                   /* traverse through presets */
    p2 = ((SFPreset *) (p->data))->zone;
    while (p2) {                /* traverse preset's zones */
      if ((size -= SFBAGSIZE) < 0)
        return FAIL;
      z = FLUID_NEW (SFZone);
      p2->data = z;
      z->gen = NULL;            /* Init gen and mod before possible failure, */
      z->mod = NULL;            /* to ensure proper cleanup (sfont_close) */
      readU16_(*srcPP, genndx);  /* possible read failure ^ */
      readU16_(*srcPP, modndx);
      z->instsamp = NULL;

      if (pz) {                 /* if not first zone */
        if (genndx < pgenndx)
          return FAIL;
        if (modndx < pmodndx)
          return FAIL;
        i = genndx - pgenndx;
        while (i--)
          pz->gen = fluid_list_prepend (pz->gen, NULL);
        i = modndx - pmodndx;
        while (i--)
          pz->mod = fluid_list_prepend (pz->mod, NULL);
      }
      pz = z;                   /* update previous zone ptr */
      pgenndx = genndx;         /* update previous zone gen index */
      pmodndx = modndx;         /* update previous zone mod index */
      p2 = fluid_list_next (p2);
    }
    p = fluid_list_next (p);
  }

  size -= SFBAGSIZE;
  if (size != 0)
    return FAIL;

  readU16_ (*srcPP, genndx);
  readU16_ (*srcPP, modndx);

  if (!pz) {
    if (genndx > 0)
      return FAIL;
    if (modndx > 0)
      return FAIL;
    return (OK);
  }

  if (genndx < pgenndx)
    return FAIL;
  if (modndx < pmodndx)
    return FAIL;
  i = genndx - pgenndx;
  while (i--)
    pz->gen = fluid_list_prepend (pz->gen, NULL);
  i = modndx - pmodndx;
  while (i--)
    pz->mod = fluid_list_prepend (pz->mod, NULL);

  return OK;
}

/* preset modulator loader */
static int load_pmod (U8 **srcPP, int size, SFData * sf) {
  fluid_list_t *p, *p2, *p3;
  SFMod *m;

  p = sf->preset;
  while (p) {                   /* traverse through all presets */
    p2 = ((SFPreset *) (p->data))->zone;
    while (p2) {                /* traverse this preset's zones */
      p3 = ((SFZone *) (p2->data))->mod;
      while (p3) {              /* load zone's modulators */
        if ((size -= SFMODSIZE) < 0)
          return FAIL;
        m = FLUID_NEW (SFMod);
        p3->data = m;
        readU16_ (*srcPP, m->src);
        readU16_ (*srcPP, m->dest);
        readU16_ (*srcPP, m->amount);
        readU16_ (*srcPP, m->amtsrc);
        readU16_ (*srcPP, m->trans);
        p3 = fluid_list_next (p3);
      }
      p2 = fluid_list_next (p2);
    }
    p = fluid_list_next (p);
  }

  /*
     If there isn't even a terminal record
     Hmmm, the specs say there should be one, but..
   */
  if (size == 0) return (OK);

  size -= SFMODSIZE;
  if (size != 0) return FAIL;
  skip_ (*srcPP, SFMODSIZE); /* terminal mod */

  return (OK);
}

/* -------------------------------------------------------------------
 *preset generator loader
 *generator (per preset) loading rules:
 *Zones with no generators or modulators shall be annihilated
 *Global zone must be 1st zone, discard additional ones (instrumentless zones)
 *
 * MB: a generator is just an effect on an otherwise plain note.
 *generator (per zone) loading rules (in order of decreasing precedence):
 *KeyRange is 1st in list (if exists), else discard
 *if a VelRange exists only preceded by a KeyRange, else discard
 *if a generator follows an instrument discard it
 *if a duplicate generator exists replace previous one
 *------------------------------------------------------------------- */
static int load_pgen (U8 **srcPP, int size, SFData * sf) {
  fluid_list_t *p, *p2, *p3, *dup, **hz = NULL;
  SFZone *z;
  SFGen *g;
  SFGenAmount genval;
  unsigned short genid;
  int level, skip, drop, gzone, discarded;

  p = sf->preset;
  while (p) {                   /* traverse through all presets */
    gzone = FALSE;
    discarded = FALSE;
    p2 = ((SFPreset *) (p->data))->zone;
    if (p2)
      hz = &p2;
    while (p2) {                /* traverse preset's zones */
      level = 0;
      z = (SFZone *) (p2->data);
      p3 = z->gen;
      while (p3) {              /* load zone's generators */
        dup = NULL;
        skip = FALSE;
        drop = FALSE;
        if ((size -= SFGENSIZE) < 0)
          return FAIL;

        readU16_ (*srcPP, genid);

        if (genid == Gen_KeyRange) {  /* nothing precedes */
          if (level == 0) {
            level = 1;
            readU8_(&genval.range.lo, *srcPP);
            readU8_(&genval.range.hi, *srcPP);
          } else
            skip = TRUE;
        } else if (genid == Gen_VelRange) { /* only KeyRange precedes */
          if (level <= 1) {
            level = 2;
            readU8_(&genval.range.lo, *srcPP);
            readU8_(&genval.range.hi, *srcPP);
          } else
            skip = TRUE;
        } else if (genid == Gen_Instrument) { /* inst is last gen */
          level = 3;
          readU16_(*srcPP, genval.uword);
          ((SFZone *) (p2->data))->instsamp = GINT_TO_POINTER (genval.uword + 1);
          break;                /* break out of generator loop */
        } else {
          level = 2;
          if (gen_validp (genid)) { /* generator valid? */
            readU16_ (*srcPP, genval.sword);
            dup = gen_inlist (genid, z->gen);
          } else
            skip = TRUE;
        }

        if (!skip) {
          if (!dup) {           /* if gen ! dup alloc new */
            g = FLUID_NEW (SFGen);
            p3->data = g;
            g->id = genid;
          } else {
            g = (SFGen *) (dup->data);  /* ptr to orig gen */
            drop = TRUE;
          }
          g->amount = genval;
        } else {                /* Skip this generator */
          discarded = TRUE;
          drop = TRUE;
          skipU16_(*srcPP);
        }

        if (!drop)
          p3 = fluid_list_next (p3);  /* next gen */
        else
          SLADVREM (z->gen, p3);  /* drop place holder */

      }                         /* generator loop */

      if (level == 3)
        SLADVREM (z->gen, p3);  /* zone has inst? */
      else {                    /* congratulations its a global zone */
        if (!gzone) {           /* Prior global zones? */
          gzone = TRUE;

          /* if global zone is not 1st zone, relocate */
          if (*hz != p2) {
            void *save = p2->data;
            FLUID_LOG (FLUID_WARN,
                       _("Preset \"%s\": Global zone is not first zone"),
                       ((SFPreset *) (p->data))->name);
            SLADVREM (*hz, p2);
            *hz = fluid_list_prepend (*hz, save);
            continue;
          }
        } else {                /* previous global zone exists, discard */
          FLUID_LOG (FLUID_WARN,
                     _("Preset \"%s\": Discarding invalid global zone"),
                     ((SFPreset *) (p->data))->name);
          sfont_zone_delete (sf, hz, (SFZone *) (p2->data));
        }
      }

      while (p3) {              /* Kill any zones following an instrument */
        discarded = TRUE;
        if ((size -= SFGENSIZE) < 0)
          return FAIL;
        skip_ (*srcPP, SFGENSIZE);
        SLADVREM (z->gen, p3);
      }

      p2 = fluid_list_next (p2);  /* next zone */
    }
    if (discarded)
      FLUID_LOG (FLUID_WARN,
                 _("Preset \"%s\": Some invalid generators were discarded"),
                 ((SFPreset *) (p->data))->name);
    p = fluid_list_next (p);
  }

  /* in case there isn't a terminal record */
  if (size == 0)
    return (OK);

  size -= SFGENSIZE;
  if (size != 0)
    return FAIL;
  skip_ (*srcPP, SFGENSIZE); /* terminal gen */

  return (OK);
}

/* instrument header loader */
static int load_ihdr (U8 **srcPP, int size, SFData *sf) {
  int i, i2;
  SFInst *p, *pr = NULL;        /* ptr to current & previous instrument */
  unsigned short zndx, pzndx = 0;

  if (size % SFIHDRSIZE || size == 0) /* chunk size is valid? */
    return FAIL;

  size = size / SFIHDRSIZE - 1;
  if (size == 0) {              /* at least one preset + term record */
    skip_ (*srcPP, SFIHDRSIZE);
    return (OK);
  }

  for (i = 0; i < size; i++) {  /* load all instrument headers */
    p = FLUID_NEW (SFInst);
    sf->inst = fluid_list_append (sf->inst, p);
    p->zone = NULL;             /* For proper cleanup if fail (sfont_close) */
    readStr_ (p->name, *srcPP);  /* Possible read failure ^ */
    readU16_ (*srcPP, zndx);

    if (pr) {                   /* not first instrument? */
      if (zndx < pzndx)
        return FAIL;
      i2 = zndx - pzndx;
      while (i2--)
        pr->zone = fluid_list_prepend (pr->zone, NULL);
    } else if (zndx > 0)        /* 1st inst, warn if ofs >0 */
      FLUID_LOG (FLUID_WARN,
                 _("%d instrument zones not referenced, discarding"), zndx);
    pzndx = zndx;
    pr = p;                     /* update instrument ptr */
  }

  skip_(*srcPP, 20);
  readU16_(*srcPP, zndx);

  if (zndx < pzndx)
    return FAIL;
  i2 = zndx - pzndx;
  while (i2--)
    pr->zone = fluid_list_prepend (pr->zone, NULL);

  return (OK);
}

/* instrument bag loader */
static int load_ibag (U8 **srcPP, int size, SFData *sf) {
  fluid_list_t *p, *p2;
  SFZone *z, *pz = NULL;
  unsigned short genndx, modndx, pgenndx = 0, pmodndx = 0;
  int i;

  if (size % SFBAGSIZE || size == 0)  /* size is multiple of SFBAGSIZE? */
    return FAIL;

  p = sf->inst;
  while (p) {                   /* traverse through inst */
    p2 = ((SFInst *) (p->data))->zone;
    while (p2) {                /* load this inst's zones */
      if ((size -= SFBAGSIZE) < 0)
        return FAIL;
      z = FLUID_NEW (SFZone);
      p2->data = z;
      z->gen = NULL;            /* In case of failure, */
      z->mod = NULL;            /* sfont_close can clean up */
      readU16_ (*srcPP, genndx); /* readU16_ = possible read failure */
      readU16_ (*srcPP, modndx);
      z->instsamp = NULL;

      if (pz) {                 /* if not first zone */
        if (genndx < pgenndx)
          return FAIL;
                        //_("Instrument generator indices not monotonic")));
        if (modndx < pmodndx)
          return FAIL;
                        //_("Instrument modulator indices not monotonic")));
        i = genndx - pgenndx;
        while (i--)
          pz->gen = fluid_list_prepend (pz->gen, NULL);
        i = modndx - pmodndx;
        while (i--)
          pz->mod = fluid_list_prepend (pz->mod, NULL);
      }
      pz = z;                   /* update previous zone ptr */
      pgenndx = genndx;
      pmodndx = modndx;
      p2 = fluid_list_next (p2);
    }
    p = fluid_list_next (p);
  }

  size -= SFBAGSIZE;
  if (size != 0)
    return FAIL;

  readU16_ (*srcPP, genndx);
  readU16_ (*srcPP, modndx);

  if (!pz) {                    /* in case that all are no zoners */
    if (genndx > 0)
      FLUID_LOG (FLUID_WARN,
                 _("No instrument generators and terminal index not 0"));
    if (modndx > 0)
      FLUID_LOG (FLUID_WARN,
                 _("No instrument modulators and terminal index not 0"));
    return (OK);
  }

  if (genndx < pgenndx)
    return FAIL;
  if (modndx < pmodndx)
    return FAIL;
  i = genndx - pgenndx;
  while (i--)
    pz->gen = fluid_list_prepend (pz->gen, NULL);
  i = modndx - pmodndx;
  while (i--)
    pz->mod = fluid_list_prepend (pz->mod, NULL);

  return (OK);
}

/* instrument modulator loader */
static int load_imod (U8 **srcPP, int size, SFData *sf) {
  fluid_list_t *p, *p2, *p3;
  SFMod *m;

  p = sf->inst;
  while (p) {                   /* traverse through all inst */
    p2 = ((SFInst *) (p->data))->zone;
    while (p2) {                /* traverse this inst's zones */
      p3 = ((SFZone *) (p2->data))->mod;
      while (p3) {              /* load zone's modulators */
        if ((size -= SFMODSIZE) < 0)
          return FAIL;
        m = FLUID_NEW (SFMod);
        p3->data = m;
        readU16_ (*srcPP, m->src);
        readU16_ (*srcPP, m->dest);
        readU16_ (*srcPP, m->amount);
        readU16_ (*srcPP, m->amtsrc);
        readU16_ (*srcPP, m->trans);
        p3 = fluid_list_next (p3);
      }
      p2 = fluid_list_next (p2);
    }
    p = fluid_list_next (p);
  }

  /*
     If there isn't even a terminal record
     Hmmm, the specs say there should be one, but..
   */
  if (size == 0)
    return (OK);

  size -= SFMODSIZE;
  if (size != 0)
    return FAIL;
  skip_ (*srcPP, SFMODSIZE); /* terminal mod */

  return (OK);
}

/* load instrument generators (see load_pgen for loading rules) */
static int load_igen (U8 **srcPP, int size, SFData *sf) {
  fluid_list_t *p, *p2, *p3, *dup, **hz = NULL;
  SFZone *z;
  SFGen *g;
  SFGenAmount genval;
  unsigned short genid;
  int level, skip, drop, gzone, discarded;

  p = sf->inst;
  while (p) {                   /* traverse through all instruments */
    gzone = FALSE;
    discarded = FALSE;
    p2 = ((SFInst *) (p->data))->zone;
    if (p2)
      hz = &p2;
    while (p2) {                /* traverse this instrument's zones */
      level = 0;
      z = (SFZone *) (p2->data);
      p3 = z->gen;
      while (p3) {              /* load zone's generators */
        dup = NULL;
        skip = FALSE;
        drop = FALSE;
        if ((size -= SFGENSIZE) < 0)
          return FAIL;

        readU16_ (*srcPP, genid);

        if (genid == Gen_KeyRange) {  /* nothing precedes */
          if (level == 0) {
            level = 1;
            readU8_ (&genval.range.lo, *srcPP);
            readU8_ (&genval.range.hi, *srcPP);
          } else
            skip = TRUE;
        } else if (genid == Gen_VelRange) { /* only KeyRange precedes */
          if (level <= 1) {
            level = 2;
            readU8_ (&genval.range.lo, *srcPP);
            readU8_ (&genval.range.hi, *srcPP);
          } else
            skip = TRUE;
        } else if (genid == Gen_SampleId) { /* sample is last gen */
          level = 3;
          readU16_ (*srcPP, genval.uword);
          ((SFZone *) (p2->data))->instsamp =
            GINT_TO_POINTER (genval.uword + 1);
          break;                /* break out of generator loop */
        } else {
          level = 2;
          if (gen_valid (genid)) {  /* gen valid? */
            readU16_ (*srcPP, genval.sword);
            dup = gen_inlist (genid, z->gen);
          } else
            skip = TRUE;
        }

        if (!skip) {
          if (!dup) {           /* if gen ! dup alloc new */
            g = FLUID_NEW (SFGen);
            p3->data = g;
            g->id = genid;
          } else {
            g = (SFGen *) (dup->data);
            drop = TRUE;
          }
          g->amount = genval;
        } else {                /* skip this generator */
          discarded = TRUE;
          drop = TRUE;
          skipU16_ (*srcPP);
        }

        if (!drop)
          p3 = fluid_list_next (p3);  /* next gen */
        else
          SLADVREM (z->gen, p3);

      }                         /* generator loop */

      if (level == 3)
        SLADVREM (z->gen, p3);  /* zone has sample? */
      else {                    /* its a global zone */
        if (!gzone) {
          gzone = TRUE;

          /* if global zone is not 1st zone, relocate */
          if (*hz != p2) {
            void *save = p2->data;
            FLUID_LOG (FLUID_WARN,
                       _("Instrument \"%s\": Global zone is not first zone"),
                       ((SFPreset *) (p->data))->name);
            SLADVREM (*hz, p2);
            *hz = fluid_list_prepend (*hz, save);
            continue;
          }
        } else {                /* previous global zone exists, discard */
          FLUID_LOG (FLUID_WARN,
                     _("Instrument \"%s\": Discarding invalid global zone"),
                     ((SFInst *) (p->data))->name);
          sfont_zone_delete (sf, hz, (SFZone *) (p2->data));
        }
      }

      while (p3) {              /* Kill any zones following a sample */
        discarded = TRUE;
        if ((size -= SFGENSIZE) < 0)
          return FAIL;
        skip_ (*srcPP, SFGENSIZE);
        SLADVREM (z->gen, p3);
      }

      p2 = fluid_list_next (p2);  /* next zone */
    }
    if (discarded)
      FLUID_LOG (FLUID_WARN,
                 _
                 ("Instrument \"%s\": Some invalid generators were discarded"),
                 ((SFInst *) (p->data))->name);
    p = fluid_list_next (p);
  }

  /* for those non-terminal record cases, grr! */
  if (size == 0)
    return (OK);

  size -= SFGENSIZE;
  if (size != 0)
    return FAIL;
  skip_ (*srcPP, SFGENSIZE); /* terminal gen */

  return (OK);
}

/* sample header loader */
static int load_shdr (U8 **srcPP, U32 size, SFData *sf) {
  U32 i;
  SFSample *p;

  if (size % SFSHDRSIZE || size == 0) /* size is multiple of SHDR size? */
    return FAIL;

  size = size / SFSHDRSIZE - 1;
  if (size == 0) {              /* at least one sample + term record? */
    FLUID_LOG (FLUID_WARN, _("File contains no samples"));
    skip_ (*srcPP, SFSHDRSIZE);
    return (OK);
  }

  /* load all sample headers */
  for (i = 0; i < size; i++) {
    p = FLUID_NEW (SFSample);
    sf->sample = fluid_list_append (sf->sample, p);
    readStr_ (p->name, *srcPP);
    readU32_ (p->start, *srcPP);
    readU32_ (p->end, *srcPP);   /* - end, loopstart and loopend */
    readU32_ (p->loopstart, *srcPP); /* - will be checked and turned into */
    readU32_ (p->loopend, *srcPP); /* - offsets in fixup_sample() */
    readU32_ (p->samplerate, *srcPP);
    readU8_ (&p->origpitch, *srcPP);
    readU8_ (&p->pitchadj, *srcPP);
    skipU16_ (*srcPP);         /* skip sample link */
    readU16_ (*srcPP, p->sampletype);
    p->samfile = 0;
  }

  skip_ (*srcPP, SFSHDRSIZE);  /* skip terminal shdr */

  return (OK);
}

/* "fixup" (inst # -> inst ptr) instrument references in preset list */
static int fixup_pgen (SFData * sf) {
  fluid_list_t *p, *p2, *p3;
  SFZone *z;
  uintptr i;

  p = sf->preset;
  while (p) {
    p2 = ((SFPreset *) (p->data))->zone;
    while (p2) {                /* traverse this preset's zones */
      z = (SFZone *) (p2->data);
      if ((i = GPOINTER_TO_INT (z->instsamp))) {  /* load instrument # */
        p3 = fluid_list_nth (sf->inst, i - 1);
        if (!p3)
          return FAIL;
        z->instsamp = p3;
      } else
        z->instsamp = NULL;
      p2 = fluid_list_next (p2);
    }
    p = fluid_list_next (p);
  }

  return (OK);
}

/* "fixup" (sample # -> sample ptr) sample references in instrument list */
static int fixup_igen (SFData * sf) {
  fluid_list_t *p, *p2, *p3;
  SFZone *z;
  uintptr i;

  p = sf->inst;
  while (p) {
    p2 = ((SFInst *) (p->data))->zone;
    while (p2) {                /* traverse instrument's zones */
      z = (SFZone *) (p2->data);
      if ((i = GPOINTER_TO_INT (z->instsamp))) {  /* load sample # */
        p3 = fluid_list_nth (sf->sample, i - 1);
        if (!p3)
          return FAIL;
        z->instsamp = p3;
      }
      p2 = fluid_list_next (p2);
    }
    p = fluid_list_next (p);
  }

  return (OK);
}

/* convert sample end, loopstart and loopend to offsets and check if valid */
static int fixup_sample (SFData * sf) {
  fluid_list_t *p;
  SFSample *sam;

  p = sf->sample;
  while (p) {
    sam = (SFSample *) (p->data);

    /* if sample is not a ROM sample and end is over the sample data chunk
       or sam start is greater than 4 less than the end (at least 4 samples) */
    if ((!(sam->sampletype & FLUID_SAMPLETYPE_ROM)
         && sam->end > sdtachunk_size) || sam->start > (sam->end - 4)) {
      FLUID_LOG (FLUID_WARN,
                 _("Sample '%s' start/end file positions are invalid,"
                   " disabling and will not be saved"), sam->name);

      /* disable sample by setting all sample markers to 0 */
      sam->start = sam->end = sam->loopstart = sam->loopend = 0;

      return (OK);
    }
    /* compressed samples get fixed up after decompression */
    else if (sam->sampletype & FLUID_SAMPLETYPE_OGG_VORBIS) {
    } else if (sam->loopend > sam->end || sam->loopstart >= sam->loopend || sam->loopstart <= sam->start) { /* loop is fowled?? (cluck cluck :) */
      /* can pad loop by 8 samples and ensure at least 4 for loop (2*8+4) */
      if ((sam->end - sam->start) >= 20) {
        sam->loopstart = sam->start + 8;
        sam->loopend = sam->end - 8;
      } else {                  /* loop is fowled, sample is tiny (can't pad 8 samples) */
        sam->loopstart = sam->start + 1;
        sam->loopend = sam->end - 1;
      }
    }

    /* convert sample end, loopstart, loopend to offsets from sam->start */
    sam->end -= sam->start + 1; /* marks last sample, contrary to SF spec. */
    sam->loopstart -= sam->start;
    sam->loopend -= sam->start;

    p = fluid_list_next (p);
  }

  return (OK);
}

/*=================================sfont.c========================
  Smurf SoundFont Editor
  ================================================================*/

/* optimum chunk area sizes (could be more optimum) */
#define PRESET_CHUNK_OPTIMUM_AREA 256
#define INST_CHUNK_OPTIMUM_AREA   256
#define SAMPLE_CHUNK_OPTIMUM_AREA 256
#define ZONE_CHUNK_OPTIMUM_AREA   256
#define MOD_CHUNK_OPTIMUM_AREA    256
#define GEN_CHUNK_OPTIMUM_AREA    256

unsigned short badgen[] =
  { Gen_Unused1, Gen_Unused2, Gen_Unused3, Gen_Unused4,
  Gen_Reserved1, Gen_Reserved2, Gen_Reserved3, 0
};

unsigned short badpgen[] =
  { Gen_StartAddrOfs, Gen_EndAddrOfs, Gen_StartLoopAddrOfs,
  Gen_EndLoopAddrOfs, Gen_StartAddrCoarseOfs, Gen_EndAddrCoarseOfs,
  Gen_StartLoopAddrCoarseOfs, Gen_Keynum, Gen_Velocity,
  Gen_EndLoopAddrCoarseOfs, Gen_SampleModes, Gen_ExclusiveClass,
  Gen_OverrideRootKey, 0
};

/* close SoundFont file and delete a SoundFont structure */
void sfont_close (SFData * sf) {
  fluid_list_t *p, *p2;

  p = sf->info;
  while (p) {
    free (p->data);
    p = fluid_list_next (p);
  }
  delete_fluid_list (sf->info);
  sf->info = NULL;

  p = sf->preset;
  while (p) {                   /* loop over presets */
    p2 = ((SFPreset *) (p->data))->zone;
    while (p2) {                /* loop over preset's zones */
      sfont_free_zone (p2->data);
      p2 = fluid_list_next (p2);
    }                           /* free preset's zone list */
    delete_fluid_list (((SFPreset *) (p->data))->zone);
    FLUID_FREE (p->data);       /* free preset chunk */
    p = fluid_list_next (p);
  }
  delete_fluid_list (sf->preset);
  sf->preset = NULL;

  p = sf->inst;
  while (p) {                   /* loop over instruments */
    p2 = ((SFInst *) (p->data))->zone;
    while (p2) {                /* loop over inst's zones */
      sfont_free_zone (p2->data);
      p2 = fluid_list_next (p2);
    }                           /* free inst's zone list */
    delete_fluid_list (((SFInst *) (p->data))->zone);
    FLUID_FREE (p->data);
    p = fluid_list_next (p);
  }
  delete_fluid_list (sf->inst);
  sf->inst = NULL;

  p = sf->sample;
  while (p) {
    FLUID_FREE (p->data);
    p = fluid_list_next (p);
  }
  delete_fluid_list (sf->sample);
  sf->sample = NULL;

  FLUID_FREE (sf);
}

/* free all elements of a zone (Preset or Instrument) */
void sfont_free_zone (SFZone * zone) {
  fluid_list_t *p;

  if (!zone)
    return;

  p = zone->gen;
  while (p) {                   /* Free gen chunks for this zone */
    if (p->data)
      FLUID_FREE (p->data);
    p = fluid_list_next (p);
  }
  delete_fluid_list (zone->gen);  /* free genlist */

  p = zone->mod;
  while (p) {                   /* Free mod chunks for this zone */
    if (p->data)
      FLUID_FREE (p->data);
    p = fluid_list_next (p);
  }
  delete_fluid_list (zone->mod);  /* free modlist */

  FLUID_FREE (zone);            /* free zone chunk */
}

/* preset sort function, first by bank, then by preset # */
int sfont_preset_compare_func (void *a, void *b) {
  int aval, bval;

  aval = (int) (((SFPreset *) a)->bank) << 16 | ((SFPreset *) a)->prenum;
  bval = (int) (((SFPreset *) b)->bank) << 16 | ((SFPreset *) b)->prenum;

  return (aval - bval);
}

/* delete zone from zone list */
void sfont_zone_delete (SFData * sf, fluid_list_t ** zlist, SFZone * zone) {
  *zlist = fluid_list_remove (*zlist, (void *) zone);
  sfont_free_zone (zone);
}

/* Find generator in gen list */
fluid_list_t *gen_inlist (int gen, fluid_list_t * genlist) {  /* is generator in gen list? */
  fluid_list_t *p;

  p = genlist;
  while (p) {
    if (p->data == NULL)
      return (NULL);
    if (gen == ((SFGen *) p->data)->id)
      break;
    p = fluid_list_next (p);
  }
  return (p);
}

/* check validity of instrument generator */
int gen_valid (int gen) {       /* is generator id valid? */
  int i = 0;

  if (gen > Gen_MaxValid)
    return (FALSE);
  while (badgen[i] && badgen[i] != gen)
    i++;
  return (badgen[i] == 0);
}

/* check validity of preset generator */
int gen_validp (int gen) {      /* is preset generator valid? */
  int i = 0;

  if (!gen_valid (gen))
    return (FALSE);
  while (badpgen[i] && badpgen[i] != (unsigned short) gen)
    i++;
  return (badpgen[i] == 0);
}
