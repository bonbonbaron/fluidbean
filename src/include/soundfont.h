#ifndef FLUIDBEAN_SOUNDFONT
#define FLUIDBEAN_SOUNDFONT
#include <botox/data.h>

typedef struct {
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
  S16 productScale;	  // Scales the product of src1 transformed and src2 transformed.
} Modulator;   // 6 bytes 

typedef union {
  S16 s16;			
  U16 u16;		
  struct {
    unsigned char u8Lo;			
    unsigned char u8Hi;			
  } range;
} GenAmount;  // 2 bytes

// Generator is a component of a sound; e.g. vibrato LFO, 
// key/velocity range, filter cutoff/Q, source sample, etc.
typedef struct {
  U8 genType;	
  U8 nMods;
  GenAmount amount;	
  Modulator *modA;  // I get the mod array appeal, but data accessed together should stay together.
} Generator;   // 8 bytes

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
  U8           nGens;
  union {   // Finally found a good use case for unions: avoiding void* while restricting purpose.
    Sample     *sampleP;
    Instrument *instP;
  }             u;
  Generator    *genA;
} Zone;  // 12 bytes

typedef struct {
  U8 nPresets;
  Preset *presetA;
} Bank;  // 8 bytes

typedef struct {
  U8 nBanks;
  Bank *bankA;
} Soundfont;  // 8 bytes
#endif
