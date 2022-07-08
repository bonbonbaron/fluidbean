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
  U32 amount;
  S16 productScale;	  // Scales the product of src1 transformed and src2 transformed.
} Modulator;   // 6 bytes 

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

U32 genScale (U8 gen, U32 value);
U32 genScaleNrpn (U8 gen, int data);

#define NUM_MOD           64

enum modFlags {
  MOD_POSITIVE = 0,
  MOD_NEGATIVE = 1,
  MOD_UNIPOLAR = 0,
  MOD_BIPOLAR = 2,
  MOD_LINEAR = 0,
  MOD_CONCAVE = 4,
  MOD_CONVEX = 8,
  MOD_SWITCH = 12,
  MOD_GC = 0,
  MOD_CC = 16
};

/* Flags telling the source of a modulator.  This corresponds to
 * SF2.01 section 8.2.1 */
enum modSrc {
  MOD_NONE = 0,
  MOD_VELOCITY = 2,
  MOD_KEY = 3,
  MOD_KEYPRESSURE = 10,
  MOD_CHANNELPRESSURE = 13,
  MOD_PITCHWHEEL = 14,
  MOD_PITCHWHEELSENS = 16
};

void modClone (Modulator * mod, Modulator * src);
int modTestIdentity (Modulator * mod1, Modulator * mod2);

#define modHasSource(mod,cc,ctrl)  \
( ((((mod)->src1 == ctrl) && (((mod)->flags1 & MOD_CC) != 0) && (cc != 0)) \
   || ((((mod)->src1 == ctrl) && (((mod)->flags1 & MOD_CC) == 0) && (cc == 0)))) \
|| ((((mod)->src2 == ctrl) && (((mod)->flags2 & MOD_CC) != 0) && (cc != 0)) \
    || ((((mod)->src2 == ctrl) && (((mod)->flags2 & MOD_CC) == 0) && (cc == 0)))))

#define modHasDest(mod,gen)  ((mod)->dest == gen)

#endif
