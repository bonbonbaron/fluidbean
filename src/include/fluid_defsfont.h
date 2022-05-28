/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * SoundFont loading code borrowed from Smurf SoundFont Editor by Josh Green
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */


#ifndef _FLUID_DEFSFONT_H
#define _FLUID_DEFSFONT_H


#include "fluidbean.h"
#include "fluidsynth_priv.h"
#include "fluid_list.h"



/********************************************************************************/
/********************************************************************************/
/********************************************************************************/
/********************************************************************************/
/********************************************************************************/

/*-----------------------------------sfont.h----------------------------*/

#define SF_SAMPMODES_LOOP	1
#define SF_SAMPMODES_UNROLL	2

#define SF_MIN_SAMPLERATE	400
#define SF_MAX_SAMPLERATE	50000

#define SF_MIN_SAMPLE_LENGTH	32

/* Sound Font structure defines */

typedef struct _SFVersion {			/* version structure */
	U16 major;
	U16 minor;
} SFVersion;

typedef struct _SFMod {					/* Modulator structure */
	U16 src;						/* source modulator */
	U16 dest;					/* destination generator */
	S16 amount;					/* signed, degree of modulation */
	U16 amtsrc;				/* second source controls amnt of first */
	U16 trans;					/* transform applied to source */
} SFMod;

typedef union _SFGenAmount {		/* Generator amount structure */
	S16 sword;						/* signed 16 bit value */
	U16 uword;					/* unsigned 16 bit value */
	struct {
		U8 lo;						/* low value for ranges */
		U8 hi;						/* high value for ranges */
	} range;
} SFGenAmount;

typedef struct _SFGen {					/* Generator structure */
	U16 id;						/* generator ID */
	SFGenAmount amount;						/* generator value */
} SFGen;

typedef struct _SFZone {				/* Sample/instrument zone structure */
	fluid_list_t *instsamp;				/* instrument/sample pointer for zone */
	fluid_list_t *gen;						/* list of generators */
	fluid_list_t *mod;						/* list of modulators */
} SFZone;

typedef struct _SFSample {			/* Sample structure */
	S8 name[21];								/* Name of sample */
	U8 samfile;				/* Loaded sfont/sample buffer = 0/1 */
	U32 start;						/* Offset in sample area to start of sample */
	U32 end;							/* Offset from start to end of sample,
																   this is the last point of the
																   sample, the SF spec has this as the
																   1st point after, corrected on
																   load/save */
	U32 loopstart;				/* Offset from start to start of loop */
	U32 loopend;					/* Offset from start to end of loop,
																   marks the first point after loop,
																   whose sample value is ideally
																   equivalent to loopstart */
	U32 samplerate;			/* Sample rate recorded at */
	U8 origpitch;			/* root midi key number */
	S8 pitchadj;					/* pitch correction in cents */
	U16 sampletype;		/* 1 mono,2 right,4 left,linked 8,0x8000=ROM */
} SFSample;

typedef struct _SFInst {				/* Instrument structure */
	S8 name[21];								/* Name of instrument */
	fluid_list_t *zone;						/* list of instrument zones */
} SFInst;

typedef struct _SFPreset {			/* Preset structure */
	S8 name[21];								/* preset name */
	U16 prenum;				/* preset number */
	U16 bank;					/* bank number */
	U32 libr;						/* Not used (preserved) */
	U32 genre;						/* Not used (preserved) */
	U32 morph;						/* Not used (preserved) */
	fluid_list_t *zone;						/* list of preset zones */
} SFPreset;

/* NOTE: sffd is also used to determine if sound font is new (NULL) */
typedef struct _SFData {				/* Sound font data structure */
	U32 samplepos;				/* position within sffd of the sample chunk */
	U32 samplesize;			/* length within sffd of the sample chunk */
	void *srcSfDataP;				      // MB: this is inflatable data the SF comes from in JB.
	fluid_list_t *info;						/* linked list of info strings (1st byte is ID) */
	fluid_list_t *preset;					/* linked list of preset info */
	fluid_list_t *inst;						/* linked list of instrument info */
	fluid_list_t *sample;					/* linked list of sample info */
} SFData;

typedef struct {

} Soundfont;   // MB

/* sf file chunk IDs */
enum { UNKN_ID, RIFF_ID, LIST_ID, SFBK_ID,
	INFO_ID, SDTA_ID, PDTA_ID,		/* info/sample/preset */

	IFIL_ID, ISNG_ID, INAM_ID, IROM_ID,	/* info ids (1st byte of info strings) */
	IVER_ID, ICRD_ID, IENG_ID, IPRD_ID,	/* more info ids */
	ICOP_ID, ICMT_ID, ISFT_ID,		/* and yet more info ids */

	SNAM_ID, SMPL_ID,							/* sample ids */
	PHDR_ID, PBAG_ID, PMOD_ID, PGEN_ID,	/* preset ids */
	IHDR_ID, IBAG_ID, IMOD_ID, IGEN_ID,	/* instrument ids */
	SHDR_ID												/* sample info */
};

/* generator types */
typedef enum { Gen_StartAddrOfs, Gen_EndAddrOfs, Gen_StartLoopAddrOfs,
	Gen_EndLoopAddrOfs, Gen_StartAddrCoarseOfs, Gen_ModLFO2Pitch,
	Gen_VibLFO2Pitch, Gen_ModEnv2Pitch, Gen_FilterFc, Gen_FilterQ,
	Gen_ModLFO2FilterFc, Gen_ModEnv2FilterFc, Gen_EndAddrCoarseOfs,
	Gen_ModLFO2Vol, Gen_Unused1, Gen_ChorusSend, Gen_ReverbSend, Gen_Pan,
	Gen_Unused2, Gen_Unused3, Gen_Unused4,
	Gen_ModLFODelay, Gen_ModLFOFreq, Gen_VibLFODelay, Gen_VibLFOFreq,
	Gen_ModEnvDelay, Gen_ModEnvAttack, Gen_ModEnvHold, Gen_ModEnvDecay,
	Gen_ModEnvSustain, Gen_ModEnvRelease, Gen_Key2ModEnvHold,
	Gen_Key2ModEnvDecay, Gen_VolEnvDelay, Gen_VolEnvAttack,
	Gen_VolEnvHold, Gen_VolEnvDecay, Gen_VolEnvSustain, Gen_VolEnvRelease,
	Gen_Key2VolEnvHold, Gen_Key2VolEnvDecay, Gen_Instrument,
	Gen_Reserved1, Gen_KeyRange, Gen_VelRange,
	Gen_StartLoopAddrCoarseOfs, Gen_Keynum, Gen_Velocity,
	Gen_Attenuation, Gen_Reserved2, Gen_EndLoopAddrCoarseOfs,
	Gen_CoarseTune, Gen_FineTune, Gen_SampleId, Gen_SampleModes,
	Gen_Reserved3, Gen_ScaleTune, Gen_ExclusiveClass, Gen_OverrideRootKey,
	Gen_Dummy
} Gen_Type;

#define Gen_MaxValid 	Gen_Dummy - 1	/* maximum valid generator */
#define Gen_Count	Gen_Dummy			/* count of generators */
#define GenArrSize sizeof(SFGenAmount)*Gen_Count	/* gen array size */

/* generator unit type */
typedef enum {
	None,													/* No unit type */
	Unit_Smpls,										/* in samples */
	Unit_32kSmpls,								/* in 32k samples */
	Unit_Cent,										/* in cents (1/100th of a semitone) */
	Unit_HzCent,									/* in Hz Cents */
	Unit_TCent,										/* in Time Cents */
	Unit_cB,											/* in centibels (1/100th of a decibel) */
	Unit_Percent,									/* in percentage */
	Unit_Semitone,								/* in semitones */
	Unit_Range										/* a range of values */
} Gen_Unit;

/* global data */

extern U16 badgen[];	/* list of bad generators */
extern U16 badpgen[];	/* list of bad preset generators */

/* functions */
void sfont_init_chunks (void);

void sfont_close (SFData * sf);
void sfont_free_zone (SFZone * zone);
S32 sfont_preset_compare_func (void *a, void *b);

void sfont_zone_delete (SFData * sf, fluid_list_t ** zlist, SFZone * zone);

fluid_list_t *gen_inlist (S32 gen, fluid_list_t * genlist);
S32 gen_valid (S32 gen);
S32 gen_validp (S32 gen);


/*-----------------------------------sffile.h----------------------------*/
/*
   File structures and routines (used to be in sffile.h)
*/

#define CHNKIDSTR(id)           &idlist[(id - 1) * 4]

/* sfont file chunk sizes */
#define SFPHDRSIZE	38
#define SFBAGSIZE	4
#define SFMODSIZE	10
#define SFGENSIZE	4
#define SFIHDRSIZE	22
#define SFSHDRSIZE	46

/* sfont file data structures */
typedef struct _SFChunk {				/* RIFF file chunk structure */
	U32 id;							/* chunk id */
	U32 size;						/* size of the following chunk */
} SFChunk;

typedef struct _SFPhdr {
	U8 name[20];				/* preset name */
	U16 preset;				/* preset number */
	U16 bank;					/* bank number */
	U16 pbagndx;				/* index into preset bag */
	U32 library;					/* just for preserving them */
	U32 genre;						/* Not used */
	U32 morphology;			/* Not used */
} SFPhdr;

typedef struct _SFBag {
	U16 genndx;				/* index into generator list */
	U16 modndx;				/* index into modulator list */
} SFBag;

typedef struct _SFIhdr {
	S8 name[20];								/* Name of instrument */
	U16 ibagndx;				/* Instrument bag index */
} SFIhdr;

typedef struct _SFShdr {				/* Sample header loading struct */
	S8 name[20];								/* Sample name */
	U32 start;						/* Offset to start of sample */
	U32 end;							/* Offset to end of sample */
	U32 loopstart;				/* Offset to start of loop */
	U32 loopend;					/* Offset to end of loop */
	U32 samplerate;			/* Sample rate recorded at */
	U8 origpitch;			/* root midi key number */
	S8 pitchadj;					/* pitch correction in cents */
	U16 samplelink;		/* Not used */
	U16 sampletype;		/* 1 mono,2 right,4 left,linked 8,0x8000=ROM */
} SFShdr;

/* data */
extern S8 idlist[];

/* functions */
SFData *sfLoadMem (void *sfDataP, S32 dataLen, U8 **inPP);



/********************************************************************************/
/********************************************************************************/
/********************************************************************************/
/********************************************************************************/
/********************************************************************************/

/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


/* Provide definitions for some commonly used macros.
 *  Some of them are only provided if they haven't already
 *  been defined. It is assumed that if they are already
 *  defined then the current definition is correct.
 */
#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(!FALSE)
#endif

#define GPOINTER_TO_INT(p)	((uintptr)   (p))
#define GINT_TO_POINTER(i)      ((void *)  (uintptr)(i))

S8 *g_strdup (const S8 *str);





/* Provide simple macro statement wrappers (adapted from Perl):
 *  G_STMT_START { statements; } G_STMT_END;
 *  can be used as a single statement, as in
 *  if (x) G_STMT_START { ... } G_STMT_END; else ...
 *
 *  For gcc we will wrap the statements within `({' and `})' braces.
 *  For SunOS they will be wrapped within `if (1)' and `else (void) 0',
 *  and otherwise within `do' and `while (0)'.
 */
#if !(defined (G_STMT_START) && defined (G_STMT_END))
#if defined (__GNUC__) && !defined (__STRICT_ANSI__) && !defined (__cplusplus)
#define G_STMT_START	(void)(
#define G_STMT_END		)
#else
#if (defined (sun) || defined (__sun__))
#define G_STMT_START	if (1)
#define G_STMT_END	else (void)0
#else
#define G_STMT_START	do
#define G_STMT_END	while (0)
#endif
#endif
#endif


/* Basic bit swapping functions
 */
#define GUINT16_SWAP_LE_BE_CONSTANT(val)	((U16) ( \
    (((U16) (val) & (U16) 0x00ffU) << 8) | \
    (((U16) (val) & (U16) 0xff00U) >> 8)))
#define GUINT32_SWAP_LE_BE_CONSTANT(val)	((U32) ( \
    (((U32) (val) & (U32) 0x000000ffU) << 24) | \
    (((U32) (val) & (U32) 0x0000ff00U) <<  8) | \
    (((U32) (val) & (U32) 0x00ff0000U) >>  8) | \
    (((U32) (val) & (U32) 0xff000000U) >> 24)))

#define GUINT16_SWAP_LE_BE(val) (GUINT16_SWAP_LE_BE_CONSTANT (val))
#define GUINT32_SWAP_LE_BE(val) (GUINT32_SWAP_LE_BE_CONSTANT (val))

#define GINT16_TO_LE(val)	((S16) (val))
#define GINT16_TO_LE(val)	((S16) (val))
#define GUINT16_TO_LE(val)	((U16) (val))
#define GINT16_TO_BE(val)	((S16) GUINT16_SWAP_LE_BE (val))
#define GINT16_TO_BE(val)	((S16) GUINT16_SWAP_LE_BE (val))
#define GUINT16_TO_BE(val)	(GUINT16_SWAP_LE_BE (val))
#define GINT32_TO_LE(val)	((S32) (val))
#define GINT32_TO_LE(val)	((S32) (val))
#define GUINT32_TO_LE(val)	((U32) (val))
#define GINT32_TO_BE(val)	((S32) GUINT32_SWAP_LE_BE (val))
#define GINT32_TO_BE(val)	((S32) GUINT32_SWAP_LE_BE (val))
#define GUINT32_TO_BE(val)	(GUINT32_SWAP_LE_BE (val))

/* The G*_TO_?E() macros are defined in glibconfig.h.
 * The transformation is symmetric, so the FROM just maps to the TO.
 */
#define GINT16_FROM_LE(val)	(GINT16_TO_LE (val))
#define GUINT16_FROM_LE(val)	(GUINT16_TO_LE (val))
#define GINT16_FROM_BE(val)	(GINT16_TO_BE (val))
#define GUINT16_FROM_BE(val)	(GUINT16_TO_BE (val))
#define GINT32_FROM_LE(val)	(GINT32_TO_LE (val))
#define GUINT32_FROM_LE(val)	(GUINT32_TO_LE (val))
#define GINT32_FROM_BE(val)	(GINT32_TO_BE (val))
#define GUINT32_FROM_BE(val)	(GUINT32_TO_BE (val))


/*-----------------------------------util.h----------------------------*/
/*
  Utility functions (formerly in util.h)
 */
#define FAIL	0
#define OK	1

enum { ErrWarn, ErrFatal, ErrStatus, ErrCorr, ErrEof, ErrMem, Errno,
	ErrRead, ErrWrite
};

#define ErrMax		ErrWrite
#define ErrnoStart	Errno
#define ErrnoEnd	ErrWrite

S32 gerr (S32 ev, S8 *fmt, ...);


/********************************************************************************/
/********************************************************************************/
/********************************************************************************/
/********************************************************************************/
/********************************************************************************/



/***************************************************************
 *
 *       FORWARD DECLARATIONS
 */
typedef struct _fluid_defsfont_t fluid_defsfont_t;
typedef struct _fluid_defpreset_t fluid_defpreset_t;
typedef struct _fluid_preset_zone_t fluid_preset_zone_t;
typedef struct _fluid_inst_t fluid_inst_t;
typedef struct _fluid_inst_zone_t fluid_inst_zone_t;

/*

  Public interface

 */

fluid_sfloader_t *new_fluid_defsfloader (void);
S32 delete_fluid_defsfloader (fluid_sfloader_t * loader);
fluid_sfont_t *fluid_defsfloader_load (void *sfontPrevDataP, void *sfDataP, S32 sfDataLen);

S32 fluid_defsfont_sfont_delete (fluid_sfont_t * sfont);
fluid_preset_t *fluid_defsfont_sfont_get_preset (fluid_sfont_t * sfont,
																								 U32 bank,
																								 U32 prenum);
void fluid_defsfont_sfont_iteration_start (fluid_sfont_t * sfont);
S32 fluid_defsfont_sfont_iteration_next (fluid_sfont_t * sfont,
																				 fluid_preset_t * preset);


S32 fluid_defpreset_preset_delete (fluid_preset_t * preset);
S32 fluid_defpreset_preset_get_banknum (fluid_preset_t * preset);
S32 fluid_defpreset_preset_get_num (fluid_preset_t * preset);
S32 fluid_defpreset_preset_noteon (fluid_preset_t * preset,
																	 fluid_synth_t * synth, S32 chan, S32 key,
																	 S32 vel);


/*
 * fluid_defsfont_t
 */
struct _fluid_defsfont_t {
	U32 samplepos;				/* the position in the file at which the sample data starts */
	U32 samplesize;			/* the size of the sample data */
	S16 *sampledata;						/* the sample data, loaded in ram */
	fluid_list_t *sample;					/* the samples in this soundfont */
	fluid_defpreset_t *preset;		/* the presets of this soundfont */

	fluid_preset_t iter_preset;		/* preset interface used in the iteration */
	fluid_defpreset_t *iter_cur;	/* the current preset in the iteration */
};


fluid_defsfont_t *new_fluid_defsfont (void);
S32 delete_fluid_defsfont (fluid_defsfont_t * sfont);
S32 fluid_defsfont_load (fluid_defsfont_t * sfont, void *sfDataP, S32 sfDataLen);
fluid_defpreset_t *fluid_defsfont_get_preset (fluid_defsfont_t * sfont,
																							U32 bank,
																							U32 prenum);
void fluid_defsfont_iteration_start (fluid_defsfont_t * sfont);
S32 fluid_defsfont_iteration_next (fluid_defsfont_t * sfont,
																	 fluid_preset_t * preset);
S32 fluid_defsfont_load_sampledata (fluid_defsfont_t * sfont, void *sfDataP);
S32 fluid_defsfont_add_sample (fluid_defsfont_t * sfont,
															 fluid_sample_t * sample);
S32 fluid_defsfont_add_preset (fluid_defsfont_t * sfont,
															 fluid_defpreset_t * preset);
fluid_sample_t *fluid_defsfont_get_sample (fluid_defsfont_t * sfont, S8 *s);


/*
 * fluid_preset_t
 */
struct _fluid_defpreset_t {
	fluid_defpreset_t *next;
	fluid_defsfont_t *sfont;			/* the soundfont this preset belongs to */
	S8 name[21];								/* the name of the preset */
	U32 bank;						/* the bank number */
	U32 num;							/* the preset number */
	fluid_preset_zone_t *global_zone;	/* the global zone of the preset */
	fluid_preset_zone_t *zone;		/* the chained list of preset zones */
};

fluid_defpreset_t *new_fluid_defpreset (fluid_defsfont_t * sfont);
S32 delete_fluid_defpreset (fluid_defpreset_t * preset);
fluid_defpreset_t *fluid_defpreset_next (fluid_defpreset_t * preset);
S32 fluid_defpreset_import_sfont (fluid_defpreset_t * preset,
																	SFPreset * sfpreset,
																	fluid_defsfont_t * sfont);
S32 fluid_defpreset_set_global_zone (fluid_defpreset_t * preset,
																		 fluid_preset_zone_t * zone);
S32 fluid_defpreset_add_zone (fluid_defpreset_t * preset,
															fluid_preset_zone_t * zone);
fluid_preset_zone_t *fluid_defpreset_get_zone (fluid_defpreset_t * preset);
fluid_preset_zone_t *fluid_defpreset_get_global_zone (fluid_defpreset_t *
																											preset);
S32 fluid_defpreset_get_banknum (fluid_defpreset_t * preset);
S32 fluid_defpreset_get_num (fluid_defpreset_t * preset);
S32 fluid_defpreset_noteon (fluid_defpreset_t * preset, fluid_synth_t * synth,
														S32 chan, S32 key, S32 vel);

/*
 * fluid_preset_zone
 */
struct _fluid_preset_zone_t {
	fluid_preset_zone_t *next;
	S8 *name;
	fluid_inst_t *inst;
	S32 keylo;
	S32 keyhi;
	S32 vello;
	S32 velhi;
	fluid_gen_t gen[GEN_LAST];
	fluid_mod_t *mod;							/* List of modulators */
};

fluid_preset_zone_t *new_fluid_preset_zone (S8 *name);
S32 delete_fluid_preset_zone (fluid_preset_zone_t * zone);
fluid_preset_zone_t *fluid_preset_zone_next (fluid_preset_zone_t * preset);
S32 fluid_preset_zone_import_sfont (fluid_preset_zone_t * zone,
																		SFZone * sfzone,
																		fluid_defsfont_t * sfont);
S32 fluid_preset_zone_inside_range (fluid_preset_zone_t * zone, S32 key,
																		S32 vel);
fluid_inst_t *fluid_preset_zone_get_inst (fluid_preset_zone_t * zone);

/*
 * fluid_inst_t
 */
struct _fluid_inst_t {
	S8 name[21];
	fluid_inst_zone_t *global_zone;
	fluid_inst_zone_t *zone;
};

fluid_inst_t *new_fluid_inst (void);
S32 delete_fluid_inst (fluid_inst_t * inst);
S32 fluid_inst_import_sfont (fluid_inst_t * inst, SFInst * sfinst,
														 fluid_defsfont_t * sfont);
S32 fluid_inst_set_global_zone (fluid_inst_t * inst,
																fluid_inst_zone_t * zone);
S32 fluid_inst_add_zone (fluid_inst_t * inst, fluid_inst_zone_t * zone);
fluid_inst_zone_t *fluid_inst_get_zone (fluid_inst_t * inst);
fluid_inst_zone_t *fluid_inst_get_global_zone (fluid_inst_t * inst);

/*
 * fluid_inst_zone_t
 */
struct _fluid_inst_zone_t {
	fluid_inst_zone_t *next;
	S8 *name;
	fluid_sample_t *sample;
	S32 keylo;
	S32 keyhi;
	S32 vello;
	S32 velhi;
	fluid_gen_t gen[GEN_LAST];
	fluid_mod_t *mod;							/* List of modulators */
};

fluid_inst_zone_t *new_fluid_inst_zone (S8 *name);
S32 delete_fluid_inst_zone (fluid_inst_zone_t * zone);
fluid_inst_zone_t *fluid_inst_zone_next (fluid_inst_zone_t * zone);
S32 fluid_inst_zone_import_sfont (fluid_inst_zone_t * zone, SFZone * sfzone,
																	fluid_defsfont_t * sfont);
S32 fluid_inst_zone_inside_range (fluid_inst_zone_t * zone, S32 key, S32 vel);
fluid_sample_t *fluid_inst_zone_get_sample (fluid_inst_zone_t * zone);



fluid_sample_t *new_fluid_sample (void);
S32 delete_fluid_sample (fluid_sample_t * sample);
S32 fluid_sample_import_sfont (fluid_sample_t * sample, SFSample * sfsample,
															 fluid_defsfont_t * sfont);
S32 fluid_sample_in_rom (fluid_sample_t * sample);


#endif /* _FLUID_SFONT_H */
