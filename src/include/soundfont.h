#ifndef FLUIDBEAN_SOUNDFONT
#define FLUIDBEAN_SOUNDFONT

#include <botox/data.h>
#include "fluidbean.h"

typedef struct _Sample {
  U8   origPitch;
  U8   origPitchAdj;
  U32  startIdx;      // I've yet to see a good reason for this if we have a pointer already.
  U32  endIdx;
  U32  loopStartIdx;
  U32  loopEndIdx;
  S16 *pcmDataP;      // Split pcm data apart to avoid duplicates.
} Sample;      // 20 bytes

// Modulator adjusts any given generator. Don't need to tell it which generator since the gen in question owns it.
typedef struct {	
  U8  src1;		
  U8  src2;   // This is kind of the second controller that controls the degree of the first.
  U8  xformType1;	
  U8  xformType2;	
  U8  dest;   // destination generator
  S16 productScale;	  // Scales the product of src1 transformed and src2 transformed.
  U32 amount;
} Modulator;   // 12 bytes 

// Generator is a component of a sound; e.g. vibrato LFO, 
// key/velocity range, filter cutoff/Q, source sample, etc.
typedef struct _Generator {
  U8 genType;	
  U8 nMods;
  U8 flags;
  U32 mod, nrpn, val;  // val is the soundfont's nominal value for this generator.
  Modulator *modA;
} Generator;   // 20 bytes

struct _Zone;

typedef struct {
  U8 nZones;  // only pertains to individual zones. Global zone is just tested for NULL.
  struct _Zone *globalZoneP;
  struct _Zone *zoneA;
} Instrument;  // 12 bytes

typedef Instrument Preset;  // Both have the same structure; so 4 bytes again!

// Zone contains all gens for an instrument or preset.
// Union has inst/sample because presets are made of 
// instruments, and instruments are made of samples.
typedef struct _Zone {
  U8             keylo;
  U8             keyhi;
  U8             vello;
  U8             velhi;
  U8             loopType;  // there's an enum for this
  U8             nGens;
  union {   // Finally found a good use case for unions: avoiding void* while restricting purpose.
    Sample     *sampleP;
    Instrument *instP;
  }             u;
  Generator    *genA;
} Zone;  // 16 bytes

typedef struct {
  U8 nPresets;
  Preset *presetA;
} Bank;  // 8 bytes

typedef struct {
  U8 nBanks;
  Bank *bankA;
} Soundfont;  // 8 bytes

#endif
