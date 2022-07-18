#include "botox/data.h"

// Basing this design off of embedded-friendly chorus paper:
// https://upcommons.upc.edu/bitstream/handle/2117/98219/pfc_doc.pdf?sequence=1&isAllowed=y

//~~~~~~~~~~~~~~~~~~~~~~~~
// 1. Store input sample into ring buffer of 1024 samples.
#define N_INPUT_BUFFER_SAMPLES (1024)
typedef struct {
  U16 inputBufferIdx;
  S16 *inputBufferA;  // will be allocated to 1024 samples
} Chorus;

//~~~~~~~~~~~~~~~~~~~~~~~~
// 2. Feed sample to (a) envelope follower, (b) pitch change detector, and (c) random number generator.
//    <a> Envelope follower:      y[n] = _lowPassFilter(_abs(x[n])) >> K
inline static S16 _abs (S16 x) {
  register S16 const absMask_ = x >> ((sizeof(S16) << 3) - 1); \
  return ((x + absMask_) ^ absMask_);
}

#if 0  // until we set N to anything other than 16, use the second, less flexible but optimal low-pass.
inline static S16 _lowPassFilter(S16 x, S16 lpfDelayFeedback, const S16 nBitshiftRight) {  
  return ((((1 << nBitshiftRight) - 1) * lpfDelayFeedback) + x) >> nBitshiftRight;
}
#else
inline static S16 _lowPassFilter(S16 x, S16 yPrev) {
  return ((15 * yPrev) + x) >> 4;
}
#endif

inline static S16 _envelopeFollower(S16 x, S16 yPrev, S16 K) {
  return _lowPassFilter(_abs(x), yPrev) >> K;
}

//    <b> Pitch change detector: 
// TODO: refer to eq. 2.13 for coeff kf of eq. 2.12.
// TODO: brush up on how to compute poles and zeros (kp and kz)
// TODO: design latching of prevous 
inline static S16 _bandPassFilter(S16 x, S16 xPrev, S16 xPrevPrev, S16 yPrev, S16 yPrevPrev) {
  // TODO: stick all k's into registers.
  return   2 * kp * kf * yPrev 
         - kp * kp * yPrevPrev
         - 2 * kz * kf * xPrev
         + kz * kz * xPrevPrev
         + x;
}

//    <c> Random number generator
# define randomNumberGenerator_(x) (x & 0x0F)   // modulo 2^N; author chose N = 4.

//~~~~~~~~~~~~~~~~~~~~~~~~
