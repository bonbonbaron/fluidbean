/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
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

#ifndef _FLUID_MOD_H
#define _FLUID_MOD_H

#include "chan.h"
#include "voice.h"

#define FLUID_NUM_MOD           64

enum fluid_mod_flags {
  FLUID_MOD_POSITIVE = 0,
  FLUID_MOD_NEGATIVE = 1,
  FLUID_MOD_UNIPOLAR = 0,
  FLUID_MOD_BIPOLAR = 2,
  FLUID_MOD_LINEAR = 0,
  FLUID_MOD_CONCAVE = 4,
  FLUID_MOD_CONVEX = 8,
  FLUID_MOD_SWITCH = 12,
  FLUID_MOD_GC = 0,
  FLUID_MOD_CC = 16
};

/* Flags telling the source of a modulator.  This corresponds to
 * SF2.01 section 8.2.1 */
enum fluid_mod_src
{
  FLUID_MOD_NONE = 0,
  FLUID_MOD_VELOCITY = 2,
  FLUID_MOD_KEY = 3,
  FLUID_MOD_KEYPRESSURE = 10,
  FLUID_MOD_CHANNELPRESSURE = 13,
  FLUID_MOD_PITCHWHEEL = 14,
  FLUID_MOD_PITCHWHEELSENS = 16
};

#include "fluidbean.h"

void fluid_mod_clone (Modulator * mod, Modulator * src);
fluid_real_t fluid_mod_get_value (Modulator * mod, struct _Channel * chan,
																	struct _Voice * voice);
void fluid_dump_modulator (Modulator * mod);

#define fluid_mod_has_source(mod,cc,ctrl)  \
( ((((mod)->src1 == ctrl) && (((mod)->flags1 & FLUID_MOD_CC) != 0) && (cc != 0)) \
   || ((((mod)->src1 == ctrl) && (((mod)->flags1 & FLUID_MOD_CC) == 0) && (cc == 0)))) \
|| ((((mod)->src2 == ctrl) && (((mod)->flags2 & FLUID_MOD_CC) != 0) && (cc != 0)) \
    || ((((mod)->src2 == ctrl) && (((mod)->flags2 & FLUID_MOD_CC) == 0) && (cc == 0)))))

#define fluid_mod_has_dest(mod,gen)  ((mod)->dest == gen)


#endif /* _FLUID_MOD_H */
