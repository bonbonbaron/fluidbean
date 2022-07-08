#include "data.h"

#define byteIdx_(key) ((key - 1) >> 3)
#define bitFlag_(key) (1 << ((key - 1) & 0x07))

inline Error jbAlloc(void **voidPP, U32 elemSz, U32 nElems) {
	if (voidPP == NULL)
		return E_BAD_ARGS;
	*voidPP = malloc(nElems * elemSz);
	if (*voidPP == NULL)
		return E_NO_MEMORY;
	return SUCCESS;
}

inline void jbFree(void **voidPP) {
	if (voidPP != NULL) {
		free(*voidPP);
		*voidPP = NULL;
	}
}

/************************************/
/********** ARRAYS (1D & 2D) ********/
/************************************/
Error arrayNew(void **arryPP, U32 elemSz, U32 nElems) {
	if (elemSz <= 0 || nElems < 0 || arryPP == NULL) 
		return E_BAD_ARGS;  /* TODO: replace with reasonable error type */
	if (nElems == 0) 
		*arryPP = NULL;
	else {
		U32 *ptr = (U32*) malloc((elemSz * nElems) + (2 * sizeof(U32)));
		if (ptr == NULL) 
			return E_NO_MEMORY;
		ptr[0] = elemSz;
		ptr[1] = nElems;
		*arryPP = (ptr + 2);
		memset(*arryPP, 0, elemSz * nElems);
	}
	return SUCCESS;
}
	
void arrayDel(void **arryPP) {
	if (arryPP != NULL && *arryPP != NULL) {
		U32 *ptr = *arryPP;
		free((ptr) - 2);
		*arryPP = NULL;
	}
}

U32 arrayGetNElems(const void *arryP) {
	U32 *ptr;
	if (arryP == NULL) 
		return 0;
	else {
		ptr = (U32*) arryP;
		return *(ptr - 1);
	}
}

U32 arrayGetElemSz(const void *arryP) {
	U32 *ptr;
	if (arryP == NULL)
		return 0;
	else {
		ptr = (U32*) arryP;
		return *(ptr - 2);
	}
}

inline static void* _arrayGetElemByIdx(const void *arryP, S32 idx) {
  const U32 nElems = arrayGetNElems(arryP);
  /* If idx < 0, return void pointer past end of array. */
  if (idx < 0) 
    return (void*) (((U8*) arryP) + (nElems * arrayGetElemSz(arryP)));
  /* If idx is valid, return void pointer to indexed element. */
  else if ((U32) idx < nElems)
    return (void*) ((U8*) arryP + (idx * arrayGetElemSz(arryP)));
  /* Index is invalid. */
  else
    return NULL;  
}

/* Also provide an external copy of above function. */
void* arrayGetVoidElemPtr(const void *arryP, S32 idx) {
  const U32 nElems = arrayGetNElems(arryP);
  /* If idx < 0, return void pointer past end of array. */
  if (idx < 0) 
    return (void*) (((U8*) arryP) + (nElems * arrayGetElemSz(arryP)));
  /* If idx is valid, return void pointer to indexed element. */
  else if ((U32) idx < nElems)
    return (void*) ((U8*) arryP + (idx * arrayGetElemSz(arryP)));
  /* Index is invalid. */
  else
    return NULL;  
}

Error arraySetVoidElem(void *arrayP, U32 idx, const void *elemSrcompP) {
	if (!arrayP)
		return E_BAD_ARGS;
	U32 elemSz = arrayGetElemSz((const void*) arrayP);
	void *dstP = (U8*) arrayP + (idx * elemSz);
	memcpy(dstP, elemSrcompP, elemSz);
	return SUCCESS;
}

void arrayIniPtrs(const void *arryP, void **startP, void **endP, S32 endIdx) {
	*startP = (void*) arryP;
	*endP = _arrayGetElemByIdx(arryP, endIdx);
}

inline static U32 _fastArrayGetElemSz(const void *arryP) {
	return *(((U32*)arryP) - 2);
}
inline static void* _fastArrayGetElemByIdx(const void *arryP, U32 idx) {
	return (void*) ((U8*) arryP + (idx * _fastArrayGetElemSz(arryP)));
}

/***********************/
/********* MAPS ********/
/***********************/
Error mapNew(Map **mapPP, const U8 elemSz, const Key nElems) {
	if (elemSz == 0 || nElems == 0) {
    return E_BAD_ARGS;
  }
  Error e = jbAlloc((void**) mapPP, sizeof(Map), 1);
	if (!e)
		e = arrayNew(&(*mapPP)->mapA, elemSz, nElems);
  if (!e) 
    memset((*mapPP)->flagA, 0, sizeof(FlagInfo) * N_FLAG_BYTES);
  else {
    arrayDel((*mapPP)->mapA);
    jbFree((void**)mapPP);
  }
	return e;
}

void mapDel(Map **mapPP) {
	if (mapPP != NULL && *mapPP != NULL) {
		arrayDel(&(*mapPP)->mapA);
		jbFree((void**) mapPP);
	}
}

inline static U8 _isMapValid(const Map *mapP) {
	return (mapP != NULL && mapP->mapA != NULL); 
}	

inline static U8 _isKeyValid(const Key key) {
  return (key > 0);
}

/* Map GETTING functions */
inline static U8 _countBits(Key bitfield) {
	register Key count = bitfield - ((bitfield >> 1) & 0x55555555);
	count = (count & 0x33333333) + ((count >> 2) & 0x33333333);
	count = (count + (count >> 4)) & 0x0f0f0f0f;
	return (count * 0x01010101) >> 24;
}
inline static FlagInfo _getFlagInfo(const Map *mapP, const Key key) {
  return mapP->flagA[byteIdx_(key)];
}

#if 0
inline static U8 _isFlagSet(const U8 flags, const Key key) {
	return flags & (1 << ((key - 1) & 0x07));
}
#endif

inline static U32 _getElemIdx(const FlagInfo f, const Key key) {
	return f.prevBitCount + _countBits(f.flags & (bitFlag_(key) - 1));
}

Error mapGetIndex(const Map *mapP, const Key key, Key *idxP) {
	const register Key keyMinus1 = key - 1;
	const register FlagInfo f = mapP->flagA[keyMinus1 >> 3];  // Divide N by 8 for byte with Nth bit.
	const register Key bitFlag = 1 << (keyMinus1 & 0x07);     // 0x07 keeps bit inside 8-bit bounds.
	if (f.flags & bitFlag) {
    *idxP = _getElemIdx(f, key);
    return SUCCESS;
 }
  return E_BAD_KEY;
}

inline static void* _getElemP(const Map *mapP, const FlagInfo f, const Key key) {
	return _fastArrayGetElemByIdx(mapP->mapA, _getElemIdx(f, key));
}	

inline static U32 _getMapElemSz(const Map *mapP) {
  return arrayGetElemSz(mapP->mapA);
}

inline static U32 _getNBitsSet(const Map *mapP) {
  return mapP->flagA[LAST_FLAG_BYTE_IDX].prevBitCount + _countBits(mapP->flagA[LAST_FLAG_BYTE_IDX].flags);
}

void* mapGet(const Map *mapP, const Key key) {
	const register U32 keyMinus1 = key - 1;
	const register FlagInfo f = mapP->flagA[keyMinus1 >> 3];
	const register U32 bitFlag = 1 << (keyMinus1 & 0x07);
	// If the bit flag in question is set, that means a value exists for the input key.
	if (f.flags & bitFlag) {
		// Count all the bits set BEFORE the key'th bit (first bit is 1) to get the array index.
		register U32 count = f.flags & (bitFlag - 1);
		count = count - ((count >> 1) & 0x55555555);
		count = (count & 0x33333333) + ((count >> 2) & 0x33333333);
		count = (count + (count >> 4)) & 0x0f0f0f0f;
		count = (count * 0x01010101) >> 24;
		return _fastArrayGetElemByIdx(mapP->mapA, count + f.prevBitCount);
	}
	return NULL;
}

/* Map SETTING functinos */
/* If any bits exist to the left of the key's bit, array elements exist in target spot. */
inline static U8 _idxIsPopulated(const U32 nBitsSet, U32 idx) {
  return (idx < nBitsSet);
}

static Error preMapSet(const Map *mapP, const Key key, void **elemPP, void **nextElemPP, U32 *nBytesTMoveP) {
  *nBytesTMoveP = 0;
  if (_isMapValid(mapP) && _isKeyValid(key)) {
    FlagInfo f;
    f = _getFlagInfo(mapP, key);
    *elemPP = _getElemP(mapP, f, key);
	  if (*elemPP) {  /* Side-stepping mapGet() to avoid NULL pointers and double-calling _isMapValid() */
      U32 nBitsSet = _getNBitsSet(mapP);
			U32 keyElemIdx = _getElemIdx(f, key);
      /* If something's already in the target index, move everything over one. */
      if (_idxIsPopulated(nBitsSet, keyElemIdx)) {
        U32 mapElemSz = _getMapElemSz(mapP);
        *nBytesTMoveP = (nBitsSet - keyElemIdx) * mapElemSz;
        *nextElemPP = (U8*) *elemPP + mapElemSz;
      }
      return SUCCESS;
    } else {
      return E_BAD_KEY;
    }
  } else 
    return E_BAD_ARGS;
}

Error mapSet(Map *mapP, const Key key, const void *valP) {
	void *elemP, *nextElemP;
  U32 nBytesToMove;
  Error e = preMapSet(mapP, key, &elemP, &nextElemP, &nBytesToMove);
  if (!e) {
    if (nBytesToMove) 
      memcpy(nextElemP, (const void*) elemP, nBytesToMove);
		/* Write value to map element. */
		memcpy(elemP, valP, _getMapElemSz(mapP));
		/* Set flag. */
		Key byteIdx = byteIdx_(key);
		mapP->flagA[byteIdx].flags |= bitFlag_(key);  /* flagNum & 0x07 gives you # of bits in the Nth byte */
		/* Increment all prevBitCounts in bytes above affected one. */
		while (++byteIdx < N_FLAG_BYTES) 
		  ++mapP->flagA[byteIdx].prevBitCount;
	}
	return e;
}

Error mapRem(Map *mapP, const Key key) {
	void *elemP, *nextElemP;
  U32 nBytesToMove;
  Error e = preMapSet(mapP, key, &elemP, &nextElemP, &nBytesToMove);
  if (!e) {
    if (nBytesToMove) 
      memcpy(elemP, (const void*) nextElemP, nBytesToMove);
		/* Unset flag. */
		U8 byteIdx = byteIdx_(key);
		mapP->flagA[byteIdx].flags &= ~bitFlag_(key);  /* key's bit position byteIdx'th byte */
		/* Increment all prevBitCounts in bytes above affected one. */
		while (++byteIdx < N_FLAG_BYTES) 
			--mapP->flagA[byteIdx].prevBitCount;
	}
	return e;
}

/************************************/
/************ HISTOGRAMS ************/
/************************************/

Error histoNew(U32 **histoPP, const U32 maxVal) {
	if (histoPP == NULL)
		return E_BAD_ARGS;
	return arrayNew((void**) histoPP, sizeof(U32), maxVal);
}	

void histoDel(U32 **histoPP) {
	arrayDel((void**) histoPP);
}

/*************************************/
/******** TINFL DECOMPRESSION ********/
/*************************************/

/* tinfl.c v1.11 - public domain inflate with zlib header parsing/adler32 checking (inflate-only subset of miniz.c)
   See "unlicense" statement at the end of this file.
   Rich Geldreich <richgel99@gmail.com>, last updated May 20, 2011
   Implements RFC 1950: http://www.ietf.org/rfc/rfc1950.txt and RFC 1951: http://www.ietf.org/rfc/rfc1951.txt

   The entire decompressor coroutine is implemented in tinflDecompress(). The other functions are optional high-depth helpers.
*/
#ifndef TINFL_HEADER_INCLUDED
#define TINFL_HEADER_INCLUDED

typedef unsigned char mzUint8;
typedef signed short mzInt16;
typedef unsigned short mzUint16;
typedef unsigned int mzUint32;
typedef unsigned int mzUint;
typedef unsigned long long mzUint64;

#if defined(_M_IX86) || defined(_M_X64)
// Set MINIZ_USE_UNALIGNED_LOADS_AND_STORES to 1 if integer loads and stores to unaligned addresses are acceptable on the target platform (slightly faster).
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 1
// Set MINIZ_LITTLE_ENDIAN to 1 if the processor is little endian.
#define MINIZ_LITTLE_ENDIAN 1
#endif

#if defined(_WIN64) || defined(__MINGW64__) || defined(_LP64) || defined(__LP64__)
// Set MINIZ_HAS_64BIT_REGISTERS to 1 if the processor has 64-bit general purpose registers (enables 64-bit bitbuffer in inflator)
#define MINIZ_HAS_64BIT_REGISTERS 1
#endif

// Works around MSVC's spammy "warning C4127: conditional expression is constant" message.
#ifdef _MSC_VER
  #define MZ_MACRO_END while (0, 0)
#else
  #define MZ_MACRO_END while (0)
#endif

// Decompression flags used by tinflDecompress().
// TINFL_FLAG_PARSE_ZLIB_HEADER: If set, the input has a valid zlib header and ends with an adler32 checksum (it's a valid zlib stream). Otherwise, the input is a raw deflate stream.
// TINFL_FLAG_HAS_MORE_INPUT: If set, there are more input bytes available beyond the end of the supplied input buffer. If clear, the input buffer contains all remaining input.
// TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF: If set, the output buffer is large enough to hold the entire decompressed stream. If clear, the output buffer is at least the size of the dictionary (typically 32KB).
// TINFL_FLAG_COMPUTE_ADLER32: Force adler-32 checksum computation of the decompressed bytes.
enum
{
  TINFL_FLAG_PARSE_ZLIB_HEADER = 1,
  TINFL_FLAG_HAS_MORE_INPUT = 2,
  TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF = 4,
  TINFL_FLAG_COMPUTE_ADLER32 = 8
};

// High depth decompression functions:
// tinflDecompressMemToHeap() decompresses a block in memory to a heap block allocated via malloc().
// On entry:
//  pSrcBuf, srcBufLen: Pointer and size of the Deflate or zlib source data to decompress.
// On return:
//  Function returns a pointer to the decompressed data, or NULL on failure.
//  *pOutLen will be set to the decompressed data's size, which could be larger than srcBufLen on uncompressible data.
//  The caller must free() the returned block when it's no longer needed.
void *tinflDecompressMemToHeap(const void *pSrcBuf, size_t srcBufLen, size_t *pOutLen, int flags);

// tinflDecompressMemToMem() decompresses a block in memory to another block in memory.
// Returns TINFL_DECOMPRESS_MEM_TO_MEM_FAILED on failure, or the number of bytes written on success.
#define TINFL_DECOMPRESS_MEM_TO_MEM_FAILED ((size_t)(-1))
size_t tinflDecompressMemToMem(void *pOutBuf, size_t outBufLen, const void *pSrcBuf, size_t srcBufLen, int flags);

// tinflDecompressMemToCallback() decompresses a block in memory to an internal 32KB buffer, and a user provided callback function will be called to flush the buffer.
// Returns 1 on success or 0 on failure.
typedef int (*tinflPutBufFuncPtr)(const void* pBuf, int len, void *pUser);
int tinflDecompressMemToCallback(const void *pInBuf, size_t *pInBufSize, tinflPutBufFuncPtr pPutBufFunc, void *pPutBufUser, int flags);

struct tinflDecompressorTag; typedef struct tinflDecompressorTag tinflDecompressor;

// Max size of LZ dictionary.
#define TINFL_LZ_DICT_SIZE 32768

// Return status.
typedef enum
{
  TINFL_STATUS_BAD_PARAM = -3,
  TINFL_STATUS_ADLER32_MISMATCH = -2,
  TINFL_STATUS_FAILED = -1,
  TINFL_STATUS_DONE = 0,
  TINFL_STATUS_NEEDS_MORE_INPUT = 1,
  TINFL_STATUS_HAS_MORE_OUTPUT = 2
} tinflStatus;

// Initializes the decompressor to its initial state.
#define tinflIni(r) do { (r)->mState = 0; } MZ_MACRO_END
#define tinflGetAdler32(r) (r)->mCheckAdler32

// Main low-depth decompressor coroutine function. This is the only function actually needed for decompression. All the other functions are just high-depth helpers for improved usability.
// This is a universal API, i.e. it can be used as a building block to build any desired higher depth decompression API. In the limit case, it can be called once per every byte input or output.
tinflStatus tinflDecompress(tinflDecompressor *r, const mzUint8 *pInBufNext, size_t *pInBufSize, mzUint8 *pOutBufStart, mzUint8 *pOutBufNext, size_t *pOutBufSize, const mzUint32 decompFlags);

// Internal/private bits follow.
enum
{
  TINFL_MAX_HUFF_TABLES = 3, TINFL_MAX_HUFF_SYMBOLS_0 = 288, TINFL_MAX_HUFF_SYMBOLS_1 = 32, TINFL_MAX_HUFF_SYMBOLS_2 = 19,
  TINFL_FAST_LOOKUP_BITS = 10, TINFL_FAST_LOOKUP_SIZE = 1 << TINFL_FAST_LOOKUP_BITS
};

typedef struct
{
  mzUint8 mCodeSize[TINFL_MAX_HUFF_SYMBOLS_0];
  mzInt16 mLookUp[TINFL_FAST_LOOKUP_SIZE], mTree[TINFL_MAX_HUFF_SYMBOLS_0 * 2];
} tinflHuffTable;

#if MINIZ_HAS_64BIT_REGISTERS
  #define TINFL_USE_64BIT_BITBUF 1
#endif

#if TINFL_USE_64BIT_BITBUF
  typedef mzUint64 tinflBitBufT;
  #define TINFL_BITBUF_SIZE (64)
#else
  typedef mzUint32 tinflBitBufT;
  #define TINFL_BITBUF_SIZE (32)
#endif

struct tinflDecompressorTag
{
  mzUint32 mState, mN_bits, mZhdr0, mZhdr1, mZ_adler32, mFinal, mType, mCheckAdler32, mDist, mCounter, mNumExtra, mTableSizes[TINFL_MAX_HUFF_TABLES];
  tinflBitBufT mBitBuf;
  size_t mDistFromOutBufStart;
  tinflHuffTable mTables[TINFL_MAX_HUFF_TABLES];
  mzUint8 mRawHeader[4], mLenCodes[TINFL_MAX_HUFF_SYMBOLS_0 + TINFL_MAX_HUFF_SYMBOLS_1 + 137];
};

#endif // #ifdef TINFL_HEADER_INCLUDED

// ------------------- End of Header: Implementation follows. (If you only want the header, define MINIZ_HEADER_FILE_ONLY.)

#ifndef TINFL_HEADER_FILE_ONLY

#include <string.h>

// MZ_MALLOC, etc. are only used by the optional high-depth helper functions.
#ifdef MINIZ_NO_MALLOC
  #define MZ_MALLOC(x) NULL
  #define MZ_FREE(x) x, ((void)0)
  #define MZ_REALLOC(p, x) NULL
#else
  #define MZ_MALLOC(x) malloc(x)
  #define MZ_FREE(x) free(x)
  #define MZ_REALLOC(p, x) realloc(p, x)
#endif

#define MZ_MAX(a,b) (((a)>(b))?(a):(b))
#define MZ_MIN(a,b) (((a)<(b))?(a):(b))
#define MZ_CLEAR_OBJ(obj) memset(&(obj), 0, sizeof(obj))

#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN
  #define MZ_READ_LE16(p) *((const mzUint16 *)(p))
  #define MZ_READ_LE32(p) *((const mzUint32 *)(p))
#else
  #define MZ_READ_LE16(p) ((mzUint32)(((const mzUint8 *)(p))[0]) | ((mzUint32)(((const mzUint8 *)(p))[1]) << 8U))
  #define MZ_READ_LE32(p) ((mzUint32)(((const mzUint8 *)(p))[0]) | ((mzUint32)(((const mzUint8 *)(p))[1]) << 8U) | ((mzUint32)(((const mzUint8 *)(p))[2]) << 16U) | ((mzUint32)(((const mzUint8 *)(p))[3]) << 24U))
#endif

#define TINFL_MEMCPY(d, s, l) memcpy(d, s, l)
#define TINFL_MEMSET(p, c, l) memset(p, c, l)

#define TINFL_CR_BEGIN switch(r->mState) { case 0:
#define TINFL_CR_RETURN(stateIndex, result) do { status = result; r->mState = stateIndex; goto commonExit; case stateIndex:; } MZ_MACRO_END
#define TINFL_CR_RETURN_FOREVER(stateIndex, result) do { for ( ; ; ) { TINFL_CR_RETURN(stateIndex, result); } } MZ_MACRO_END
#define TINFL_CR_FINISH }

// TODO: If the caller has indicated that there's no more input, and we attempt to read beyond the input buf, then something is wrong with the input because the inflator never
// reads ahead more than it needs to. Currently TINFL_GET_BYTE() pads the end of the stream with 0's in this scenario.
#define TINFL_GET_BYTE(stateIndex, c) do { \
  if (pInBufCur >= pInBufEnd) { \
    for ( ; ; ) { \
      if (decompFlags & TINFL_FLAG_HAS_MORE_INPUT) { \
        TINFL_CR_RETURN(stateIndex, TINFL_STATUS_NEEDS_MORE_INPUT); \
        if (pInBufCur < pInBufEnd) { \
          c = *pInBufCur++; \
          break; \
        } \
      } else { \
        c = 0; \
        break; \
      } \
    } \
  } else c = *pInBufCur++; } MZ_MACRO_END

#define TINFL_NEED_BITS(stateIndex, n) do { mzUint c; TINFL_GET_BYTE(stateIndex, c); bitBuf |= (((tinflBitBufT)c) << numBits); numBits += 8; } while (numBits < (mzUint)(n))
#define TINFL_SKIP_BITS(stateIndex, n) do { if (numBits < (mzUint)(n)) { TINFL_NEED_BITS(stateIndex, n); } bitBuf >>= (n); numBits -= (n); } MZ_MACRO_END
#define TINFL_GET_BITS(stateIndex, b, n) do { if (numBits < (mzUint)(n)) { TINFL_NEED_BITS(stateIndex, n); } b = bitBuf & ((1 << (n)) - 1); bitBuf >>= (n); numBits -= (n); } MZ_MACRO_END

// TINFL_HUFF_BITBUF_FILL() is only used rarely, when the number of bytes remaining in the input buffer falls below 2.
// It reads just enough bytes from the input stream that are needed to decode the next Huffman code (and absolutely no more). It works by trying to fully decode a
// Huffman code by using whatever bits are currently present in the bit buffer. If this fails, it reads another byte, and tries again until it succeeds or until the
// bit buffer contains >=15 bits (deflate's max. Huffman code size).
#define TINFL_HUFF_BITBUF_FILL(stateIndex, pHuff) \
  do { \
    temp = (pHuff)->mLookUp[bitBuf & (TINFL_FAST_LOOKUP_SIZE - 1)]; \
    if (temp >= 0) { \
      codeLen = temp >> 9; \
      if ((codeLen) && (numBits >= codeLen)) \
      break; \
    } else if (numBits > TINFL_FAST_LOOKUP_BITS) { \
       codeLen = TINFL_FAST_LOOKUP_BITS; \
       do { \
          temp = (pHuff)->mTree[~temp + ((bitBuf >> codeLen++) & 1)]; \
       } while ((temp < 0) && (numBits >= (codeLen + 1))); if (temp >= 0) break; \
    } TINFL_GET_BYTE(stateIndex, c); bitBuf |= (((tinflBitBufT)c) << numBits); numBits += 8; \
  } while (numBits < 15);

// TINFL_HUFF_DECODE() decodes the next Huffman coded symbol. It's more complex than you would initially expect because the zlib API expects the decompressor to never read
// beyond the final byte of the deflate stream. (In other words, when this macro wants to read another byte from the input, it REALLY needs another byte in order to fully
// decode the next Huffman code.) Handling this properly is particularly important on raw deflate (non-zlib) streams, which aren't followed by a byte aligned adler-32.
// The slow path is only executed at the very end of the input buffer.
#define TINFL_HUFF_DECODE(stateIndex, sym, pHuff) do { \
  int temp; mzUint codeLen, c; \
  if (numBits < 15) { \
    if ((pInBufEnd - pInBufCur) < 2) { \
       TINFL_HUFF_BITBUF_FILL(stateIndex, pHuff); \
    } else { \
       bitBuf |= (((tinflBitBufT)pInBufCur[0]) << numBits) | (((tinflBitBufT)pInBufCur[1]) << (numBits + 8)); pInBufCur += 2; numBits += 16; \
    } \
  } \
  if ((temp = (pHuff)->mLookUp[bitBuf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0) \
    codeLen = temp >> 9, temp &= 511; \
  else { \
    codeLen = TINFL_FAST_LOOKUP_BITS; do { temp = (pHuff)->mTree[~temp + ((bitBuf >> codeLen++) & 1)]; } while (temp < 0); \
  } sym = temp; bitBuf >>= codeLen; numBits -= codeLen; } MZ_MACRO_END

tinflStatus tinflDecompress(tinflDecompressor *r, const mzUint8 *pInBufNext, size_t *pInBufSize, mzUint8 *pOutBufStart, mzUint8 *pOutBufNext, size_t *pOutBufSize, const mzUint32 decompFlags)
{
  static const int sLengthBase[31] = { 3,4,5,6,7,8,9,10,11,13, 15,17,19,23,27,31,35,43,51,59, 67,83,99,115,131,163,195,227,258,0,0 };
  static const int sLengthExtra[31]= { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0,0,0 };
  static const int sDistBase[32] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193, 257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,0,0};
  static const int sDistExtra[32] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
  static const mzUint8 sLengthDezigzag[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
  static const int sMinTableSizes[3] = { 257, 1, 4 };

  tinflStatus status = TINFL_STATUS_FAILED; mzUint32 numBits, dist, counter, numExtra; tinflBitBufT bitBuf;
  const mzUint8 *pInBufCur = pInBufNext, *const pInBufEnd = pInBufNext + *pInBufSize;
  mzUint8 *pOutBufCur = pOutBufNext, *const pOutBufEnd = pOutBufNext + *pOutBufSize;
  size_t outBufSizeMask = (decompFlags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF) ? (size_t)-1 : ((pOutBufNext - pOutBufStart) + *pOutBufSize) - 1, distFromOutBufStart;

  // Ensure the output buffer's size is a power of 2, unless the output buffer is large enough to hold the entire output file (in which case it doesn't matter).
  if (((outBufSizeMask + 1) & outBufSizeMask) || (pOutBufNext < pOutBufStart)) { *pInBufSize = *pOutBufSize = 0; return TINFL_STATUS_BAD_PARAM; }

  numBits = r->mN_bits; bitBuf = r->mBitBuf; dist = r->mDist; counter = r->mCounter; numExtra = r->mNumExtra; distFromOutBufStart = r->mDistFromOutBufStart;
  TINFL_CR_BEGIN

  bitBuf = numBits = dist = counter = numExtra = r->mZhdr0 = r->mZhdr1 = 0; r->mZ_adler32 = r->mCheckAdler32 = 1;
  if (decompFlags & TINFL_FLAG_PARSE_ZLIB_HEADER)
  {
    TINFL_GET_BYTE(1, r->mZhdr0); TINFL_GET_BYTE(2, r->mZhdr1);
    counter = (((r->mZhdr0 * 256 + r->mZhdr1) % 31 != 0) || (r->mZhdr1 & 32) || ((r->mZhdr0 & 15) != 8));
    if (!(decompFlags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF)) counter |= (((1U << (8U + (r->mZhdr0 >> 4))) > 32768U) || ((outBufSizeMask + 1) < (size_t)(1U << (8U + (r->mZhdr0 >> 4)))));
    if (counter) { TINFL_CR_RETURN_FOREVER(36, TINFL_STATUS_FAILED); }
  }

  do
  {
    TINFL_GET_BITS(3, r->mFinal, 3); r->mType = r->mFinal >> 1;
    if (r->mType == 0)
    {
      TINFL_SKIP_BITS(5, numBits & 7);
      for (counter = 0; counter < 4; ++counter) { if (numBits) TINFL_GET_BITS(6, r->mRawHeader[counter], 8); else TINFL_GET_BYTE(7, r->mRawHeader[counter]); }
      if ((counter = (r->mRawHeader[0] | (r->mRawHeader[1] << 8))) != (mzUint)(0xFFFF ^ (r->mRawHeader[2] | (r->mRawHeader[3] << 8)))) { TINFL_CR_RETURN_FOREVER(39, TINFL_STATUS_FAILED); }
      while ((counter) && (numBits))
      {
        TINFL_GET_BITS(51, dist, 8);
        while (pOutBufCur >= pOutBufEnd) { TINFL_CR_RETURN(52, TINFL_STATUS_HAS_MORE_OUTPUT); }
        *pOutBufCur++ = (mzUint8)dist;
        counter--;
      }
      while (counter)
      {
        size_t n; while (pOutBufCur >= pOutBufEnd) { TINFL_CR_RETURN(9, TINFL_STATUS_HAS_MORE_OUTPUT); }
        while (pInBufCur >= pInBufEnd)
        {
          if (decompFlags & TINFL_FLAG_HAS_MORE_INPUT)
          {
            TINFL_CR_RETURN(38, TINFL_STATUS_NEEDS_MORE_INPUT);
          }
          else
          {
            TINFL_CR_RETURN_FOREVER(40, TINFL_STATUS_FAILED);
          }
        }
        n = MZ_MIN(MZ_MIN((size_t)(pOutBufEnd - pOutBufCur), (size_t)(pInBufEnd - pInBufCur)), counter);
        TINFL_MEMCPY(pOutBufCur, pInBufCur, n); pInBufCur += n; pOutBufCur += n; counter -= (mzUint)n;
      }
    }
    else if (r->mType == 3)
    {
      TINFL_CR_RETURN_FOREVER(10, TINFL_STATUS_FAILED);
    }
    else
    {
      if (r->mType == 1)
      {
        mzUint8 *p = r->mTables[0].mCodeSize; mzUint i;
        r->mTableSizes[0] = 288; r->mTableSizes[1] = 32; TINFL_MEMSET(r->mTables[1].mCodeSize, 5, 32);
        for ( i = 0; i <= 143; ++i) *p++ = 8; 
				for ( ; i <= 255; ++i) *p++ = 9; 
				for ( ; i <= 279; ++i) *p++ = 7; 
				for ( ; i <= 287; ++i) *p++ = 8;
      }
      else
      {
        for (counter = 0; counter < 3; counter++) { TINFL_GET_BITS(11, r->mTableSizes[counter], "\05\05\04"[counter]); r->mTableSizes[counter] += sMinTableSizes[counter]; }
        MZ_CLEAR_OBJ(r->mTables[2].mCodeSize); 
				for (counter = 0; counter < r->mTableSizes[2]; counter++) { mzUint s; TINFL_GET_BITS(14, s, 3); r->mTables[2].mCodeSize[sLengthDezigzag[counter]] = (mzUint8)s; }
        r->mTableSizes[2] = 19;
      }
      for ( ; (int)r->mType >= 0; r->mType--)
      {
        int treeNext, treeCur; tinflHuffTable *pTable;
        mzUint i, j, usedSyms, total, symIndex, nextCode[17], totalSyms[16]; pTable = &r->mTables[r->mType]; MZ_CLEAR_OBJ(totalSyms); MZ_CLEAR_OBJ(pTable->mLookUp); MZ_CLEAR_OBJ(pTable->mTree);
        for (i = 0; i < r->mTableSizes[r->mType]; ++i) totalSyms[pTable->mCodeSize[i]]++;
        usedSyms = 0, total = 0; nextCode[0] = nextCode[1] = 0;
        for (i = 1; i <= 15; ++i) { usedSyms += totalSyms[i]; nextCode[i + 1] = (total = ((total + totalSyms[i]) << 1)); }
        if ((65536 != total) && (usedSyms > 1))
        {
          TINFL_CR_RETURN_FOREVER(35, TINFL_STATUS_FAILED);
        }
        for (treeNext = -1, symIndex = 0; symIndex < r->mTableSizes[r->mType]; ++symIndex)
        {
          mzUint revCode = 0, l, curCode, codeSize = pTable->mCodeSize[symIndex]; if (!codeSize) continue;
          curCode = nextCode[codeSize]++; 
					for (l = codeSize; l > 0; l--, curCode >>= 1) revCode = (revCode << 1) | (curCode & 1);
          if (codeSize <= TINFL_FAST_LOOKUP_BITS) { mzInt16 k = (mzInt16)((codeSize << 9) | symIndex); while (revCode < TINFL_FAST_LOOKUP_SIZE) { pTable->mLookUp[revCode] = k; revCode += (1 << codeSize); } continue; }
          if (0 == (treeCur = pTable->mLookUp[revCode & (TINFL_FAST_LOOKUP_SIZE - 1)])) { pTable->mLookUp[revCode & (TINFL_FAST_LOOKUP_SIZE - 1)] = (mzInt16)treeNext; treeCur = treeNext; treeNext -= 2; }
          revCode >>= (TINFL_FAST_LOOKUP_BITS - 1);
          for (j = codeSize; j > (TINFL_FAST_LOOKUP_BITS + 1); j--)
          {
            treeCur -= ((revCode >>= 1) & 1);
            if (!pTable->mTree[-treeCur - 1]) { pTable->mTree[-treeCur - 1] = (mzInt16)treeNext; treeCur = treeNext; treeNext -= 2; } else treeCur = pTable->mTree[-treeCur - 1];
          }
          treeCur -= ((revCode >>= 1) & 1); pTable->mTree[-treeCur - 1] = (mzInt16)symIndex;
        }
        if (r->mType == 2)
        {
          for (counter = 0; counter < (r->mTableSizes[0] + r->mTableSizes[1]); )
          {
            mzUint s; TINFL_HUFF_DECODE(16, dist, &r->mTables[2]); if (dist < 16) { r->mLenCodes[counter++] = (mzUint8)dist; continue; }
            if ((dist == 16) && (!counter))
            {
              TINFL_CR_RETURN_FOREVER(17, TINFL_STATUS_FAILED);
            }
            numExtra = "\02\03\07"[dist - 16]; TINFL_GET_BITS(18, s, numExtra); s += "\03\03\013"[dist - 16];
            TINFL_MEMSET(r->mLenCodes + counter, (dist == 16) ? r->mLenCodes[counter - 1] : 0, s); counter += s;
          }
          if ((r->mTableSizes[0] + r->mTableSizes[1]) != counter)
          {
            TINFL_CR_RETURN_FOREVER(21, TINFL_STATUS_FAILED);
          }
          TINFL_MEMCPY(r->mTables[0].mCodeSize, r->mLenCodes, r->mTableSizes[0]); TINFL_MEMCPY(r->mTables[1].mCodeSize, r->mLenCodes + r->mTableSizes[0], r->mTableSizes[1]);
        }
      }
      for ( ; ; )
      {
        mzUint8 *pSrc;
        for ( ; ; )
        {
          if (((pInBufEnd - pInBufCur) < 4) || ((pOutBufEnd - pOutBufCur) < 2))
          {
            TINFL_HUFF_DECODE(23, counter, &r->mTables[0]);
            if (counter >= 256)
              break;
            while (pOutBufCur >= pOutBufEnd) { TINFL_CR_RETURN(24, TINFL_STATUS_HAS_MORE_OUTPUT); }
            *pOutBufCur++ = (mzUint8)counter;
          }
          else
          {
            int sym2; mzUint codeLen;
#if TINFL_USE_64BIT_BITBUF
            if (numBits < 30) { bitBuf |= (((tinflBitBufT)MZ_READ_LE32(pInBufCur)) << numBits); pInBufCur += 4; numBits += 32; }
#else
            if (numBits < 15) { bitBuf |= (((tinflBitBufT)MZ_READ_LE16(pInBufCur)) << numBits); pInBufCur += 2; numBits += 16; }
#endif
            if ((sym2 = r->mTables[0].mLookUp[bitBuf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0)
              codeLen = sym2 >> 9;
            else
            {
              codeLen = TINFL_FAST_LOOKUP_BITS; do { sym2 = r->mTables[0].mTree[~sym2 + ((bitBuf >> codeLen++) & 1)]; } while (sym2 < 0);
            }
            counter = sym2; bitBuf >>= codeLen; numBits -= codeLen;
            if (counter & 256)
              break;

#if !TINFL_USE_64BIT_BITBUF
            if (numBits < 15) { bitBuf |= (((tinflBitBufT)MZ_READ_LE16(pInBufCur)) << numBits); pInBufCur += 2; numBits += 16; }
#endif
            if ((sym2 = r->mTables[0].mLookUp[bitBuf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0)
              codeLen = sym2 >> 9;
            else
            {
              codeLen = TINFL_FAST_LOOKUP_BITS; do { sym2 = r->mTables[0].mTree[~sym2 + ((bitBuf >> codeLen++) & 1)]; } while (sym2 < 0);
            }
            bitBuf >>= codeLen; numBits -= codeLen;

            pOutBufCur[0] = (mzUint8)counter;
            if (sym2 & 256)
            {
              pOutBufCur++;
              counter = sym2;
              break;
            }
            pOutBufCur[1] = (mzUint8)sym2;
            pOutBufCur += 2;
          }
        }
        if ((counter &= 511) == 256) break;

        numExtra = sLengthExtra[counter - 257]; counter = sLengthBase[counter - 257];
        if (numExtra) { mzUint extraBits; TINFL_GET_BITS(25, extraBits, numExtra); counter += extraBits; }

        TINFL_HUFF_DECODE(26, dist, &r->mTables[1]);
        numExtra = sDistExtra[dist]; dist = sDistBase[dist];
        if (numExtra) { mzUint extraBits; TINFL_GET_BITS(27, extraBits, numExtra); dist += extraBits; }

        distFromOutBufStart = pOutBufCur - pOutBufStart;
        if ((dist > distFromOutBufStart) && (decompFlags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF))
        {
          TINFL_CR_RETURN_FOREVER(37, TINFL_STATUS_FAILED);
        }

        pSrc = pOutBufStart + ((distFromOutBufStart - dist) & outBufSizeMask);

        if ((MZ_MAX(pOutBufCur, pSrc) + counter) > pOutBufEnd)
        {
          while (counter--)
          {
            while (pOutBufCur >= pOutBufEnd) { TINFL_CR_RETURN(53, TINFL_STATUS_HAS_MORE_OUTPUT); }
            *pOutBufCur++ = pOutBufStart[(distFromOutBufStart++ - dist) & outBufSizeMask];
          }
          continue;
        }
#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES
        else if ((counter >= 9) && (counter <= dist))
        {
          const mzUint8 *pSrcEnd = pSrc + (counter & ~7);
          do
          {
            ((mzUint32 *)pOutBufCur)[0] = ((const mzUint32 *)pSrc)[0];
            ((mzUint32 *)pOutBufCur)[1] = ((const mzUint32 *)pSrc)[1];
            pOutBufCur += 8;
          } while ((pSrc += 8) < pSrcEnd);
          if ((counter &= 7) < 3)
          {
            if (counter)
            {
              pOutBufCur[0] = pSrc[0];
              if (counter > 1)
                pOutBufCur[1] = pSrc[1];
              pOutBufCur += counter;
            }
            continue;
          }
        }
#endif
        do
        {
          pOutBufCur[0] = pSrc[0];
          pOutBufCur[1] = pSrc[1];
          pOutBufCur[2] = pSrc[2];
          pOutBufCur += 3; pSrc += 3;
        } while ((int)(counter -= 3) > 2);
        if ((int)counter > 0)
        {
          pOutBufCur[0] = pSrc[0];
          if ((int)counter > 1)
            pOutBufCur[1] = pSrc[1];
          pOutBufCur += counter;
        }
      }
    }
  } while (!(r->mFinal & 1));
  if (decompFlags & TINFL_FLAG_PARSE_ZLIB_HEADER)
  {
    TINFL_SKIP_BITS(32, numBits & 7); for (counter = 0; counter < 4; ++counter) { mzUint s; if (numBits) TINFL_GET_BITS(41, s, 8); else TINFL_GET_BYTE(42, s); r->mZ_adler32 = (r->mZ_adler32 << 8) | s; }
  }
  TINFL_CR_RETURN_FOREVER(34, TINFL_STATUS_DONE);
  TINFL_CR_FINISH

commonExit:
  r->mN_bits = numBits; r->mBitBuf = bitBuf; r->mDist = dist; r->mCounter = counter; r->mNumExtra = numExtra; r->mDistFromOutBufStart = distFromOutBufStart;
  *pInBufSize = pInBufCur - pInBufNext; *pOutBufSize = pOutBufCur - pOutBufNext;
  if ((decompFlags & (TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_COMPUTE_ADLER32)) && (status >= 0))
  {
    const mzUint8 *ptr = pOutBufNext; size_t bufLen = *pOutBufSize;
    mzUint32 i, s1 = r->mCheckAdler32 & 0xffff, s2 = r->mCheckAdler32 >> 16; size_t blockLen = bufLen % 5552;
    while (bufLen)
    {
      for (i = 0; i + 7 < blockLen; i += 8, ptr += 8)
      {
        s1 += ptr[0], s2 += s1; s1 += ptr[1], s2 += s1; s1 += ptr[2], s2 += s1; s1 += ptr[3], s2 += s1;
        s1 += ptr[4], s2 += s1; s1 += ptr[5], s2 += s1; s1 += ptr[6], s2 += s1; s1 += ptr[7], s2 += s1;
      }
      for ( ; i < blockLen; ++i) s1 += *ptr++, s2 += s1;
      s1 %= 65521U, s2 %= 65521U; bufLen -= blockLen; blockLen = 5552;
    }
    r->mCheckAdler32 = (s2 << 16) + s1; 
		if ((status == TINFL_STATUS_DONE) && (decompFlags & TINFL_FLAG_PARSE_ZLIB_HEADER) && (r->mCheckAdler32 != r->mZ_adler32)) {
	 	 status = TINFL_STATUS_ADLER32_MISMATCH;
		}
  }
  return status;
}

// Higher depth helper functions.
void *tinflDecompressMemToHeap(const void *pSrcBuf, size_t srcBufLen, size_t *pOutLen, int flags) {
  tinflDecompressor decomp; void *pBuf = NULL, *pNewBuf; size_t srcBufOfs = 0, outBufCapacity = 0;
  *pOutLen = 0;
  tinflIni(&decomp);
  tinflStatus status;
  for (;;) {
    size_t srcBufSize = srcBufLen - srcBufOfs, dstBufSize = outBufCapacity - *pOutLen, newOutBufCapacity;
    status = tinflDecompress(&decomp, (const mzUint8*)pSrcBuf + srcBufOfs, &srcBufSize, (mzUint8*)pBuf, pBuf ? (mzUint8*)pBuf + *pOutLen : NULL, &dstBufSize,
      (flags & ~TINFL_FLAG_HAS_MORE_INPUT) | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);

    if ((status < 0) || (status == TINFL_STATUS_NEEDS_MORE_INPUT)) {
      MZ_FREE(pBuf); *pOutLen = 0; return NULL;
		}

    srcBufOfs += srcBufSize;
    *pOutLen += dstBufSize;

    if (status == TINFL_STATUS_DONE) 
			break;

    newOutBufCapacity = outBufCapacity * 2; if (newOutBufCapacity < 128) newOutBufCapacity = 128;
    pNewBuf = MZ_REALLOC(pBuf, newOutBufCapacity);
		
    if (!pNewBuf) {
      MZ_FREE(pBuf); *pOutLen = 0; return NULL;
		}

    pBuf = pNewBuf; outBufCapacity = newOutBufCapacity;
  }
  
  return pBuf;
}



#endif // #ifndef TINFL_HEADER_FILE_ONLY

/*
  This is free and unencumbered software released into the public domain.

  Anyone is free to copy, modify, publish, use, compile, sell, or
  distribute this software, either in source code form or as a compiled
  binary, for any purpose, commercial or non-commercial, and by any
  means.

  In jurisdictions that recognize copyright laws, the author or authors
  of this software dedicate any and all copyright interest in the
  software to the public domain. We make this dedication for the benefit
  of the public at large and to the detriment of our heirs and
  successors. We intend this dedication to be an overt act of
  relinquishment in perpetuity of all present and future rights to this
  software under copyright law.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.

  For more information, please refer to <http://unlicense.org/>
*/

Error inflate(Inflatable *inflatableP) {
	Error e = SUCCESS;
	long long unsigned int expectedInflatedLen;
	if (inflatableP != NULL && inflatableP->inflatedDataP == NULL) {
		
		e = jbAlloc(&inflatableP->inflatedDataP, inflatableP->inflatedLen, 1);
		if (!e) {
			expectedInflatedLen = inflatableP->inflatedLen;
			inflatableP->inflatedDataP = tinflDecompressMemToHeap((const void*) inflatableP->compressedData, 
					                                                    (size_t) inflatableP->compressedLen, 
																															&inflatableP->inflatedLen,
																														 	TINFL_FLAG_PARSE_ZLIB_HEADER); 
			if (inflatableP->inflatedLen != expectedInflatedLen) {
				e = E_UNEXPECTED_DCMP_SZ;
				jbFree(&inflatableP->inflatedDataP);
			}
		}
	}
  return e;
}

void deflate(Inflatable **inflatablePP) {
  if (inflatablePP && *inflatablePP && (*inflatablePP)->inflatedDataP)
    jbFree(&(*inflatablePP)->inflatedDataP);
}


// Efficient Arrays (frays)
#define N_PREFRAY_ELEMS (5)
#define OFFSET_INACTIVE     (N_PREFRAY_ELEMS)
#define OFFSET_N_PAUSED   (N_PREFRAY_ELEMS - 1)  // TODO avoid updates by changing to N_PAUSED
#define OFFSET_1ST_EMPTY    (N_PREFRAY_ELEMS - 2)
#define OFFSET_ELEM_SZ      (N_PREFRAY_ELEMS - 3)
#define OFFSET_N_ELEMS      (N_PREFRAY_ELEMS - 4)
#define frayGetNElems_ arrayGetNElems
#define frayGetElemSz_ arrayGetElemSz
#define frayGetElemByIdx_ _fastArrayGetElemByIdx
Error frayNew(void **fPP, U32 elemSz, U32 nElems) {
	if (elemSz <= 0 || nElems <= 0 || fPP == NULL) 
		return E_BAD_ARGS;  
	else {
    // Add 1 more element for swaps. 
		U32 *ptr = (U32*) malloc((elemSz * nElems) + ((N_PREFRAY_ELEMS + 1) * sizeof(U32)));
		if (ptr == NULL) 
			return E_NO_MEMORY;
		ptr[OFFSET_INACTIVE]   = 0;       
		ptr[OFFSET_N_PAUSED]   = 0;  
		ptr[OFFSET_1ST_EMPTY]  = 0;       
		ptr[OFFSET_ELEM_SZ]    = elemSz;
		ptr[OFFSET_N_ELEMS]    = nElems;
		*fPP = (ptr + N_PREFRAY_ELEMS);
		memset(*fPP, 0, elemSz * nElems);
  }
  return SUCCESS;
}

void frayDel(void **frayPP) {
	if (frayPP != NULL && *frayPP != NULL) {
		U32 *ptr = *frayPP;
		free((ptr) - N_PREFRAY_ELEMS);
		*frayPP = NULL;
	}
}

void frayClr(void *fP) {
  memset(fP, 0, frayGetElemSz_(fP) * arrayGetNElems(fP));
  *frayGetFirstEmptyIdxP(fP) = 0;
}

// Pointers beat values. We usually inc/decrement it after using it. Avoids double-queries.
inline static U32 _frayGetFirstInactiveIdx(const void *frayP) {
  return *(((U32*) frayP - OFFSET_INACTIVE));
}

inline static U32* _frayGetFirstInactiveIdxP(const void *frayP) {
  return ((U32*) frayP - OFFSET_INACTIVE);
}

inline static U32* _frayGetNPausedP(const void *frayP) {
  return ((U32*) frayP - OFFSET_N_PAUSED);
}

inline static U32* _frayGetFirstEmptyIdxP(const void *frayP) {
  return ((U32*) frayP - OFFSET_1ST_EMPTY);
}

// Non-static versions of the above for global use
U32 frayGetFirstInactiveIdx(const void *frayP) {
  return _frayGetFirstInactiveIdx(frayP);
}

U32* frayGetFirstInactiveIdxP(const void *frayP) {
  return _frayGetFirstInactiveIdxP(frayP);
}

U32* frayGetNPausedP(const void *frayP) {
  return _frayGetNPausedP(frayP);
}

inline static U32 _frayGetNPaused(const void *frayP) {
  return *((U32*) frayP - OFFSET_N_PAUSED);
}

U32 frayGetNPaused(const void *frayP) {
  return _frayGetNPaused(frayP);
}

U32 frayGetFirstPausedIdx(const void *frayP) {
  return _frayGetFirstInactiveIdx(frayP) - _frayGetNPaused(frayP);
}

U32* frayGetFirstEmptyIdxP(const void *frayP) {
  return _frayGetFirstEmptyIdxP(frayP);
}

/* Checks if the component, wherever it is in the jagged array, is before the function's stopping point in its array. */
inline static U8 _frayElemIsActive(const void *frayP, U32 idx) {
  return idx < _frayGetFirstInactiveIdx(frayP);
}

U8 frayElemIsActive(const void *frayP, U32 idx) {
  return _frayElemIsActive(frayP, idx);
}

inline static U8 _frayHasRoom(const void *frayP) {
  return (*_frayGetFirstEmptyIdxP(frayP) < frayGetNElems_(frayP));
}

// Returns index of added element
Error frayAdd(const void *frayP, void *elemP, U32 *elemNewIdxP) {
  if (!_frayHasRoom(frayP))
    return E_FRAY_FULL;
  U32 *firstEmptyIdxP = _frayGetFirstEmptyIdxP(frayP);
  void *dstP = frayGetElemByIdx_(frayP, (*firstEmptyIdxP)++);
  memcpy(dstP, elemP, frayGetElemSz_(frayP));
  if (elemNewIdxP)
    *elemNewIdxP = *firstEmptyIdxP - 1;
  return SUCCESS;
}

static void _fraySwap(const void *frayP, U32 oldIdx, U32 newIdx) {
  // Get source, destination, and placeholder
  register void *elem1P       = frayGetElemByIdx_(frayP, oldIdx);
  register void *placeholderP = frayGetElemByIdx_(frayP, frayGetNElems_(frayP));
  register void *elem2P       = frayGetElemByIdx_(frayP, newIdx); 
  // Swap with the first inactive.
  register U32   elemSz       = frayGetElemSz_(frayP);
  memcpy(placeholderP, elem1P,       elemSz);
  memcpy(elem1P,       elem2P,       elemSz);
  memcpy(elem2P,       placeholderP, elemSz);
}

static U8 _frayElemIsPaused(const void *frayP, U32 idx) {
  U32 firstInactiveIdx = frayGetFirstInactiveIdx(frayP);
  return idx < firstInactiveIdx &&
         idx > firstInactiveIdx - _frayGetNPaused(frayP);
}

// Pausing *active* elements moves them to the first paused position.
// Pausing *inactive* elements moves them to the last paused position.
U32 frayPause(const void *frayP, U32 idx) {
  U32 *nPausedP = _frayGetNPausedP(frayP);
  if (!_frayElemIsPaused(frayP, idx) && *nPausedP < arrayGetNElems(frayP)) {
    U32 *firstInactiveIdxP = _frayGetFirstInactiveIdxP(frayP);
    ++(*nPausedP);
    U32 newIdx;
    if (idx < *firstInactiveIdxP) 
      newIdx = *firstInactiveIdxP - *nPausedP;
    else 
      newIdx = (*firstInactiveIdxP)++;
    _fraySwap(frayP, idx, newIdx);
    return newIdx;
  }
  return idx;
}

// Unlike pausing, unpausing can only send elements in one direction: leftward.
U32 frayUnpause(const void *frayP, U32 idx) {
  register U32  *nPausedP = _frayGetNPausedP(frayP);
  U32 firstInactiveIdx =  _frayGetFirstInactiveIdx(frayP);
  if (_frayElemIsPaused(frayP, idx) && *nPausedP < firstInactiveIdx) {
    U32 newIdx = firstInactiveIdx - (*nPausedP)--;
    _fraySwap(frayP, idx, newIdx);
    return newIdx;
  }
  return idx;
}

void frayPauseAll(const void *frayP) {
  *_frayGetNPausedP(frayP) = _frayGetFirstInactiveIdx(frayP);
}

void frayUnpauseAll(const void *frayP) {
  *_frayGetNPausedP(frayP) = 0;
}

// Returns new index of activated element 
U32 frayActivate(const void *frayP, U32 idx) {
  if (!_frayElemIsActive(frayP, idx)) {
    U32 *firstInactiveIdxP = _frayGetFirstInactiveIdxP(frayP);
    U32 newIdx;
    U32 nPaused = _frayGetNPaused(frayP);
    if (!nPaused)  // With no paused elements, we can blissfully single-swap.
      newIdx = (*firstInactiveIdxP)++;            // swap with first inactive (from left side)
    else {  // Otherwise, we must double-swap to preserve intermediate paused elems' contiguity.
      newIdx = *firstInactiveIdxP;                // swap with first inactive (from right side)
      _fraySwap(frayP, idx, newIdx);
      idx = newIdx;                  
      newIdx = (*firstInactiveIdxP)++ - nPaused;  // swap 1st deact. elem with 1st paused
    }
    _fraySwap(frayP, idx, newIdx);
    // Return index of activated element's new position.
    return newIdx;
  }
  return idx;
}

// Returns new index of deactivated element 
U32 frayDeactivate(const void *frayP, U32 idx) {
  if (_frayElemIsActive(frayP, idx)) {
    U32 *firstInactiveIdxP = _frayGetFirstInactiveIdxP(frayP);
    U32 newIdx;
    U32 nPaused = _frayGetNPaused(frayP);
    if (!nPaused)  // With no paused elements, we can blissfully single-swap.
      newIdx = --(*firstInactiveIdxP);            // swap with last active 
    else {  // Otherwise, we must double-swap to preserve intermediate paused elems' contiguity.
      newIdx = *firstInactiveIdxP - nPaused - 1;  // swap with last active
      _fraySwap(frayP, idx, newIdx);
      idx = newIdx;                  
      newIdx = --(*firstInactiveIdxP);  // swap last active with last paused
    }
    _fraySwap(frayP, idx, newIdx);
    // Return index of activated element's new position.
    return newIdx;
  }
  return idx;
}

// Messaging
// There is no corresponding mailboxRead() function because that's specific to each implementer.
Error mailboxWrite(Message *mailboxP, Key address, Key attn, Key cmd, Key arg) {
  Message message = {
    .address = address,
    .attn = attn,
    .cmd = cmd,
    .arg = arg 
  };
  return frayAdd((void*) mailboxP, &message, NULL);
}

Error mailboxForward(Message *mailboxP, Message *msgP) {
  return frayAdd((void*) mailboxP, msgP, NULL);
}

