#ifndef MOD_H
#define MOD_H

#include "botox/data.h"
#include "soundfont.h"
#include "chan.h"
#include "voice.h"

// Modulators
#define NUM_MOD           64
void modClone (Modulator * mod, Modulator * src);
int modTestIdentity (Modulator * mod1, Modulator * mod2);
S32 modGetValue (Modulator * mod, Channel *chan, Voice * voice);

#define modHasSource(mod,cc,ctrl)  \
( ((((mod)->src1 == ctrl) && (((mod)->xformType1 & MOD_CC) != 0) && (cc != 0)) \
   || ((((mod)->src1 == ctrl) && (((mod)->xformType1 & MOD_CC) == 0) && (cc == 0)))) \
|| ((((mod)->src2 == ctrl) && (((mod)->xformType2 & MOD_CC) != 0) && (cc != 0)) \
    || ((((mod)->src2 == ctrl) && (((mod)->xformType2 & MOD_CC) == 0) && (cc == 0)))))

#define modHasDest(mod,gen)  ((mod)->dest == gen)

#endif
