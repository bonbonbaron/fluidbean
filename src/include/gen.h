#ifndef GEN_H
#define GEN_H

#include "botox/data.h"
struct _Generator;
// Generators
U32 genScale (U8 gen, U32 value);
U32 genScaleNrpn (U8 gen, int data);
int genInit (struct _Generator *voiceGenA, S32 *chanGenA, S8 *chanGenAbsA);

#endif
