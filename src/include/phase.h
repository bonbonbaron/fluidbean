#ifndef _PHASE_H
#define _PHASE_H

#include "config.h"

/*
 *  phase
 */

#define INTERP_BITS        8
#define INTERP_BITS_MASK   0xff000000
#define INTERP_BITS_SHIFT  24
#define INTERP_MAX         256

#define FRACT_MAX ((double)4294967296.0)

/* phaseT
* Purpose:
* Playing pointer for voice playback
*
* When a sample is played back at a different pitch, the playing pointer in the
* source sample will not advance exactly one sample per output sample.
* This playing pointer is implemented using phaseT.
* It is a 64 bit number. The higher 32 bits contain the 'index' (number of
* the current sample), the lower 32 bits the fractional part.
*/
typedef unsigned long long phaseT;

/* Purpose:
 * Set a to b.
 * a: phaseT
 * b: phaseT
 */
#define phaseSet(a,b) a=b;

#define phaseSetInt(a, b)    ((a) = ((unsigned long long)(b)) << 32)

/* Purpose:
 * Sets the phase a to a phase increment given in b.
 * For example, assume b is 0.9. After setting a to it, adding a to
 * the playing pointer will advance it by 0.9 samples. */
#define phaseSetFloat(a, b) \
  (a) = (((unsigned long long)(b)) << 32) \
  | (uint32) (((double)(b) - (S32)(b)) * (double)FRACT_MAX)

/* create a phaseT from an index and a fraction value */
#define phaseFromIndexFract(index, fract) \
  ((((unsigned long long)(index)) << 32) + (fract))

/* Purpose:
 * Return the index and the fractional part, respectively. */
#define phaseIndex(_x) \
  ((U32)((_x) >> 32))
#define phaseFract(_x) \
  ((uint32)((_x) & 0xFFFFFFFF))

/* Get the phase index with fractional rounding */
#define phaseIndexRound(_x) \
  ((U32)(((_x) + 0x80000000) >> 32))


/* Purpose:
 * Takes the fractional part of the argument phase and
 * calculates the corresponding position in the interpolation table.
 * The fractional position of the playing pointer is calculated with a quite high
 * resolution (32 bits). It would be unpractical to keep a set of interpolation
 * coefficients for each possible fractional part...
 */
#define phaseFractToTablerow(_x) \
  ((U32)(phaseFract(_x) & INTERP_BITS_MASK) >> INTERP_BITS_SHIFT)

#define phaseDouble(_x) \
  ((double)(phaseIndex(_x)) + ((double)phaseFract(_x) / FRACT_MAX))

/* Purpose:
 * Advance a by a step of b (both are phaseT).
 */
#define phaseIncr(a, b)  a += b

/* Purpose:
 * Subtract b from a (both are phaseT).
 */
#define phaseDecr(a, b)  a -= b

/* Purpose:
 * Subtract b samples from a.
 */
#define phaseSubInt(a, b)  ((a) -= (unsigned long long)(b) << 32)

/* Purpose:
 * Creates the expression a.index++. */
#define phaseIndexPlusplus(a)  (((a) += 0x100000000LL)

#endif /* _PHASE_H */
