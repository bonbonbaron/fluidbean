#include "fluidbean.h"
#include "data.h"
#include "enums.h"
#include "soundfont.h"
#include "chan.h"

typedef struct {
	S8 init;										/* Does the generator need to be initialized (cfr. voiceInit()) */
	S8 nrpnScale;							/* The scale to convert from NRPN (cfr. genMapNrpn()) */
	S32 min;										/* The minimum value */
	S32 max;										/* The maximum value */
	S32 def;										/* The default value (cfr. genSetDefaultValues()) */
} GenDefault;

/* See SFSpec21 $8.1.3 */
GenDefault genDefaultsA[] = {
	/* init  scale         min        max         de */
	{1, 1, 0, 0xFFFFFFFF, 0}, // GEN_STARTADDROFS, 
	{1, 1, 0, 0xFFFFFFFF, 0}, // GEN_ENDADDROFS, 
	{1, 1, 0, 0xFFFFFFFF, 0}, // GEN_STARTLOOPADDROFS, 
	{1, 1, 0, 0xFFFFFFFF, 0}, // GEN_ENDLOOPADDROFS, 
	{0, 1, 0, 0xFFFFFFFF, 0}, // GEN_STARTADDRCOARSEOFS, 
	{1, 2, -12000, 12000, 0}, // GEN_MODLFOTOPITCH, 
	{1, 2, -12000, 12000, 0}, // GEN_VIBLFOTOPITCH, 
	{1, 2, -12000, 12000, 0}, // GEN_MODENVTOPITCH, 
	{1, 2, 1500, 13500, 13500}, // GEN_FILTERFC, 
	{1, 1, 0, 960, 0}, // GEN_FILTERQ, 
	{1, 2, -12000, 12000, 0}, // GEN_MODLFOTOFILTERFC, 
	{1, 2, -12000, 12000, 0}, // GEN_MODENVTOFILTERFC, 
	{0, 1, 0, 0, 0}, // GEN_ENDADDRCOARSEOFS, 
	{1, 1, -960, 960, 0}, // GEN_MODLFOTOVOL, 
	{0, 0, 0, 0, 0}, // GEN_UNUSED1, 
	{1, 1, 0, 1000, 0}, // GEN_CHORUSSEND, 
	{1, 1, 0, 1000, 0}, // GEN_REVERBSEND, 
	{1, 1, -500, 500, 0}, // GEN_PAN, 
	{0, 0, 0, 0, 0}, // GEN_UNUSED2, 
	{0, 0, 0, 0, 0}, // GEN_UNUSED3, 
	{0, 0, 0, 0, 0}, // GEN_UNUSED4, 
	{1, 2, -12000, 5000, -12000}, // GEN_MODLFODELAY, 
	{1, 4, -16000, 4500, 0}, // GEN_MODLFOFREQ, 
	{1, 2, -12000, 5000, -12000}, // GEN_VIBLFODELAY, 
	{1, 4, -16000, 4500, 0}, // GEN_VIBLFOFREQ, 
	{1, 2, -12000, 5000, -12000}, // GEN_MODENVDELAY, 
	{1, 2, -12000, 8000, -12000}, // GEN_MODENVATTACK, 
	{1, 2, -12000, 5000, -12000}, // GEN_MODENVHOLD, 
	{1, 2, -12000, 8000, -12000}, // GEN_MODENVDECAY, 
	{0, 1, 0, 1000, 0}, // GEN_MODENVSUSTAIN, 
	{1, 2, -12000, 8000, -12000}, // GEN_MODENVRELEASE, 
	{0, 1, -1200, 1200, 0}, // GEN_KEYTOMODENVHOLD, 
	{0, 1, -1200, 1200, 0}, // GEN_KEYTOMODENVDECAY, 
	{1, 2, -12000, 5000, -12000}, // GEN_VOLENVDELAY, 
	{1, 2, -12000, 8000, -12000}, // GEN_VOLENVATTACK, 
	{1, 2, -12000, 5000, -12000}, // GEN_VOLENVHOLD, 
	{1, 2, -12000, 8000, -12000}, // GEN_VOLENVDECAY, 
	{0, 1, 0, 1440, 0}, // GEN_VOLENVSUSTAIN, 
	{1, 2, -12000, 8000, -12000}, // GEN_VOLENVRELEASE, 
	{0, 1, -1200, 1200, 0}, // GEN_KEYTOVOLENVHOLD, 
	{0, 1, -1200, 1200, 0}, // GEN_KEYTOVOLENVDECAY, 
	{0, 0, 0, 0, 0}, // GEN_INSTRUMENT, 
	{0, 0, 0, 0, 0}, // GEN_RESERVED1, 
	{0, 0, 0, 127, 0}, // GEN_KEYRANGE, 
	{0, 0, 0, 127, 0}, // GEN_VELRANGE, 
	{0, 1, 0, 0xFFFFFFFF, 0}, // GEN_STARTLOOPADDRCOARSEOFS, 
	{1, 0, 0, 127, -1}, // GEN_KEYNUM, 
	{1, 1, 0, 127, -1}, // GEN_VELOCITY, 
	{1, 1, 0, 1440, 0}, // GEN_ATTENUATION, 
	{0, 0, 0, 0, 0}, // GEN_RESERVED2, 
	{0, 1, 0, 0xFFFFFFFF, 0}, // GEN_ENDLOOPADDRCOARSEOFS, 
	{0, 1, -120, 120, 0}, // GEN_COARSETUNE, 
	{0, 1, -99, 99, 0}, // GEN_FINETUNE, 
	{0, 0, 0, 0, 0}, // GEN_SAMPLEID, 
	{0, 0, 0, 0, 0}, // GEN_SAMPLEMODE, 
	{0, 0, 0, 0, 0}, // GEN_RESERVED3, 
	{0, 1, 0, 1200, 100}, // GEN_SCALETUNE, 
	{0, 0, 0, 0, 0}, // GEN_EXCLUSIVECLASS, 
	{1, 0, 0, 127, -1}, // GEN_OVERRIDEROOTKEY, 
	{1, 0, 0, 127, 0} // GEN_PITCH, 
};

void genSetDefaultValues (Generator *genA, U8 nGens) {
  Generator *genEndP = genA + nGens;
  for (Generator *genP = genA; genP < genEndP; ++genP) {
		genP->flags = GEN_UNUSED;
		genP->mod = 0;
		genP->nrpn = 0;
		genP->val = genDefaultsA[genP->genType].def;  // val is the soundfont's nominal value for this generator.
	}
}


/* genInit: Set an array of generators to their initial value */
int genInit (Generator * genA, U8 nGens, Channel * channelP) {
	genSetDefaultValues(genA, nGens);

  Generator *genEndP = genA + nGens;

  for (Generator *genP = genA; genP < genEndP; ++genP) {
		genP->nrpn = channelGetGen (channelP, genP->genType);
		/* This is an extension to the SoundFont standard. More
		 * documentation is available at the synthSetGen2()
		 * function. */
    if (channelP->genAbs[genP->genType])
			genP->flags = GEN_ABS_NRPN;
	}

	return OK;
}

U32 genScale (U8 gen, U32 value) {
	return (genDefaultsA[gen].min + value * (genDefaultsA[gen].max - genDefaultsA[gen].min));
}

U32 genScaleNrpn (U8 gen, int data) {
	realT value = data - 8192;
	clip (value, -8192, 8192);
	return value * genDefaultsA[gen].nrpnScale;
}
