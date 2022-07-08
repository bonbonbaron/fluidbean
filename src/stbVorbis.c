// Ogg Vorbis audio decoder - v1.20 - public domain
// http://nothings.org/stbVorbis/
//
// Original version written by Sean Barrett in 2007.
//
// Originally sponsored by RAD Game Tools. Seeking implementation
// sponsored by Phillip Bennefall, Marc Andersen, Aaron Baker,
// Elias Software, Aras Pranckevicius, and Sean Barrett.
//
// LICENSE
//
//   See end of file for license information.
//
// Limitations:
//
//   - floor 0 not supported (used in old ogg vorbis files pre-2004)
//   - lossless sample-truncation at beginning ignored
//   - cannot concatenate multiple vorbis streams
//   - sample positions are 32-bit, limiting seekable 192Khz
//       files to around 6 hours (Ogg supports 64-bit)
//
// Feature contributors:
//    Dougall Johnson (sample-exact seeking)
//
// Bugfix/warning contributors:
//    Terje Mathisen     Niklas Frykholm     Andy Hill
//    Casey Muratori     John Bolton         Gargaj
//    Laurent Gomila     Marc LeBlanc        Ronny Chevalier
//    Bernhard Wodo      Evan Balster        github:alxprd
//    Tom Beaumont       Ingo Leitgeb        Nicolas Guillemot
//    Phillip Bennefall  Rohit               Thiago Goulart
//    github:manxorist   saga musix          github:infatum
//    Timur Gagiev       Maxwell Koo         Peter Waller
//    github:audinowho   Dougall Johnson     David Reid
//    github:Clownacy    Pedro J. Estebanez  Remi Verschelde
//
// Partial history:
//    1.20    - 2020-07-11 - several small fixes
//    1.19    - 2020-02-05 - warnings
//    1.18    - 2020-02-02 - fix seek bugs; parse header comments; misc warnings etc.
//    1.17    - 2019-07-08 - fix CVE-2019-13217..CVE-2019-13223 (by ForAllSecure)
//    1.16    - 2019-03-04 - fix warnings
//    1.15    - 2019-02-07 - explicit failure if Ogg Skeleton data is found
//    1.14    - 2018-02-11 - delete bogus dealloca usage
//    1.13    - 2018-01-29 - fix truncation of last frame (hopefully)
//    1.12    - 2017-11-21 - limit residue begin/end to blocksize/2 to avoid large temp allocs in bad/corrupt files
//    1.11    - 2017-07-23 - fix MinGW compilation
//    1.10    - 2017-03-03 - more robust seeking; fix negative ilog(); clear error in openMemory
//    1.09    - 2016-04-04 - back out 'truncation of last frame' fix from previous version
//    1.08    - 2016-04-02 - warnings; setup memory leaks; truncation of last frame
//    1.07    - 2015-01-16 - fixes for crashes on invalid files; warning fixes; const
//    1.06    - 2015-08-31 - full, correct support for seeking API (Dougall Johnson)
//                           some crash fixes when out of memory or with corrupt files
//                           fix some inappropriately signed shifts
//    1.05    - 2015-04-19 - don't define __forceinline if it's redundant
//    1.04    - 2014-08-27 - fix missing const-correct case in API
//    1.03    - 2014-08-07 - warning fixes
//    1.02    - 2014-07-09 - declare qsort comparison as explicitly _cdecl in Windows
//    1.01    - 2014-06-18 - fix stbVorbisGetSamplesFloat (interleaved was correct)
//    1.0     - 2014-05-26 - fix memory leaks; fix warnings; fix bugs in >2-channel;
//                           (API change) report sample rate for decode-full-file funcs
//
// See end of file for full version history.


//////////////////////////////////////////////////////////////////////////////
//
//  HEADER BEGINS HERE
//

#ifndef STB_VORBIS_INCLUDE_STB_VORBIS_H
#define STB_VORBIS_INCLUDE_STB_VORBIS_H

#if defined(STB_VORBIS_NO_CRT) && !defined(STB_VORBIS_NO_STDIO)
#define STB_VORBIS_NO_STDIO 1
#endif

#ifndef STB_VORBIS_NO_STDIO
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

///////////   THREAD SAFETY

// Individual stbVorbis* handles are not thread-safe; you cannot decode from
// them from multiple threads at the same time. However, you can have multiple
// stbVorbis* handles and decode from them independently in multiple thrads.


///////////   MEMORY ALLOCATION

// normally stbVorbis uses malloc() to allocate memory at startup,
// and alloca() to allocate temporary memory during a frame on the
// stack. (Memory consumption will depend on the amount of setup
// data in the file and how you set the compile flags for speed
// vs. size. In my test files the maximal-size usage is ~150KB.)
//
// You can modify the wrapper functions in the source (setupMalloc,
// setupTempMalloc, tempMalloc) to change this behavior, or you
// can use a simpler allocation model: you pass in a buffer from
// which stbVorbis will allocate _all_ its memory (including the
// temp memory). "open" may fail with a VORBIS_outofmem if you
// do not pass in enough data; there is no way to determine how
// much you do need except to succeed (at which point you can
// query getInfo to find the exact amount required. yes I know
// this is lame).
//
// If you pass in a non-NULL buffer of the type below, allocation
// will occur from it as described above. Otherwise just pass NULL
// to use malloc()/alloca()

typedef struct
{
   char *allocBuffer;
   int   allocBufferLengthInBytes;
} stbVorbisAlloc;


///////////   FUNCTIONS USEABLE WITH ALL INPUT MODES

typedef struct stbVorbis stbVorbis;

typedef struct
{
   unsigned int sampleRate;
   int channels;

   unsigned int setupMemoryRequired;
   unsigned int setupTempMemoryRequired;
   unsigned int tempMemoryRequired;

   int maxFrameSize;
} stbVorbisInfo;

// get general information about the file
extern stbVorbisInfo stbVorbisGetInfo(stbVorbis *f);

// get the last error detected (clears it, too)
extern int stbVorbisGetError(stbVorbis *f);

// close an ogg vorbis file and free all memory in use
extern void stbVorbisClose(stbVorbis *f);

// this function returns the offset (in samples) from the beginning of the
// file that will be returned by the next decode, if it is known, or -1
// otherwise. after a flushPushdata() call, this may take a while before
// it becomes valid again.
// NOT WORKING YET after a seek with PULLDATA API
extern int stbVorbisGetSampleOffset(stbVorbis *f);

// returns the current seek point within the file, or offset from the beginning
// of the memory buffer. In pushdata mode it returns 0.
extern unsigned int stbVorbisGetFileOffset(stbVorbis *f);

///////////   PUSHDATA API

#ifndef STB_VORBIS_NO_PUSHDATA_API

// this API allows you to get blocks of data from any source and hand
// them to stbVorbis. you have to buffer them; stbVorbis will tell
// you how much it used, and you have to give it the rest next time;
// and stbVorbis may not have enough data to work with and you will
// need to give it the same data again PLUS more. Note that the Vorbis
// specification does not bound the size of an individual frame.

extern stbVorbis *stbVorbisOpenPushdata(
         const unsigned char * datablock, int datablockLengthInBytes,
         int *datablockMemoryConsumedInBytes,
         int *error,
         const stbVorbisAlloc *allocBuffer);
// create a vorbis decoder by passing in the initial data block containing
//    the ogg&vorbis headers (you don't need to do parse them, just provide
//    the first N bytes of the file--you're told if it's not enough, see below)
// on success, returns an stbVorbis *, does not set error, returns the amount of
//    data parsed/consumed on this call in *datablockMemoryConsumedInBytes;
// on failure, returns NULL on error and sets *error, does not change *datablockMemoryConsumed
// if returns NULL and *error is VORBIS_needMoreData, then the input block was
//       incomplete and you need to pass in a larger block from the start of the file

extern int stbVorbisDecodeFramePushdata(
         stbVorbis *f,
         const unsigned char *datablock, int datablockLengthInBytes,
         int *channels,             // place to write number of float * buffers
         float ***output,           // place to write float ** array of float * buffers
         int *samples               // place to write number of output samples
     );
// decode a frame of audio sample data if possible from the passed-in data block
//
// return value: number of bytes we used from datablock
//
// possible cases:
//     0 bytes used, 0 samples output (need more data)
//     N bytes used, 0 samples output (resynching the stream, keep going)
//     N bytes used, M samples output (one frame of data)
// note that after opening a file, you will ALWAYS get one N-bytes,0-sample
// frame, because Vorbis always "discards" the first frame.
//
// Note that on resynch, stbVorbis will rarely consume all of the buffer,
// instead only datablockLengthInBytes-3 or less. This is because it wants
// to avoid missing parts of a page header if they cross a datablock boundary,
// without writing state-machiney code to record a partial detection.
//
// The number of channels returned are stored in *channels (which can be
// NULL--it is always the same as the number of channels reported by
// getInfo). *output will contain an array of float* buffers, one per
// channel. In other words, (*output)[0][0] contains the first sample from
// the first channel, and (*output)[1][0] contains the first sample from
// the second channel.

extern void stbVorbisFlushPushdata(stbVorbis *f);
// inform stbVorbis that your next datablock will not be contiguous with
// previous ones (e.g. you've seeked in the data); future attempts to decode
// frames will cause stbVorbis to resynchronize (as noted above), and
// once it sees a valid Ogg page (typically 4-8KB, as large as 64KB), it
// will begin decoding the _next_ frame.
//
// if you want to seek using pushdata, you need to seek in your file, then
// call stbVorbisFlushPushdata(), then start calling decoding, then once
// decoding is returning you data, call stbVorbisGetSampleOffset, and
// if you don't like the result, seek your file again and repeat.
#endif


//////////   PULLING INPUT API

#ifndef STB_VORBIS_NO_PULLDATA_API
// This API assumes stbVorbis is allowed to pull data from a source--
// either a block of memory containing the _entire_ vorbis stream, or a
// FILE * that you or it create, or possibly some other reading mechanism
// if you go modify the source to replace the FILE * case with some kind
// of callback to your code. (But if you don't support seeking, you may
// just want to go ahead and use pushdata.)

#if !defined(STB_VORBIS_NO_STDIO) && !defined(STB_VORBIS_NO_INTEGER_CONVERSION)
extern int stbVorbisDecodeFilename(const char *filename, int *channels, int *sampleRate, short **output);
#endif
#if !defined(STB_VORBIS_NO_INTEGER_CONVERSION)
extern int stbVorbisDecodeMemory(const unsigned char *mem, int len, int *channels, int *sampleRate, short **output);
#endif
// decode an entire file and output the data interleaved into a malloc()ed
// buffer stored in *output. The return value is the number of samples
// decoded, or -1 if the file could not be opened or was not an ogg vorbis file.
// When you're done with it, just free() the pointer returned in *output.

extern stbVorbis * stbVorbisOpenMemory(const unsigned char *data, int len,
                                  int *error, const stbVorbisAlloc *allocBuffer);
// create an ogg vorbis decoder from an ogg vorbis stream in memory (note
// this must be the entire stream!). on failure, returns NULL and sets *error

#ifndef STB_VORBIS_NO_STDIO
extern stbVorbis * stbVorbisOpenFilename(const char *filename,
                                  int *error, const stbVorbisAlloc *allocBuffer);
// create an ogg vorbis decoder from a filename via fopen(). on failure,
// returns NULL and sets *error (possibly to VORBIS_fileOpenFailure).

extern int stbVorbisSeekFrame(stbVorbis *f, unsigned int sampleNumber);
extern int stbVorbisSeek(stbVorbis *f, unsigned int sampleNumber);
// these functions seek in the Vorbis file to (approximately) 'sampleNumber'.
// after calling seekFrame(), the next call to getFrame_*() will include
// the specified sample. after calling stbVorbisSeek(), the next call to
// stbVorbisGetSamples_* will start with the specified sample. If you
// do not need to seek to EXACTLY the target sample when using getSamples_*,
// you can also use seekFrame().

extern int stbVorbisSeekStart(stbVorbis *f);
// this function is equivalent to stbVorbisSeek(f,0)

extern unsigned int stbVorbisStreamLengthInSamples(stbVorbis *f);
extern float        stbVorbisStreamLengthInSeconds(stbVorbis *f);
// these functions return the total length of the vorbis stream

extern int stbVorbisGetFrameFloat(stbVorbis *f, int *channels, float ***output);
// decode the next frame and return the number of samples. the number of
// channels returned are stored in *channels (which can be NULL--it is always
// the same as the number of channels reported by getInfo). *output will
// contain an array of float* buffers, one per channel. These outputs will
// be overwritten on the next call to stbVorbisGetFrame_*.
//
// You generally should not intermix calls to stbVorbisGetFrame_*()
// and stbVorbisGetSamples_*(), since the latter calls the former.

#ifndef STB_VORBIS_NO_INTEGER_CONVERSION
extern int stbVorbisGetFrameShortInterleaved(stbVorbis *f, int numC, short *buffer, int numShorts);
extern int stbVorbisGetFrameShort            (stbVorbis *f, int numC, short **buffer, int numSamples);
#endif
// decode the next frame and return the number of *samples* per channel.
// Note that for interleaved data, you pass in the number of shorts (the
// size of your array), but the return value is the number of samples per
// channel, not the total number of samples.
//
// The data is coerced to the number of channels you request according to the
// channel coercion rules (see below). You must pass in the size of your
// buffer(s) so that stbVorbis will not overwrite the end of the buffer.
// The maximum buffer size needed can be gotten from getInfo(); however,
// the Vorbis I specification implies an absolute maximum of 4096 samples
// per channel.

// Channel coercion rules:
//    Let M be the number of channels requested, and N the number of channels present,
//    and Cn be the nth channel; let stereo L be the sum of all L and center channels,
//    and stereo R be the sum of all R and center channels (channel assignment from the
//    vorbis spec).
//        M    N       output
//        1    k      sum(Ck) for all k
//        2    *      stereo L, stereo R
//        k    l      k > l, the first l channels, then 0s
//        k    l      k <= l, the first k channels
//    Note that this is not _good_ surround etc. mixing at all! It's just so
//    you get something useful.

extern int stbVorbisGetSamplesFloatInterleaved(stbVorbis *f, int channels, float *buffer, int numFloats);
extern int stbVorbisGetSamplesFloat(stbVorbis *f, int channels, float **buffer, int numSamples);
// gets numSamples samples, not necessarily on a frame boundary--this requires
// buffering so you have to supply the buffers. DOES NOT APPLY THE COERCION RULES.
// Returns the number of samples stored per channel; it may be less than requested
// at the end of the file. If there are no more samples in the file, returns 0.

#ifndef STB_VORBIS_NO_INTEGER_CONVERSION
extern int stbVorbisGetSamplesShortInterleaved(stbVorbis *f, int channels, short *buffer, int numShorts);
extern int stbVorbisGetSamplesShort(stbVorbis *f, int channels, short **buffer, int numSamples);
#endif
// gets numSamples samples, not necessarily on a frame boundary--this requires
// buffering so you have to supply the buffers. Applies the coercion rules above
// to produce 'channels' channels. Returns the number of samples stored per channel;
// it may be less than requested at the end of the file. If there are no more
// samples in the file, returns 0.

#endif

////////   ERROR CODES

enum STBVorbisError
{
   VORBIS__noError,

   VORBIS_needMoreData=1,             // not a real error

   VORBIS_invalidApiMixing,           // can't mix API modes
   VORBIS_outofmem,                     // not enough memory
   VORBIS_featureNotSupported,        // uses floor 0
   VORBIS_tooManyChannels,            // STB_VORBIS_MAX_CHANNELS is too small
   VORBIS_fileOpenFailure,            // fopen() failed
   VORBIS_seekWithoutLength,          // can't seek in unknown-length file

   VORBIS_unexpectedEof=10,            // file is truncated?
   VORBIS_seekInvalid,                 // seek past EOF

   // decoding errors (corrupt/invalid stream) -- you probably
   // don't care about the exact details of these

   // vorbis errors:
   VORBIS_invalidSetup=20,
   VORBIS_invalidStream,

   // ogg errors:
   VORBIS_missingCapturePattern=30,
   VORBIS_invalidStreamStructureVersion,
   VORBIS_continuedPacketFlagInvalid,
   VORBIS_incorrectStreamSerialNumber,
   VORBIS_invalidFirstPage,
   VORBIS_badPacketType,
   VORBIS_cantFindLastPage,
   VORBIS_seekFailed,
   VORBIS_oggSkeletonNotSupported
};


#ifdef __cplusplus
}
#endif

#endif // STB_VORBIS_INCLUDE_STB_VORBIS_H
//
//  HEADER ENDS HERE
//
//////////////////////////////////////////////////////////////////////////////

#ifndef STB_VORBIS_HEADER_ONLY

// global configuration settings (e.g. set these in the project/makefile),
// or just set them in this file at the top (although ideally the first few
// should be visible when the header file is compiled too, although it's not
// crucial)

// STB_VORBIS_NO_PUSHDATA_API
//     does not compile the code for the various stbVorbis_*_pushdata()
//     functions
// #define STB_VORBIS_NO_PUSHDATA_API

// STB_VORBIS_NO_PULLDATA_API
//     does not compile the code for the non-pushdata APIs
// #define STB_VORBIS_NO_PULLDATA_API

// STB_VORBIS_NO_STDIO
//     does not compile the code for the APIs that use FILE *s internally
//     or externally (implied by STB_VORBIS_NO_PULLDATA_API)
// #define STB_VORBIS_NO_STDIO

// STB_VORBIS_NO_INTEGER_CONVERSION
//     does not compile the code for converting audio sample data from
//     float to integer (implied by STB_VORBIS_NO_PULLDATA_API)
// #define STB_VORBIS_NO_INTEGER_CONVERSION

// STB_VORBIS_NO_FAST_SCALED_FLOAT
//      does not use a fast float-to-int trick to accelerate float-to-int on
//      most platforms which requires endianness be defined correctly.
//#define STB_VORBIS_NO_FAST_SCALED_FLOAT


// STB_VORBIS_MAX_CHANNELS [number]
//     globally define this to the maximum number of channels you need.
//     The spec does not put a restriction on channels except that
//     the count is stored in a byte, so 255 is the hard limit.
//     Reducing this saves about 16 bytes per value, so using 16 saves
//     (255-16)*16 or around 4KB. Plus anything other memory usage
//     I forgot to account for. Can probably go as low as 8 (7.1 audio),
//     6 (5.1 audio), or 2 (stereo only).
#ifndef STB_VORBIS_MAX_CHANNELS
#define STB_VORBIS_MAX_CHANNELS    16  // enough for anyone?
#endif

// STB_VORBIS_PUSHDATA_CRC_COUNT [number]
//     after a flushPushdata(), stbVorbis begins scanning for the
//     next valid page, without backtracking. when it finds something
//     that looks like a page, it streams through it and verifies its
//     CRC32. Should that validation fail, it keeps scanning. But it's
//     possible that _while_ streaming through to check the CRC32 of
//     one candidate page, it sees another candidate page. This #define
//     determines how many "overlapping" candidate pages it can search
//     at once. Note that "real" pages are typically ~4KB to ~8KB, whereas
//     garbage pages could be as big as 64KB, but probably average ~16KB.
//     So don't hose ourselves by scanning an apparent 64KB page and
//     missing a ton of real ones in the interim; so minimum of 2
#ifndef STB_VORBIS_PUSHDATA_CRC_COUNT
#define STB_VORBIS_PUSHDATA_CRC_COUNT  4
#endif

// STB_VORBIS_FAST_HUFFMAN_LENGTH [number]
//     sets the log size of the huffman-acceleration table.  Maximum
//     supported value is 24. with larger numbers, more decodings are O(1),
//     but the table size is larger so worse cache missing, so you'll have
//     to probe (and try multiple ogg vorbis files) to find the sweet spot.
#ifndef STB_VORBIS_FAST_HUFFMAN_LENGTH
#define STB_VORBIS_FAST_HUFFMAN_LENGTH   10
#endif

// STB_VORBIS_FAST_BINARY_LENGTH [number]
//     sets the log size of the binary-search acceleration table. this
//     is used in similar fashion to the fast-huffman size to set initial
//     parameters for the binary search

// STB_VORBIS_FAST_HUFFMAN_INT
//     The fast huffman tables are much more efficient if they can be
//     stored as 16-bit results instead of 32-bit results. This restricts
//     the codebooks to having only 65535 possible outcomes, though.
//     (At least, accelerated by the huffman table.)
#ifndef STB_VORBIS_FAST_HUFFMAN_INT
#define STB_VORBIS_FAST_HUFFMAN_SHORT
#endif

// STB_VORBIS_NO_HUFFMAN_BINARY_SEARCH
//     If the 'fast huffman' search doesn't succeed, then stbVorbis falls
//     back on binary searching for the correct one. This requires storing
//     extra tables with the huffman codes in sorted order. Defining this
//     symbol trades off space for speed by forcing a linear search in the
//     non-fast case, except for "sparse" codebooks.
// #define STB_VORBIS_NO_HUFFMAN_BINARY_SEARCH

// STB_VORBIS_DIVIDES_IN_RESIDUE
//     stbVorbis precomputes the result of the scalar residue decoding
//     that would otherwise require a divide per chunk. you can trade off
//     space for time by defining this symbol.
// #define STB_VORBIS_DIVIDES_IN_RESIDUE

// STB_VORBIS_DIVIDES_IN_CODEBOOK
//     vorbis VQ codebooks can be encoded two ways: with every case explicitly
//     stored, or with all elements being chosen from a small range of values,
//     and all values possible in all elements. By default, stbVorbis expands
//     this latter kind out to look like the former kind for ease of decoding,
//     because otherwise an integer divide-per-vector-element is required to
//     unpack the index. If you define STB_VORBIS_DIVIDES_IN_CODEBOOK, you can
//     trade off storage for speed.
//#define STB_VORBIS_DIVIDES_IN_CODEBOOK

#ifdef STB_VORBIS_CODEBOOK_SHORTS
#error "STB_VORBIS_CODEBOOK_SHORTS is no longer supported as it produced incorrect results for some input formats"
#endif

// STB_VORBIS_DIVIDE_TABLE
//     this replaces small integer divides in the floor decode loop with
//     table lookups. made less than 1% difference, so disabled by default.

// STB_VORBIS_NO_INLINE_DECODE
//     disables the inlining of the scalar codebook fast-huffman decode.
//     might save a little codespace; useful for debugging
// #define STB_VORBIS_NO_INLINE_DECODE

// STB_VORBIS_NO_DEFER_FLOOR
//     Normally we only decode the floor without synthesizing the actual
//     full curve. We can instead synthesize the curve immediately. This
//     requires more memory and is very likely slower, so I don't think
//     you'd ever want to do it except for debugging.
// #define STB_VORBIS_NO_DEFER_FLOOR




//////////////////////////////////////////////////////////////////////////////

#ifdef STB_VORBIS_NO_PULLDATA_API
   #define STB_VORBIS_NO_INTEGER_CONVERSION
   #define STB_VORBIS_NO_STDIO
#endif
// MB: defaulting to no STDIO.
#define STB_VORBIS_NO_STDIO 1

#if defined(STB_VORBIS_NO_CRT) && !defined(STB_VORBIS_NO_STDIO)
   #define STB_VORBIS_NO_STDIO 1
#endif

#ifndef STB_VORBIS_NO_INTEGER_CONVERSION
#ifndef STB_VORBIS_NO_FAST_SCALED_FLOAT

   // only need endianness for fast-float-to-int, which we don't
   // use for pushdata

   #ifndef STB_VORBIS_BIG_ENDIAN
     #define STB_VORBIS_ENDIAN  0
   #else
     #define STB_VORBIS_ENDIAN  1
   #endif

#endif
#endif


#ifndef STB_VORBIS_NO_STDIO
#include <stdio.h>
#endif

#ifndef STB_VORBIS_NO_CRT
   #include <stdlib.h>
   #include <string.h>
   #include <assert.h>
   #include <math.h>
#else // STB_VORBIS_NO_CRT
   #define NULL 0
   #define malloc(s)   0
   #define free(s)     ((void) 0)
   #define realloc(s)  0
#endif // STB_VORBIS_NO_CRT

/* we need alloca() regardless of STB_VORBIS_NO_CRT,
 * because there is not a corresponding 'dealloca' */
#if !defined(alloca)
# if defined(HAVE_ALLOCA_H)
#  include <alloca.h>
# elif defined(__GNUC__)
#  define alloca __builtinAlloca
# elif defined(_MSC_VER)
#  include <malloc.h>
#  define alloca _alloca
# elif defined(__WATCOMC__)
#  include <malloc.h>
# endif
#endif

#include <limits.h>

#ifndef STB_FORCEINLINE
    #if defined(_MSC_VER)
        #define STB_FORCEINLINE __forceinline
    #elif defined(__GNUC__) || defined(__clang__)
        #define STB_FORCEINLINE static __inline __attribute__((always_inline))
    #else
        #define STB_FORCEINLINE static __inline
    #endif
#endif

#if STB_VORBIS_MAX_CHANNELS > 256
#error "Value of STB_VORBIS_MAX_CHANNELS outside of allowed range"
#endif

#if STB_VORBIS_FAST_HUFFMAN_LENGTH > 24
#error "Value of STB_VORBIS_FAST_HUFFMAN_LENGTH outside of allowed range"
#endif


#if 0
#include <crtdbg.h>
#define CHECK(f)   _CrtIsValidHeapPointer(f->channelBuffers[1])
#else
#define CHECK(f)   ((void) 0)
#endif

#define MAX_BLOCKSIZE_LOG  13   // from specification
#define MAX_BLOCKSIZE      (1 << MAX_BLOCKSIZE_LOG)


typedef unsigned char  uint8;
typedef   signed char   int8;
typedef unsigned short uint16;
typedef   signed short  int16;
typedef unsigned int   uint32;
typedef   signed int    int32;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

typedef float codetype;

// @NOTE
//
// Some arrays below are tagged "//varies", which means it's actually
// a variable-sized piece of data, but rather than malloc I assume it's
// small enough it's better to just allocate it all together with the
// main thing
//
// Most of the variables are specified with the smallest size I could pack
// them into. It might give better performance to make them all full-sized
// integers. It should be safe to freely rearrange the structures or change
// the sizes larger--nothing relies on silently truncating etc., nor the
// order of variables.

#define FAST_HUFFMAN_TABLE_SIZE   (1 << STB_VORBIS_FAST_HUFFMAN_LENGTH)
#define FAST_HUFFMAN_TABLE_MASK   (FAST_HUFFMAN_TABLE_SIZE - 1)

typedef struct
{
   int dimensions, entries;
   uint8 *codewordLengths;
   float  minimumValue;
   float  deltaValue;
   uint8  valueBits;
   uint8  lookupType;
   uint8  sequenceP;
   uint8  sparse;
   uint32 lookupValues;
   codetype *multiplicands;
   uint32 *codewords;
   #ifdef STB_VORBIS_FAST_HUFFMAN_SHORT
    int16  fastHuffman[FAST_HUFFMAN_TABLE_SIZE];
   #else
    int32  fastHuffman[FAST_HUFFMAN_TABLE_SIZE];
   #endif
   uint32 *sortedCodewords;
   int    *sortedValues;
   int     sortedEntries;
} Codebook;

typedef struct
{
   uint8 order;
   uint16 rate;
   uint16 barkMapSize;
   uint8 amplitudeBits;
   uint8 amplitudeOffset;
   uint8 numberOfBooks;
   uint8 bookList[16]; // varies
} Floor0;

typedef struct
{
   uint8 partitions;
   uint8 partitionClassList[32]; // varies
   uint8 classDimensions[16]; // varies
   uint8 classSubclasses[16]; // varies
   uint8 classMasterbooks[16]; // varies
   int16 subclassBooks[16][8]; // varies
   uint16 Xlist[31*8+2]; // varies
   uint8 sortedOrder[31*8+2];
   uint8 neighbors[31*8+2][2];
   uint8 floor1_multiplier;
   uint8 rangebits;
   int values;
} Floor1;

typedef union
{
   Floor0 floor0;
   Floor1 floor1;
} Floor;

typedef struct
{
   uint32 begin, end;
   uint32 partSize;
   uint8 classifications;
   uint8 classbook;
   uint8 **classdata;
   int16 (*residueBooks)[8];
} Residue;

typedef struct
{
   uint8 magnitude;
   uint8 angle;
   uint8 mux;
} MappingChannel;

typedef struct
{
   uint16 couplingSteps;
   MappingChannel *chan;
   uint8  submaps;
   uint8  submapFloor[15]; // varies
   uint8  submapResidue[15]; // varies
} Mapping;

typedef struct
{
   uint8 blockflag;
   uint8 mapping;
   uint16 windowtype;
   uint16 transformtype;
} Mode;

typedef struct
{
   uint32  goalCrc;    // expected crc if match
   int     bytesLeft;  // bytes left in packet
   uint32  crcSoFar;  // running crc
   int     bytesDone;  // bytes processed in _current_ chunk
   uint32  sampleLoc;  // granule pos encoded in page
} CRCscan;

typedef struct
{
   uint32 pageStart, pageEnd;
   uint32 lastDecodedSample;
} ProbedPage;

struct stbVorbis
{
  // user-accessible info
   unsigned int sampleRate;
   int channels;

   unsigned int setupMemoryRequired;
   unsigned int tempMemoryRequired;
   unsigned int setupTempMemoryRequired;

  // input config
#ifndef STB_VORBIS_NO_STDIO
   FILE *f;
   uint32 fStart;
   int closeOnFree;
#endif

   uint8 *stream;
   uint8 *streamStart;
   uint8 *streamEnd;

   uint32 streamLen;

   uint8  pushMode;

   // the page to seek to when seeking to start, may be zero
   uint32 firstAudioPageOffset;

   // pFirst is the page on which the first audio packet ends
   // (but not necessarily the page on which it starts)
   ProbedPage pFirst, pLast;

  // memory management
   stbVorbisAlloc alloc;
   int setupOffset;
   int tempOffset;

  // run-time results
   int eof;
   enum STBVorbisError error;

  // user-useful data

  // header info
   int blocksize[2];
   int blocksize_0, blocksize_1;
   int codebookCount;
   Codebook *codebooks;
   int floorCount;
   uint16 floorTypes[64]; // varies
   Floor *floorConfig;
   int residueCount;
   uint16 residueTypes[64]; // varies
   Residue *residueConfig;
   int mappingCount;
   Mapping *mapping;
   int modeCount;
   Mode modeConfig[64];  // varies

   uint32 totalSamples;

  // decode buffer
   float *channelBuffers[STB_VORBIS_MAX_CHANNELS];
   float *outputs        [STB_VORBIS_MAX_CHANNELS];

   float *previousWindow[STB_VORBIS_MAX_CHANNELS];
   int previousLength;

   #ifndef STB_VORBIS_NO_DEFER_FLOOR
   int16 *finalY[STB_VORBIS_MAX_CHANNELS];
   #else
   float *floorBuffers[STB_VORBIS_MAX_CHANNELS];
   #endif

   uint32 currentLoc; // sample location of next frame to decode
   int    currentLocValid;

  // per-blocksize precomputed data

   // twiddle factors
   float *A[2],*B[2],*C[2];
   float *window[2];
   uint16 *bitReverse[2];

  // current page/packet/segment streaming info
   uint32 serial; // stream serial number for verification
   int lastPage;
   int segmentCount;
   uint8 segments[255];
   uint8 pageFlag;
   uint8 bytesInSeg;
   uint8 firstDecode;
   int nextSeg;
   int lastSeg;  // flag that we're on the last segment
   int lastSegWhich; // what was the segment number of the last seg?
   uint32 acc;
   int validBits;
   int packetBytes;
   int endSegWithKnownLoc;
   uint32 knownLocForPacket;
   int discardSamplesDeferred;
   uint32 samplesOutput;

  // push mode scanning
   int pageCrcTests; // only in pushMode: number of tests active; -1 if not searching
#ifndef STB_VORBIS_NO_PUSHDATA_API
   CRCscan scan[STB_VORBIS_PUSHDATA_CRC_COUNT];
#endif

  // sample-access
   int channelBufferStart;
   int channelBufferEnd;
};

#if defined(STB_VORBIS_NO_PUSHDATA_API)
   #define IS_PUSH_MODE(f)   FALSE
#elif defined(STB_VORBIS_NO_PULLDATA_API)
   #define IS_PUSH_MODE(f)   TRUE
#else
   #define IS_PUSH_MODE(f)   ((f)->pushMode)
#endif

typedef struct stbVorbis vorb;

static int stbVorbisError(vorb *f, enum STBVorbisError e) {
   f->error = e;
   if (!f->eof && e != VORBIS_needMoreData) {
      f->error=e; // breakpoint for debugging
   }
   return 0;
}


// these functions are used for allocating temporary memory
// while decoding. if you can afford the stack space, use
// alloca(); otherwise, provide a temp buffer and it will
// allocate out of those.

#define arraySizeRequired(count,size)  (count*(sizeof(void *)+(size)))

#define tempAlloc(f,size)              (f->alloc.allocBuffer ? setupTempMalloc(f,size) : alloca(size))
#define tempFree(f,p)                  (void)0
#define tempAllocSave(f)              ((f)->tempOffset)
#define tempAllocRestore(f,p)         ((f)->tempOffset = (p))

#define tempBlockArray(f,count,size)  makeBlockArray(tempAlloc(f,arraySizeRequired(count,size)), count, size)

// given a sufficiently large block of memory, make an array of pointers to subblocks of it
static void *makeBlockArray(void *mem, int count, int size)
{
   int i;
   void ** p = (void **) mem;
   char *q = (char *) (p + count);
   for (i=0; i < count; ++i) {
      p[i] = q;
      q += size;
   }
   return p;
}

static void *setupMalloc(vorb *f, int sz)
{
   sz = (sz+7) & ~7; // round up to nearest 8 for alignment of future allocs.
   f->setupMemoryRequired += sz;
   if (f->alloc.allocBuffer) {
      void *p = (char *) f->alloc.allocBuffer + f->setupOffset;
      if (f->setupOffset + sz > f->tempOffset) return NULL;
      f->setupOffset += sz;
      return p;
   }
   return sz ? malloc(sz) : NULL;
}

static void setupFree(vorb *f, void *p)
{
   if (f->alloc.allocBuffer) return; // do nothing; setup mem is a stack
   free(p);
}

static void *setupTempMalloc(vorb *f, int sz)
{
   sz = (sz+7) & ~7; // round up to nearest 8 for alignment of future allocs.
   if (f->alloc.allocBuffer) {
      if (f->tempOffset - sz < f->setupOffset) return NULL;
      f->tempOffset -= sz;
      return (char *) f->alloc.allocBuffer + f->tempOffset;
   }
   return malloc(sz);
}

static void setupTempFree(vorb *f, void *p, int sz)
{
   if (f->alloc.allocBuffer) {
      f->tempOffset += (sz+7)&~7;
      return;
   }
   free(p);
}

#define CRC32_POLY    0x04c11db7   // from spec

static uint32 crcTable[256];
static void crc32_init(void)
{
   int i,j;
   uint32 s;
   for(i=0; i < 256; i++) {
      for (s=(uint32) i << 24, j=0; j < 8; ++j)
         s = (s << 1) ^ (s >= (1U<<31) ? CRC32_POLY : 0);
      crcTable[i] = s;
   }
}

STB_FORCEINLINE uint32 crc32_update(uint32 crc, uint8 byte)
{
   return (crc << 8) ^ crcTable[byte ^ (crc >> 24)];
}


// used in setup, and for huffman that doesn't go fast path
static unsigned int bitReverse(unsigned int n)
{
  n = ((n & 0xAAAAAAAA) >>  1) | ((n & 0x55555555) << 1);
  n = ((n & 0xCCCCCCCC) >>  2) | ((n & 0x33333333) << 2);
  n = ((n & 0xF0F0F0F0) >>  4) | ((n & 0x0F0F0F0F) << 4);
  n = ((n & 0xFF00FF00) >>  8) | ((n & 0x00FF00FF) << 8);
  return (n >> 16) | (n << 16);
}

static float square(float x)
{
   return x*x;
}

// this is a weird definition of log2() for which log2(1) = 1, log2(2) = 2, log2(4) = 3
// as required by the specification. fast(?) implementation from stb.h
// @OPTIMIZE: called multiple times per-packet with "constants"; move to setup
static int ilog(int32 n)
{
   static signed char log2_4[16] = { 0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4 };

   if (n < 0) return 0; // signed n returns 0

   // 2 compares if n < 16, 3 compares otherwise (4 if signed or n > 1<<29)
   if (n < (1 << 14))
        if (n < (1 <<  4))            return  0 + log2_4[n      ];
        else if (n < (1 <<  9))       return  5 + log2_4[n >>  5];
             else                     return 10 + log2_4[n >> 10];
   else if (n < (1 << 24))
             if (n < (1 << 19))       return 15 + log2_4[n >> 15];
             else                     return 20 + log2_4[n >> 20];
        else if (n < (1 << 29))       return 25 + log2_4[n >> 25];
             else                     return 30 + log2_4[n >> 30];
}

#ifndef M_PI
  #define M_PI  3.14159265358979323846264f  // from CRC
#endif

// code length assigned to a value with no huffman encoding
#define NO_CODE   255

/////////////////////// LEAF SETUP FUNCTIONS //////////////////////////
//
// these functions are only called at setup, and only a few times
// per file

static float float32_unpack(uint32 x)
{
   // from the specification
   uint32 mantissa = x & 0x1fffff;
   uint32 sign = x & 0x80000000;
   uint32 exp = (x & 0x7fe00000) >> 21;
   double res = sign ? -(double)mantissa : (double)mantissa;
   return (float) ldexp((float)res, exp-788);
}


// zlib & jpeg huffman tables assume that the output symbols
// can either be arbitrarily arranged, or have monotonically
// increasing frequencies--they rely on the lengths being sorted;
// this makes for a very simple generation algorithm.
// vorbis allows a huffman table with non-sorted lengths. This
// requires a more sophisticated construction, since symbols in
// order do not map to huffman codes "in order".
static void addEntry(Codebook *c, uint32 huffCode, int symbol, int count, int len, uint32 *values)
{
   if (!c->sparse) {
      c->codewords      [symbol] = huffCode;
   } else {
      c->codewords       [count] = huffCode;
      c->codewordLengths[count] = len;
      values             [count] = symbol;
   }
}

static int computeCodewords(Codebook *c, uint8 *len, int n, uint32 *values)
{
   int i,k,m=0;
   uint32 available[32];

   memset(available, 0, sizeof(available));
   // find the first entry
   for (k=0; k < n; ++k) if (len[k] < NO_CODE) break;
   if (k == n) { assert(c->sortedEntries == 0); return TRUE; }
   // add to the list
   addEntry(c, 0, k, m++, len[k], values);
   // add all available leaves
   for (i=1; i <= len[k]; ++i)
      available[i] = 1U << (32-i);
   // note that the above code treats the first case specially,
   // but it's really the same as the following code, so they
   // could probably be combined (except the initial code is 0,
   // and I use 0 in available[] to mean 'empty')
   for (i=k+1; i < n; ++i) {
      uint32 res;
      int z = len[i], y;
      if (z == NO_CODE) continue;
      // find lowest available leaf (should always be earliest,
      // which is what the specification calls for)
      // note that this property, and the fact we can never have
      // more than one free leaf at a given level, isn't totally
      // trivial to prove, but it seems true and the assert never
      // fires, so!
      while (z > 0 && !available[z]) --z;
      if (z == 0) { return FALSE; }
      res = available[z];
      assert(z >= 0 && z < 32);
      available[z] = 0;
      addEntry(c, bitReverse(res), i, m++, len[i], values);
      // propagate availability up the tree
      if (z != len[i]) {
         assert(/*len[i] >= 0 &&*/ len[i] < 32);
         for (y=len[i]; y > z; --y) {
            assert(available[y] == 0);
            available[y] = res + (1 << (32-y));
         }
      }
   }
   return TRUE;
}

// accelerated huffman table allows fast O(1) match of all symbols
// of length <= STB_VORBIS_FAST_HUFFMAN_LENGTH
static void computeAcceleratedHuffman(Codebook *c)
{
   int i, len;
   for (i=0; i < FAST_HUFFMAN_TABLE_SIZE; ++i)
      c->fastHuffman[i] = -1;

   len = c->sparse ? c->sortedEntries : c->entries;
   #ifdef STB_VORBIS_FAST_HUFFMAN_SHORT
   if (len > 32767) len = 32767; // largest possible value we can encode!
   #endif
   for (i=0; i < len; ++i) {
      if (c->codewordLengths[i] <= STB_VORBIS_FAST_HUFFMAN_LENGTH) {
         uint32 z = c->sparse ? bitReverse(c->sortedCodewords[i]) : c->codewords[i];
         // set table entries for all bit combinations in the higher bits
         while (z < FAST_HUFFMAN_TABLE_SIZE) {
             c->fastHuffman[z] = i;
             z += 1 << c->codewordLengths[i];
         }
      }
   }
}

#ifdef _MSC_VER
#define STBV_CDECL __cdecl
#else
#define STBV_CDECL
#endif

static int STBV_CDECL uint32_compare(const void *p, const void *q)
{
   uint32 x = * (uint32 *) p;
   uint32 y = * (uint32 *) q;
   return x < y ? -1 : x > y;
}

static int includeInSort(Codebook *c, uint8 len)
{
   if (c->sparse) { assert(len != NO_CODE); return TRUE; }
   if (len == NO_CODE) return FALSE;
   if (len > STB_VORBIS_FAST_HUFFMAN_LENGTH) return TRUE;
   return FALSE;
}

// if the fast table above doesn't work, we want to binary
// search them... need to reverse the bits
static void computeSortedHuffman(Codebook *c, uint8 *lengths, uint32 *values)
{
   int i, len;
   // build a list of all the entries
   // OPTIMIZATION: don't include the short ones, since they'll be caught by FAST_HUFFMAN.
   // this is kind of a frivolous optimization--I don't see any performance improvement,
   // but it's like 4 extra lines of code, so.
   if (!c->sparse) {
      int k = 0;
      for (i=0; i < c->entries; ++i)
         if (includeInSort(c, lengths[i]))
            c->sortedCodewords[k++] = bitReverse(c->codewords[i]);
      assert(k == c->sortedEntries);
   } else {
      for (i=0; i < c->sortedEntries; ++i)
         c->sortedCodewords[i] = bitReverse(c->codewords[i]);
   }

   qsort(c->sortedCodewords, c->sortedEntries, sizeof(c->sortedCodewords[0]), uint32_compare);
   c->sortedCodewords[c->sortedEntries] = 0xffffffff;

   len = c->sparse ? c->sortedEntries : c->entries;
   // now we need to indicate how they correspond; we could either
   //   #1: sort a different data structure that says who they correspond to
   //   #2: for each sorted entry, search the original list to find who corresponds
   //   #3: for each original entry, find the sorted entry
   // #1 requires extra storage, #2 is slow, #3 can use binary search!
   for (i=0; i < len; ++i) {
      int huffLen = c->sparse ? lengths[values[i]] : lengths[i];
      if (includeInSort(c,huffLen)) {
         uint32 code = bitReverse(c->codewords[i]);
         int x=0, n=c->sortedEntries;
         while (n > 1) {
            // invariant: sc[x] <= code < sc[x+n]
            int m = x + (n >> 1);
            if (c->sortedCodewords[m] <= code) {
               x = m;
               n -= (n>>1);
            } else {
               n >>= 1;
            }
         }
         assert(c->sortedCodewords[x] == code);
         if (c->sparse) {
            c->sortedValues[x] = values[i];
            c->codewordLengths[x] = huffLen;
         } else {
            c->sortedValues[x] = i;
         }
      }
   }
}

// only run while parsing the header (3 times)
static int vorbisValidate(uint8 *data)
{
   static uint8 vorbis[6] = { 'v', 'o', 'r', 'b', 'i', 's' };
   return memcmp(data, vorbis, 6) == 0;
}

// called from setup only, once per code book
// (formula implied by specification)
static int lookup1_values(int entries, int dim)
{
   int r = (int) floor(exp((float) log((float) entries) / dim));
   if ((int) floor(pow((float) r+1, dim)) <= entries)   // (int) cast for MinGW warning;
      ++r;                                              // floor() to avoid _ftol() when non-CRT
   if (pow((float) r+1, dim) <= entries)
      return -1;
   if ((int) floor(pow((float) r, dim)) > entries)
      return -1;
   return r;
}

// called twice per file
static void computeTwiddleFactors(int n, float *A, float *B, float *C)
{
   int n4 = n >> 2, n8 = n >> 3;
   int k,k2;

   for (k=k2=0; k < n4; ++k,k2+=2) {
      A[k2  ] = (float)  cos(4*k*M_PI/n);
      A[k2+1] = (float) -sin(4*k*M_PI/n);
      B[k2  ] = (float)  cos((k2+1)*M_PI/n/2) * 0.5f;
      B[k2+1] = (float)  sin((k2+1)*M_PI/n/2) * 0.5f;
   }
   for (k=k2=0; k < n8; ++k,k2+=2) {
      C[k2  ] = (float)  cos(2*(k2+1)*M_PI/n);
      C[k2+1] = (float) -sin(2*(k2+1)*M_PI/n);
   }
}

static void computeWindow(int n, float *window)
{
   int n2 = n >> 1, i;
   for (i=0; i < n2; ++i)
      window[i] = (float) sin(0.5 * M_PI * square((float) sin((i - 0 + 0.5) / n2 * 0.5 * M_PI)));
}

static void computeBitreverse(int n, uint16 *rev)
{
   int ld = ilog(n) - 1; // ilog is off-by-one from normal definitions
   int i, n8 = n >> 3;
   for (i=0; i < n8; ++i)
      rev[i] = (bitReverse(i) >> (32-ld+3)) << 2;
}

static int initBlocksize(vorb *f, int b, int n)
{
   int n2 = n >> 1, n4 = n >> 2, n8 = n >> 3;
   f->A[b] = (float *) setupMalloc(f, sizeof(float) * n2);
   f->B[b] = (float *) setupMalloc(f, sizeof(float) * n2);
   f->C[b] = (float *) setupMalloc(f, sizeof(float) * n4);
   if (!f->A[b] || !f->B[b] || !f->C[b]) return stbVorbisError(f, VORBIS_outofmem);
   computeTwiddleFactors(n, f->A[b], f->B[b], f->C[b]);
   f->window[b] = (float *) setupMalloc(f, sizeof(float) * n2);
   if (!f->window[b]) return stbVorbisError(f, VORBIS_outofmem);
   computeWindow(n, f->window[b]);
   f->bitReverse[b] = (uint16 *) setupMalloc(f, sizeof(uint16) * n8);
   if (!f->bitReverse[b]) return stbVorbisError(f, VORBIS_outofmem);
   computeBitreverse(n, f->bitReverse[b]);
   return TRUE;
}

static void neighbors(uint16 *x, int n, int *plow, int *phigh)
{
   int low = -1;
   int high = 65536;
   int i;
   for (i=0; i < n; ++i) {
      if (x[i] > low  && x[i] < x[n]) { *plow  = i; low = x[i]; }
      if (x[i] < high && x[i] > x[n]) { *phigh = i; high = x[i]; }
   }
}

// this has been repurposed so y is now the original index instead of y
typedef struct
{
   uint16 x,id;
} stbv__floorOrdering;

static int STBV_CDECL pointCompare(const void *p, const void *q)
{
   stbv__floorOrdering *a = (stbv__floorOrdering *) p;
   stbv__floorOrdering *b = (stbv__floorOrdering *) q;
   return a->x < b->x ? -1 : a->x > b->x;
}

//
/////////////////////// END LEAF SETUP FUNCTIONS //////////////////////////


#if defined(STB_VORBIS_NO_STDIO)
   #define USE_MEMORY(z)    TRUE
#else
   #define USE_MEMORY(z)    ((z)->stream)
#endif

static uint8 get8(vorb *z)
{
   if (USE_MEMORY(z)) {
      if (z->stream >= z->streamEnd) { z->eof = TRUE; return 0; }
      return *z->stream++;
   }

   #ifndef STB_VORBIS_NO_STDIO
   {
   int c = fgetc(z->f);
   if (c == EOF) { z->eof = TRUE; return 0; }
   return c;
   }
   #endif
}

static uint32 get32(vorb *f)
{
   uint32 x;
   x = get8(f);
   x += get8(f) << 8;
   x += get8(f) << 16;
   x += (uint32) get8(f) << 24;
   return x;
}

static int getn(vorb *z, uint8 *data, int n)
{
   if (USE_MEMORY(z)) {
      if (z->stream+n > z->streamEnd) { z->eof = 1; return 0; }
      memcpy(data, z->stream, n);
      z->stream += n;
      return 1;
   }

   #ifndef STB_VORBIS_NO_STDIO
   if (fread(data, n, 1, z->f) == 1)
      return 1;
   else {
      z->eof = 1;
      return 0;
   }
   #endif
}

static void skip(vorb *z, int n)
{
   if (USE_MEMORY(z)) {
      z->stream += n;
      if (z->stream >= z->streamEnd) z->eof = 1;
      return;
   }
   #ifndef STB_VORBIS_NO_STDIO
   {
      long x = ftell(z->f);
      fseek(z->f, x+n, SEEK_SET);
   }
   #endif
}

static int setFileOffset(stbVorbis *f, unsigned int loc)
{
   #ifndef STB_VORBIS_NO_PUSHDATA_API
   if (f->pushMode) return 0;
   #endif
   f->eof = 0;
   if (USE_MEMORY(f)) {
      if (f->streamStart + loc >= f->streamEnd || f->streamStart + loc < f->streamStart) {
         f->stream = f->streamEnd;
         f->eof = 1;
         return 0;
      } else {
         f->stream = f->streamStart + loc;
         return 1;
      }
   }
   #ifndef STB_VORBIS_NO_STDIO
   if (loc + f->fStart < loc || loc >= 0x80000000) {
      loc = 0x7fffffff;
      f->eof = 1;
   } else {
      loc += f->fStart;
   }
   if (!fseek(f->f, loc, SEEK_SET))
      return 1;
   f->eof = 1;
   fseek(f->f, f->fStart, SEEK_END);
   return 0;
   #endif
}


static uint8 oggPageHeader[4] = { 0x4f, 0x67, 0x67, 0x53 };

static int capturePattern(vorb *f)
{
   if (0x4f != get8(f)) return FALSE;
   if (0x67 != get8(f)) return FALSE;
   if (0x67 != get8(f)) return FALSE;
   if (0x53 != get8(f)) return FALSE;
   return TRUE;
}

#define PAGEFLAG_continuedPacket   1
#define PAGEFLAG_firstPage         2
#define PAGEFLAG_lastPage          4

static int startPageNoCapturepattern(vorb *f)
{
   uint32 loc0,loc1,n;
   if (f->firstDecode && !IS_PUSH_MODE(f)) {
      f->pFirst.pageStart = stbVorbisGetFileOffset(f) - 4;
   }
   // stream structure version
   if (0 != get8(f)) return stbVorbisError(f, VORBIS_invalidStreamStructureVersion);
   // header flag
   f->pageFlag = get8(f);
   // absolute granule position
   loc0 = get32(f);
   loc1 = get32(f);
   // @TODO: validate loc0,loc1 as valid positions?
   // stream serial number -- vorbis doesn't interleave, so discard
   get32(f);
   //if (f->serial != get32(f)) return stbVorbisError(f, VORBIS_incorrectStreamSerialNumber);
   // page sequence number
   n = get32(f);
   f->lastPage = n;
   // CRC32
   get32(f);
   // pageSegments
   f->segmentCount = get8(f);
   if (!getn(f, f->segments, f->segmentCount))
      return stbVorbisError(f, VORBIS_unexpectedEof);
   // assume we _don't_ know any the sample position of any segments
   f->endSegWithKnownLoc = -2;
   if (loc0 != ~0U || loc1 != ~0U) {
      int i;
      // determine which packet is the last one that will complete
      for (i=f->segmentCount-1; i >= 0; --i)
         if (f->segments[i] < 255)
            break;
      // 'i' is now the index of the _last_ segment of a packet that ends
      if (i >= 0) {
         f->endSegWithKnownLoc = i;
         f->knownLocForPacket   = loc0;
      }
   }
   if (f->firstDecode) {
      int i,len;
      len = 0;
      for (i=0; i < f->segmentCount; ++i)
         len += f->segments[i];
      len += 27 + f->segmentCount;
      f->pFirst.pageEnd = f->pFirst.pageStart + len;
      f->pFirst.lastDecodedSample = loc0;
   }
   f->nextSeg = 0;
   return TRUE;
}

static int startPage(vorb *f)
{
   if (!capturePattern(f)) return stbVorbisError(f, VORBIS_missingCapturePattern);
   return startPageNoCapturepattern(f);
}

static int startPacket(vorb *f)
{
   while (f->nextSeg == -1) {
      if (!startPage(f)) return FALSE;
      if (f->pageFlag & PAGEFLAG_continuedPacket)
         return stbVorbisError(f, VORBIS_continuedPacketFlagInvalid);
   }
   f->lastSeg = FALSE;
   f->validBits = 0;
   f->packetBytes = 0;
   f->bytesInSeg = 0;
   // f->nextSeg is now valid
   return TRUE;
}

static int maybeStartPacket(vorb *f)
{
   if (f->nextSeg == -1) {
      int x = get8(f);
      if (f->eof) return FALSE; // EOF at page boundary is not an error!
      if (0x4f != x      ) return stbVorbisError(f, VORBIS_missingCapturePattern);
      if (0x67 != get8(f)) return stbVorbisError(f, VORBIS_missingCapturePattern);
      if (0x67 != get8(f)) return stbVorbisError(f, VORBIS_missingCapturePattern);
      if (0x53 != get8(f)) return stbVorbisError(f, VORBIS_missingCapturePattern);
      if (!startPageNoCapturepattern(f)) return FALSE;
      if (f->pageFlag & PAGEFLAG_continuedPacket) {
         // set up enough state that we can read this packet if we want,
         // e.g. during recovery
         f->lastSeg = FALSE;
         f->bytesInSeg = 0;
         return stbVorbisError(f, VORBIS_continuedPacketFlagInvalid);
      }
   }
   return startPacket(f);
}

static int nextSegment(vorb *f)
{
   int len;
   if (f->lastSeg) return 0;
   if (f->nextSeg == -1) {
      f->lastSegWhich = f->segmentCount-1; // in case startPage fails
      if (!startPage(f)) { f->lastSeg = 1; return 0; }
      if (!(f->pageFlag & PAGEFLAG_continuedPacket)) return stbVorbisError(f, VORBIS_continuedPacketFlagInvalid);
   }
   len = f->segments[f->nextSeg++];
   if (len < 255) {
      f->lastSeg = TRUE;
      f->lastSegWhich = f->nextSeg-1;
   }
   if (f->nextSeg >= f->segmentCount)
      f->nextSeg = -1;
   assert(f->bytesInSeg == 0);
   f->bytesInSeg = len;
   return len;
}

#define EOP    (-1)
#define INVALID_BITS  (-1)

static int get8_packetRaw(vorb *f)
{
   if (!f->bytesInSeg) {  // CLANG!
      if (f->lastSeg) return EOP;
      else if (!nextSegment(f)) return EOP;
   }
   assert(f->bytesInSeg > 0);
   --f->bytesInSeg;
   ++f->packetBytes;
   return get8(f);
}

static int get8_packet(vorb *f)
{
   int x = get8_packetRaw(f);
   f->validBits = 0;
   return x;
}

static void flushPacket(vorb *f)
{
   while (get8_packetRaw(f) != EOP);
}

// @OPTIMIZE: this is the secondary bit decoder, so it's probably not as important
// as the huffman decoder?
static uint32 getBits(vorb *f, int n)
{
   uint32 z;

   if (f->validBits < 0) return 0;
   if (f->validBits < n) {
      if (n > 24) {
         // the accumulator technique below would not work correctly in this case
         z = getBits(f, 24);
         z += getBits(f, n-24) << 24;
         return z;
      }
      if (f->validBits == 0) f->acc = 0;
      while (f->validBits < n) {
         int z = get8_packetRaw(f);
         if (z == EOP) {
            f->validBits = INVALID_BITS;
            return 0;
         }
         f->acc += z << f->validBits;
         f->validBits += 8;
      }
   }

   assert(f->validBits >= n);
   z = f->acc & ((1 << n)-1);
   f->acc >>= n;
   f->validBits -= n;
   return z;
}

// @OPTIMIZE: primary accumulator for huffman
// expand the buffer to as many bits as possible without reading off end of packet
// it might be nice to allow f->validBits and f->acc to be stored in registers,
// e.g. cache them locally and decode locally
STB_FORCEINLINE void prepHuffman(vorb *f)
{
   if (f->validBits <= 24) {
      if (f->validBits == 0) f->acc = 0;
      do {
         int z;
         if (f->lastSeg && !f->bytesInSeg) return;
         z = get8_packetRaw(f);
         if (z == EOP) return;
         f->acc += (unsigned) z << f->validBits;
         f->validBits += 8;
      } while (f->validBits <= 24);
   }
}

enum
{
   VORBIS_packetId = 1,
   VORBIS_packetComment = 3,
   VORBIS_packetSetup = 5
};

static int codebookDecodeScalarRaw(vorb *f, Codebook *c)
{
   int i;
   prepHuffman(f);

   if (c->codewords == NULL && c->sortedCodewords == NULL)
      return -1;

   // cases to use binary search: sortedCodewords && !c->codewords
   //                             sortedCodewords && c->entries > 8
   if (c->entries > 8 ? c->sortedCodewords!=NULL : !c->codewords) {
      // binary search
      uint32 code = bitReverse(f->acc);
      int x=0, n=c->sortedEntries, len;

      while (n > 1) {
         // invariant: sc[x] <= code < sc[x+n]
         int m = x + (n >> 1);
         if (c->sortedCodewords[m] <= code) {
            x = m;
            n -= (n>>1);
         } else {
            n >>= 1;
         }
      }
      // x is now the sorted index
      if (!c->sparse) x = c->sortedValues[x];
      // x is now sorted index if sparse, or symbol otherwise
      len = c->codewordLengths[x];
      if (f->validBits >= len) {
         f->acc >>= len;
         f->validBits -= len;
         return x;
      }

      f->validBits = 0;
      return -1;
   }

   // if small, linear search
   assert(!c->sparse);
   for (i=0; i < c->entries; ++i) {
      if (c->codewordLengths[i] == NO_CODE) continue;
      if (c->codewords[i] == (f->acc & ((1 << c->codewordLengths[i])-1))) {
         if (f->validBits >= c->codewordLengths[i]) {
            f->acc >>= c->codewordLengths[i];
            f->validBits -= c->codewordLengths[i];
            return i;
         }
         f->validBits = 0;
         return -1;
      }
   }

   stbVorbisError(f, VORBIS_invalidStream);
   f->validBits = 0;
   return -1;
}

#ifndef STB_VORBIS_NO_INLINE_DECODE

#define DECODE_RAW(var, f,c)                                  \
   if (f->validBits < STB_VORBIS_FAST_HUFFMAN_LENGTH)        \
      prepHuffman(f);                                        \
   var = f->acc & FAST_HUFFMAN_TABLE_MASK;                    \
   var = c->fastHuffman[var];                                \
   if (var >= 0) {                                            \
      int n = c->codewordLengths[var];                       \
      f->acc >>= n;                                           \
      f->validBits -= n;                                     \
      if (f->validBits < 0) { f->validBits = 0; var = -1; } \
   } else {                                                   \
      var = codebookDecodeScalarRaw(f,c);                  \
   }

#else

static int codebookDecodeScalar(vorb *f, Codebook *c)
{
   int i;
   if (f->validBits < STB_VORBIS_FAST_HUFFMAN_LENGTH)
      prepHuffman(f);
   // fast huffman table lookup
   i = f->acc & FAST_HUFFMAN_TABLE_MASK;
   i = c->fastHuffman[i];
   if (i >= 0) {
      f->acc >>= c->codewordLengths[i];
      f->validBits -= c->codewordLengths[i];
      if (f->validBits < 0) { f->validBits = 0; return -1; }
      return i;
   }
   return codebookDecodeScalarRaw(f,c);
}

#define DECODE_RAW(var,f,c)    var = codebookDecodeScalar(f,c);

#endif

#define DECODE(var,f,c)                                       \
   DECODE_RAW(var,f,c)                                        \
   if (c->sparse) var = c->sortedValues[var];

#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
  #define DECODE_VQ(var,f,c)   DECODE_RAW(var,f,c)
#else
  #define DECODE_VQ(var,f,c)   DECODE(var,f,c)
#endif






// CODEBOOK_ELEMENT_FAST is an optimization for the CODEBOOK_FLOATS case
// where we avoid one addition
#define CODEBOOK_ELEMENT(c,off)          (c->multiplicands[off])
#define CODEBOOK_ELEMENT_FAST(c,off)     (c->multiplicands[off])
#define CODEBOOK_ELEMENT_BASE(c)         (0)

static int codebookDecodeStart(vorb *f, Codebook *c)
{
   int z = -1;

   // type 0 is only legal in a scalar context
   if (c->lookupType == 0)
      stbVorbisError(f, VORBIS_invalidStream);
   else {
      DECODE_VQ(z,f,c);
      if (c->sparse) assert(z < c->sortedEntries);
      if (z < 0) {  // check for EOP
         if (!f->bytesInSeg)
            if (f->lastSeg)
               return z;
         stbVorbisError(f, VORBIS_invalidStream);
      }
   }
   return z;
}

static int codebookDecode(vorb *f, Codebook *c, float *output, int len)
{
   int i,z = codebookDecodeStart(f,c);
   if (z < 0) return FALSE;
   if (len > c->dimensions) len = c->dimensions;

#ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
   if (c->lookupType == 1) {
      float last = CODEBOOK_ELEMENT_BASE(c);
      int div = 1;
      for (i=0; i < len; ++i) {
         int off = (z / div) % c->lookupValues;
         float val = CODEBOOK_ELEMENT_FAST(c,off) + last;
         output[i] += val;
         if (c->sequenceP) last = val + c->minimumValue;
         div *= c->lookupValues;
      }
      return TRUE;
   }
#endif

   z *= c->dimensions;
   if (c->sequenceP) {
      float last = CODEBOOK_ELEMENT_BASE(c);
      for (i=0; i < len; ++i) {
         float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
         output[i] += val;
         last = val + c->minimumValue;
      }
   } else {
      float last = CODEBOOK_ELEMENT_BASE(c);
      for (i=0; i < len; ++i) {
         output[i] += CODEBOOK_ELEMENT_FAST(c,z+i) + last;
      }
   }

   return TRUE;
}

static int codebookDecodeStep(vorb *f, Codebook *c, float *output, int len, int step)
{
   int i,z = codebookDecodeStart(f,c);
   float last = CODEBOOK_ELEMENT_BASE(c);
   if (z < 0) return FALSE;
   if (len > c->dimensions) len = c->dimensions;

#ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
   if (c->lookupType == 1) {
      int div = 1;
      for (i=0; i < len; ++i) {
         int off = (z / div) % c->lookupValues;
         float val = CODEBOOK_ELEMENT_FAST(c,off) + last;
         output[i*step] += val;
         if (c->sequenceP) last = val;
         div *= c->lookupValues;
      }
      return TRUE;
   }
#endif

   z *= c->dimensions;
   for (i=0; i < len; ++i) {
      float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
      output[i*step] += val;
      if (c->sequenceP) last = val;
   }

   return TRUE;
}

static int codebookDecodeDeinterleaveRepeat(vorb *f, Codebook *c, float **outputs, int ch, int *cInterP, int *pInterP, int len, int totalDecode)
{
   int cInter = *cInterP;
   int pInter = *pInterP;
   int i,z, effective = c->dimensions;

   // type 0 is only legal in a scalar context
   if (c->lookupType == 0)   return stbVorbisError(f, VORBIS_invalidStream);

   while (totalDecode > 0) {
      float last = CODEBOOK_ELEMENT_BASE(c);
      DECODE_VQ(z,f,c);
      #ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
      assert(!c->sparse || z < c->sortedEntries);
      #endif
      if (z < 0) {
         if (!f->bytesInSeg)
            if (f->lastSeg) return FALSE;
         return stbVorbisError(f, VORBIS_invalidStream);
      }

      // if this will take us off the end of the buffers, stop short!
      // we check by computing the length of the virtual interleaved
      // buffer (len*ch), our current offset within it (pInter*ch)+(cInter),
      // and the length we'll be using (effective)
      if (cInter + pInter*ch + effective > len * ch) {
         effective = len*ch - (pInter*ch - cInter);
      }

   #ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
      if (c->lookupType == 1) {
         int div = 1;
         for (i=0; i < effective; ++i) {
            int off = (z / div) % c->lookupValues;
            float val = CODEBOOK_ELEMENT_FAST(c,off) + last;
            if (outputs[cInter])
               outputs[cInter][pInter] += val;
            if (++cInter == ch) { cInter = 0; ++pInter; }
            if (c->sequenceP) last = val;
            div *= c->lookupValues;
         }
      } else
   #endif
      {
         z *= c->dimensions;
         if (c->sequenceP) {
            for (i=0; i < effective; ++i) {
               float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
               if (outputs[cInter])
                  outputs[cInter][pInter] += val;
               if (++cInter == ch) { cInter = 0; ++pInter; }
               last = val;
            }
         } else {
            for (i=0; i < effective; ++i) {
               float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
               if (outputs[cInter])
                  outputs[cInter][pInter] += val;
               if (++cInter == ch) { cInter = 0; ++pInter; }
            }
         }
      }

      totalDecode -= effective;
   }
   *cInterP = cInter;
   *pInterP = pInter;
   return TRUE;
}

static int predictPoint(int x, int x0, int x1, int y0, int y1)
{
   int dy = y1 - y0;
   int adx = x1 - x0;
   // @OPTIMIZE: force int division to round in the right direction... is this necessary on x86?
   int err = abs(dy) * (x - x0);
   int off = err / adx;
   return dy < 0 ? y0 - off : y0 + off;
}

// the following table is block-copied from the specification
static float inverseDbTable[256] =
{
  1.0649863e-07f, 1.1341951e-07f, 1.2079015e-07f, 1.2863978e-07f,
  1.3699951e-07f, 1.4590251e-07f, 1.5538408e-07f, 1.6548181e-07f,
  1.7623575e-07f, 1.8768855e-07f, 1.9988561e-07f, 2.1287530e-07f,
  2.2670913e-07f, 2.4144197e-07f, 2.5713223e-07f, 2.7384213e-07f,
  2.9163793e-07f, 3.1059021e-07f, 3.3077411e-07f, 3.5226968e-07f,
  3.7516214e-07f, 3.9954229e-07f, 4.2550680e-07f, 4.5315863e-07f,
  4.8260743e-07f, 5.1396998e-07f, 5.4737065e-07f, 5.8294187e-07f,
  6.2082472e-07f, 6.6116941e-07f, 7.0413592e-07f, 7.4989464e-07f,
  7.9862701e-07f, 8.5052630e-07f, 9.0579828e-07f, 9.6466216e-07f,
  1.0273513e-06f, 1.0941144e-06f, 1.1652161e-06f, 1.2409384e-06f,
  1.3215816e-06f, 1.4074654e-06f, 1.4989305e-06f, 1.5963394e-06f,
  1.7000785e-06f, 1.8105592e-06f, 1.9282195e-06f, 2.0535261e-06f,
  2.1869758e-06f, 2.3290978e-06f, 2.4804557e-06f, 2.6416497e-06f,
  2.8133190e-06f, 2.9961443e-06f, 3.1908506e-06f, 3.3982101e-06f,
  3.6190449e-06f, 3.8542308e-06f, 4.1047004e-06f, 4.3714470e-06f,
  4.6555282e-06f, 4.9580707e-06f, 5.2802740e-06f, 5.6234160e-06f,
  5.9888572e-06f, 6.3780469e-06f, 6.7925283e-06f, 7.2339451e-06f,
  7.7040476e-06f, 8.2047000e-06f, 8.7378876e-06f, 9.3057248e-06f,
  9.9104632e-06f, 1.0554501e-05f, 1.1240392e-05f, 1.1970856e-05f,
  1.2748789e-05f, 1.3577278e-05f, 1.4459606e-05f, 1.5399272e-05f,
  1.6400004e-05f, 1.7465768e-05f, 1.8600792e-05f, 1.9809576e-05f,
  2.1096914e-05f, 2.2467911e-05f, 2.3928002e-05f, 2.5482978e-05f,
  2.7139006e-05f, 2.8902651e-05f, 3.0780908e-05f, 3.2781225e-05f,
  3.4911534e-05f, 3.7180282e-05f, 3.9596466e-05f, 4.2169667e-05f,
  4.4910090e-05f, 4.7828601e-05f, 5.0936773e-05f, 5.4246931e-05f,
  5.7772202e-05f, 6.1526565e-05f, 6.5524908e-05f, 6.9783085e-05f,
  7.4317983e-05f, 7.9147585e-05f, 8.4291040e-05f, 8.9768747e-05f,
  9.5602426e-05f, 0.00010181521f, 0.00010843174f, 0.00011547824f,
  0.00012298267f, 0.00013097477f, 0.00013948625f, 0.00014855085f,
  0.00015820453f, 0.00016848555f, 0.00017943469f, 0.00019109536f,
  0.00020351382f, 0.00021673929f, 0.00023082423f, 0.00024582449f,
  0.00026179955f, 0.00027881276f, 0.00029693158f, 0.00031622787f,
  0.00033677814f, 0.00035866388f, 0.00038197188f, 0.00040679456f,
  0.00043323036f, 0.00046138411f, 0.00049136745f, 0.00052329927f,
  0.00055730621f, 0.00059352311f, 0.00063209358f, 0.00067317058f,
  0.00071691700f, 0.00076350630f, 0.00081312324f, 0.00086596457f,
  0.00092223983f, 0.00098217216f, 0.0010459992f,  0.0011139742f,
  0.0011863665f,  0.0012634633f,  0.0013455702f,  0.0014330129f,
  0.0015261382f,  0.0016253153f,  0.0017309374f,  0.0018434235f,
  0.0019632195f,  0.0020908006f,  0.0022266726f,  0.0023713743f,
  0.0025254795f,  0.0026895994f,  0.0028643847f,  0.0030505286f,
  0.0032487691f,  0.0034598925f,  0.0036847358f,  0.0039241906f,
  0.0041792066f,  0.0044507950f,  0.0047400328f,  0.0050480668f,
  0.0053761186f,  0.0057254891f,  0.0060975636f,  0.0064938176f,
  0.0069158225f,  0.0073652516f,  0.0078438871f,  0.0083536271f,
  0.0088964928f,  0.009474637f,   0.010090352f,   0.010746080f,
  0.011444421f,   0.012188144f,   0.012980198f,   0.013823725f,
  0.014722068f,   0.015678791f,   0.016697687f,   0.017782797f,
  0.018938423f,   0.020169149f,   0.021479854f,   0.022875735f,
  0.024362330f,   0.025945531f,   0.027631618f,   0.029427276f,
  0.031339626f,   0.033376252f,   0.035545228f,   0.037855157f,
  0.040315199f,   0.042935108f,   0.045725273f,   0.048696758f,
  0.051861348f,   0.055231591f,   0.058820850f,   0.062643361f,
  0.066714279f,   0.071049749f,   0.075666962f,   0.080584227f,
  0.085821044f,   0.091398179f,   0.097337747f,   0.10366330f,
  0.11039993f,    0.11757434f,    0.12521498f,    0.13335215f,
  0.14201813f,    0.15124727f,    0.16107617f,    0.17154380f,
  0.18269168f,    0.19456402f,    0.20720788f,    0.22067342f,
  0.23501402f,    0.25028656f,    0.26655159f,    0.28387361f,
  0.30232132f,    0.32196786f,    0.34289114f,    0.36517414f,
  0.38890521f,    0.41417847f,    0.44109412f,    0.46975890f,
  0.50028648f,    0.53279791f,    0.56742212f,    0.60429640f,
  0.64356699f,    0.68538959f,    0.72993007f,    0.77736504f,
  0.82788260f,    0.88168307f,    0.9389798f,     1.0f
};


// @OPTIMIZE: if you want to replace this bresenham line-drawing routine,
// note that you must produce bit-identical output to decode correctly;
// this specific sequence of operations is specified in the spec (it's
// drawing integer-quantized frequency-space lines that the encoder
// expects to be exactly the same)
//     ... also, isn't the whole point of Bresenham's algorithm to NOT
// have to divide in the setup? sigh.
#ifndef STB_VORBIS_NO_DEFER_FLOOR
#define LINE_OP(a,b)   a *= b
#else
#define LINE_OP(a,b)   a = b
#endif

#ifdef STB_VORBIS_DIVIDE_TABLE
#define DIVTAB_NUMER   32
#define DIVTAB_DENOM   64
int8 integerDivideTable[DIVTAB_NUMER][DIVTAB_DENOM]; // 2KB
#endif

STB_FORCEINLINE void drawLine(float *output, int x0, int y0, int x1, int y1, int n)
{
   int dy = y1 - y0;
   int adx = x1 - x0;
   int ady = abs(dy);
   int base;
   int x=x0,y=y0;
   int err = 0;
   int sy;

#ifdef STB_VORBIS_DIVIDE_TABLE
   if (adx < DIVTAB_DENOM && ady < DIVTAB_NUMER) {
      if (dy < 0) {
         base = -integerDivideTable[ady][adx];
         sy = base-1;
      } else {
         base =  integerDivideTable[ady][adx];
         sy = base+1;
      }
   } else {
      base = dy / adx;
      if (dy < 0)
         sy = base - 1;
      else
         sy = base+1;
   }
#else
   base = dy / adx;
   if (dy < 0)
      sy = base - 1;
   else
      sy = base+1;
#endif
   ady -= abs(base) * adx;
   if (x1 > n) x1 = n;
   if (x < x1) {
      LINE_OP(output[x], inverseDbTable[y&255]);
      for (++x; x < x1; ++x) {
         err += ady;
         if (err >= adx) {
            err -= adx;
            y += sy;
         } else
            y += base;
         LINE_OP(output[x], inverseDbTable[y&255]);
      }
   }
}

static int residueDecode(vorb *f, Codebook *book, float *target, int offset, int n, int rtype)
{
   int k;
   if (rtype == 0) {
      int step = n / book->dimensions;
      for (k=0; k < step; ++k)
         if (!codebookDecodeStep(f, book, target+offset+k, n-offset-k, step))
            return FALSE;
   } else {
      for (k=0; k < n; ) {
         if (!codebookDecode(f, book, target+offset, n-k))
            return FALSE;
         k += book->dimensions;
         offset += book->dimensions;
      }
   }
   return TRUE;
}

// n is 1/2 of the blocksize --
// specification: "Correct per-vector decode length is [n]/2"
static void decodeResidue(vorb *f, float *residueBuffers[], int ch, int n, int rn, uint8 *doNotDecode)
{
   int i,j,pass;
   Residue *r = f->residueConfig + rn;
   int rtype = f->residueTypes[rn];
   int c = r->classbook;
   int classwords = f->codebooks[c].dimensions;
   unsigned int actualSize = rtype == 2 ? n*2 : n;
   unsigned int limitR_begin = (r->begin < actualSize ? r->begin : actualSize);
   unsigned int limitR_end   = (r->end   < actualSize ? r->end   : actualSize);
   int nRead = limitR_end - limitR_begin;
   int partRead = nRead / r->partSize;
   int tempAllocPoint = tempAllocSave(f);
   #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
   uint8 ***partClassdata = (uint8 ***) tempBlockArray(f,f->channels, partRead * sizeof(**partClassdata));
   #else
   int **classifications = (int **) tempBlockArray(f,f->channels, partRead * sizeof(**classifications));
   #endif

   CHECK(f);

   for (i=0; i < ch; ++i)
      if (!doNotDecode[i])
         memset(residueBuffers[i], 0, sizeof(float) * n);

   if (rtype == 2 && ch != 1) {
      for (j=0; j < ch; ++j)
         if (!doNotDecode[j])
            break;
      if (j == ch)
         goto done;

      for (pass=0; pass < 8; ++pass) {
         int pcount = 0, classSet = 0;
         if (ch == 2) {
            while (pcount < partRead) {
               int z = r->begin + pcount*r->partSize;
               int cInter = (z & 1), pInter = z>>1;
               if (pass == 0) {
                  Codebook *c = f->codebooks+r->classbook;
                  int q;
                  DECODE(q,f,c);
                  if (q == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  partClassdata[0][classSet] = r->classdata[q];
                  #else
                  for (i=classwords-1; i >= 0; --i) {
                     classifications[0][i+pcount] = q % r->classifications;
                     q /= r->classifications;
                  }
                  #endif
               }
               for (i=0; i < classwords && pcount < partRead; ++i, ++pcount) {
                  int z = r->begin + pcount*r->partSize;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = partClassdata[0][classSet][i];
                  #else
                  int c = classifications[0][pcount];
                  #endif
                  int b = r->residueBooks[c][pass];
                  if (b >= 0) {
                     Codebook *book = f->codebooks + b;
                     #ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
                     if (!codebookDecodeDeinterleaveRepeat(f, book, residueBuffers, ch, &cInter, &pInter, n, r->partSize))
                        goto done;
                     #else
                     // saves 1%
                     if (!codebookDecodeDeinterleaveRepeat(f, book, residueBuffers, ch, &cInter, &pInter, n, r->partSize))
                        goto done;
                     #endif
                  } else {
                     z += r->partSize;
                     cInter = z & 1;
                     pInter = z >> 1;
                  }
               }
               #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
               ++classSet;
               #endif
            }
         } else if (ch > 2) {
            while (pcount < partRead) {
               int z = r->begin + pcount*r->partSize;
               int cInter = z % ch, pInter = z/ch;
               if (pass == 0) {
                  Codebook *c = f->codebooks+r->classbook;
                  int q;
                  DECODE(q,f,c);
                  if (q == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  partClassdata[0][classSet] = r->classdata[q];
                  #else
                  for (i=classwords-1; i >= 0; --i) {
                     classifications[0][i+pcount] = q % r->classifications;
                     q /= r->classifications;
                  }
                  #endif
               }
               for (i=0; i < classwords && pcount < partRead; ++i, ++pcount) {
                  int z = r->begin + pcount*r->partSize;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = partClassdata[0][classSet][i];
                  #else
                  int c = classifications[0][pcount];
                  #endif
                  int b = r->residueBooks[c][pass];
                  if (b >= 0) {
                     Codebook *book = f->codebooks + b;
                     if (!codebookDecodeDeinterleaveRepeat(f, book, residueBuffers, ch, &cInter, &pInter, n, r->partSize))
                        goto done;
                  } else {
                     z += r->partSize;
                     cInter = z % ch;
                     pInter = z / ch;
                  }
               }
               #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
               ++classSet;
               #endif
            }
         }
      }
      goto done;
   }
   CHECK(f);

   for (pass=0; pass < 8; ++pass) {
      int pcount = 0, classSet=0;
      while (pcount < partRead) {
         if (pass == 0) {
            for (j=0; j < ch; ++j) {
               if (!doNotDecode[j]) {
                  Codebook *c = f->codebooks+r->classbook;
                  int temp;
                  DECODE(temp,f,c);
                  if (temp == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  partClassdata[j][classSet] = r->classdata[temp];
                  #else
                  for (i=classwords-1; i >= 0; --i) {
                     classifications[j][i+pcount] = temp % r->classifications;
                     temp /= r->classifications;
                  }
                  #endif
               }
            }
         }
         for (i=0; i < classwords && pcount < partRead; ++i, ++pcount) {
            for (j=0; j < ch; ++j) {
               if (!doNotDecode[j]) {
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = partClassdata[j][classSet][i];
                  #else
                  int c = classifications[j][pcount];
                  #endif
                  int b = r->residueBooks[c][pass];
                  if (b >= 0) {
                     float *target = residueBuffers[j];
                     int offset = r->begin + pcount * r->partSize;
                     int n = r->partSize;
                     Codebook *book = f->codebooks + b;
                     if (!residueDecode(f, book, target, offset, n, rtype))
                        goto done;
                  }
               }
            }
         }
         #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
         ++classSet;
         #endif
      }
   }
  done:
   CHECK(f);
   #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
   tempFree(f,partClassdata);
   #else
   tempFree(f,classifications);
   #endif
   tempAllocRestore(f,tempAllocPoint);
}


#if 0
// slow way for debugging
void inverseMdctSlow(float *buffer, int n)
{
   int i,j;
   int n2 = n >> 1;
   float *x = (float *) malloc(sizeof(*x) * n2);
   memcpy(x, buffer, sizeof(*x) * n2);
   for (i=0; i < n; ++i) {
      float acc = 0;
      for (j=0; j < n2; ++j)
         // formula from paper:
         //acc += n/4.0f * x[j] * (float) cos(M_PI / 2 / n * (2 * i + 1 + n/2.0)*(2*j+1));
         // formula from wikipedia
         //acc += 2.0f / n2 * x[j] * (float) cos(M_PI/n2 * (i + 0.5 + n2/2)*(j + 0.5));
         // these are equivalent, except the formula from the paper inverts the multiplier!
         // however, what actually works is NO MULTIPLIER!?!
         //acc += 64 * 2.0f / n2 * x[j] * (float) cos(M_PI/n2 * (i + 0.5 + n2/2)*(j + 0.5));
         acc += x[j] * (float) cos(M_PI / 2 / n * (2 * i + 1 + n/2.0)*(2*j+1));
      buffer[i] = acc;
   }
   free(x);
}
#elif 0
// same as above, but just barely able to run in real time on modern machines
void inverseMdctSlow(float *buffer, int n, vorb *f, int blocktype)
{
   float mcos[16384];
   int i,j;
   int n2 = n >> 1, nmask = (n << 2) -1;
   float *x = (float *) malloc(sizeof(*x) * n2);
   memcpy(x, buffer, sizeof(*x) * n2);
   for (i=0; i < 4*n; ++i)
      mcos[i] = (float) cos(M_PI / 2 * i / n);

   for (i=0; i < n; ++i) {
      float acc = 0;
      for (j=0; j < n2; ++j)
         acc += x[j] * mcos[(2 * i + 1 + n2)*(2*j+1) & nmask];
      buffer[i] = acc;
   }
   free(x);
}
#elif 0
// transform to use a slow dct-iv; this is STILL basically trivial,
// but only requires half as many ops
void dctIvSlow(float *buffer, int n)
{
   float mcos[16384];
   float x[2048];
   int i,j;
   int n2 = n >> 1, nmask = (n << 3) - 1;
   memcpy(x, buffer, sizeof(*x) * n);
   for (i=0; i < 8*n; ++i)
      mcos[i] = (float) cos(M_PI / 4 * i / n);
   for (i=0; i < n; ++i) {
      float acc = 0;
      for (j=0; j < n; ++j)
         acc += x[j] * mcos[((2 * i + 1)*(2*j+1)) & nmask];
      buffer[i] = acc;
   }
}

void inverseMdctSlow(float *buffer, int n, vorb *f, int blocktype)
{
   int i, n4 = n >> 2, n2 = n >> 1, n3_4 = n - n4;
   float temp[4096];

   memcpy(temp, buffer, n2 * sizeof(float));
   dctIvSlow(temp, n2);  // returns -c'-d, a-b'

   for (i=0; i < n4  ; ++i) buffer[i] = temp[i+n4];            // a-b'
   for (   ; i < n3_4; ++i) buffer[i] = -temp[n3_4 - i - 1];   // b-a', c+d'
   for (   ; i < n   ; ++i) buffer[i] = -temp[i - n3_4];       // c'+d
}
#endif

#ifndef LIBVORBIS_MDCT
#define LIBVORBIS_MDCT 0
#endif

#if LIBVORBIS_MDCT
// directly call the vorbis MDCT using an interface documented
// by Jeff Roberts... useful for performance comparison
typedef struct
{
  int n;
  int log2n;

  float *trig;
  int   *bitrev;

  float scale;
} mdctLookup;

extern void mdctInit(mdctLookup *lookup, int n);
extern void mdctClear(mdctLookup *l);
extern void mdctBackward(mdctLookup *init, float *in, float *out);

mdctLookup M1,M2;

void inverseMdct(float *buffer, int n, vorb *f, int blocktype)
{
   mdctLookup *M;
   if (M1.n == n) M = &M1;
   else if (M2.n == n) M = &M2;
   else if (M1.n == 0) { mdctInit(&M1, n); M = &M1; }
   else {
      if (M2.n) __asm int 3;
      mdctInit(&M2, n);
      M = &M2;
   }

   mdctBackward(M, buffer, buffer);
}
#endif


// the following were split out into separate functions while optimizing;
// they could be pushed back up but eh. __forceinline showed no change;
// they're probably already being inlined.
static void imdctStep3_iter0_loop(int n, float *e, int iOff, int kOff, float *A)
{
   float *ee0 = e + iOff;
   float *ee2 = ee0 + kOff;
   int i;

   assert((n & 3) == 0);
   for (i=(n>>2); i > 0; --i) {
      float k00_20, k01_21;
      k00_20  = ee0[ 0] - ee2[ 0];
      k01_21  = ee0[-1] - ee2[-1];
      ee0[ 0] += ee2[ 0];//ee0[ 0] = ee0[ 0] + ee2[ 0];
      ee0[-1] += ee2[-1];//ee0[-1] = ee0[-1] + ee2[-1];
      ee2[ 0] = k00_20 * A[0] - k01_21 * A[1];
      ee2[-1] = k01_21 * A[0] + k00_20 * A[1];
      A += 8;

      k00_20  = ee0[-2] - ee2[-2];
      k01_21  = ee0[-3] - ee2[-3];
      ee0[-2] += ee2[-2];//ee0[-2] = ee0[-2] + ee2[-2];
      ee0[-3] += ee2[-3];//ee0[-3] = ee0[-3] + ee2[-3];
      ee2[-2] = k00_20 * A[0] - k01_21 * A[1];
      ee2[-3] = k01_21 * A[0] + k00_20 * A[1];
      A += 8;

      k00_20  = ee0[-4] - ee2[-4];
      k01_21  = ee0[-5] - ee2[-5];
      ee0[-4] += ee2[-4];//ee0[-4] = ee0[-4] + ee2[-4];
      ee0[-5] += ee2[-5];//ee0[-5] = ee0[-5] + ee2[-5];
      ee2[-4] = k00_20 * A[0] - k01_21 * A[1];
      ee2[-5] = k01_21 * A[0] + k00_20 * A[1];
      A += 8;

      k00_20  = ee0[-6] - ee2[-6];
      k01_21  = ee0[-7] - ee2[-7];
      ee0[-6] += ee2[-6];//ee0[-6] = ee0[-6] + ee2[-6];
      ee0[-7] += ee2[-7];//ee0[-7] = ee0[-7] + ee2[-7];
      ee2[-6] = k00_20 * A[0] - k01_21 * A[1];
      ee2[-7] = k01_21 * A[0] + k00_20 * A[1];
      A += 8;
      ee0 -= 8;
      ee2 -= 8;
   }
}

static void imdctStep3_innerR_loop(int lim, float *e, int d0, int kOff, float *A, int k1)
{
   int i;
   float k00_20, k01_21;

   float *e0 = e + d0;
   float *e2 = e0 + kOff;

   for (i=lim >> 2; i > 0; --i) {
      k00_20 = e0[-0] - e2[-0];
      k01_21 = e0[-1] - e2[-1];
      e0[-0] += e2[-0];//e0[-0] = e0[-0] + e2[-0];
      e0[-1] += e2[-1];//e0[-1] = e0[-1] + e2[-1];
      e2[-0] = (k00_20)*A[0] - (k01_21) * A[1];
      e2[-1] = (k01_21)*A[0] + (k00_20) * A[1];

      A += k1;

      k00_20 = e0[-2] - e2[-2];
      k01_21 = e0[-3] - e2[-3];
      e0[-2] += e2[-2];//e0[-2] = e0[-2] + e2[-2];
      e0[-3] += e2[-3];//e0[-3] = e0[-3] + e2[-3];
      e2[-2] = (k00_20)*A[0] - (k01_21) * A[1];
      e2[-3] = (k01_21)*A[0] + (k00_20) * A[1];

      A += k1;

      k00_20 = e0[-4] - e2[-4];
      k01_21 = e0[-5] - e2[-5];
      e0[-4] += e2[-4];//e0[-4] = e0[-4] + e2[-4];
      e0[-5] += e2[-5];//e0[-5] = e0[-5] + e2[-5];
      e2[-4] = (k00_20)*A[0] - (k01_21) * A[1];
      e2[-5] = (k01_21)*A[0] + (k00_20) * A[1];

      A += k1;

      k00_20 = e0[-6] - e2[-6];
      k01_21 = e0[-7] - e2[-7];
      e0[-6] += e2[-6];//e0[-6] = e0[-6] + e2[-6];
      e0[-7] += e2[-7];//e0[-7] = e0[-7] + e2[-7];
      e2[-6] = (k00_20)*A[0] - (k01_21) * A[1];
      e2[-7] = (k01_21)*A[0] + (k00_20) * A[1];

      e0 -= 8;
      e2 -= 8;

      A += k1;
   }
}

static void imdctStep3_innerS_loop(int n, float *e, int iOff, int kOff, float *A, int aOff, int k0)
{
   int i;
   float A0 = A[0];
   float A1 = A[0+1];
   float A2 = A[0+aOff];
   float A3 = A[0+aOff+1];
   float A4 = A[0+aOff*2+0];
   float A5 = A[0+aOff*2+1];
   float A6 = A[0+aOff*3+0];
   float A7 = A[0+aOff*3+1];

   float k00,k11;

   float *ee0 = e  +iOff;
   float *ee2 = ee0+kOff;

   for (i=n; i > 0; --i) {
      k00     = ee0[ 0] - ee2[ 0];
      k11     = ee0[-1] - ee2[-1];
      ee0[ 0] =  ee0[ 0] + ee2[ 0];
      ee0[-1] =  ee0[-1] + ee2[-1];
      ee2[ 0] = (k00) * A0 - (k11) * A1;
      ee2[-1] = (k11) * A0 + (k00) * A1;

      k00     = ee0[-2] - ee2[-2];
      k11     = ee0[-3] - ee2[-3];
      ee0[-2] =  ee0[-2] + ee2[-2];
      ee0[-3] =  ee0[-3] + ee2[-3];
      ee2[-2] = (k00) * A2 - (k11) * A3;
      ee2[-3] = (k11) * A2 + (k00) * A3;

      k00     = ee0[-4] - ee2[-4];
      k11     = ee0[-5] - ee2[-5];
      ee0[-4] =  ee0[-4] + ee2[-4];
      ee0[-5] =  ee0[-5] + ee2[-5];
      ee2[-4] = (k00) * A4 - (k11) * A5;
      ee2[-5] = (k11) * A4 + (k00) * A5;

      k00     = ee0[-6] - ee2[-6];
      k11     = ee0[-7] - ee2[-7];
      ee0[-6] =  ee0[-6] + ee2[-6];
      ee0[-7] =  ee0[-7] + ee2[-7];
      ee2[-6] = (k00) * A6 - (k11) * A7;
      ee2[-7] = (k11) * A6 + (k00) * A7;

      ee0 -= k0;
      ee2 -= k0;
   }
}

STB_FORCEINLINE void iter_54(float *z)
{
   float k00,k11,k22,k33;
   float y0,y1,y2,y3;

   k00  = z[ 0] - z[-4];
   y0   = z[ 0] + z[-4];
   y2   = z[-2] + z[-6];
   k22  = z[-2] - z[-6];

   z[-0] = y0 + y2;      // z0 + z4 + z2 + z6
   z[-2] = y0 - y2;      // z0 + z4 - z2 - z6

   // done with y0,y2

   k33  = z[-3] - z[-7];

   z[-4] = k00 + k33;    // z0 - z4 + z3 - z7
   z[-6] = k00 - k33;    // z0 - z4 - z3 + z7

   // done with k33

   k11  = z[-1] - z[-5];
   y1   = z[-1] + z[-5];
   y3   = z[-3] + z[-7];

   z[-1] = y1 + y3;      // z1 + z5 + z3 + z7
   z[-3] = y1 - y3;      // z1 + z5 - z3 - z7
   z[-5] = k11 - k22;    // z1 - z5 + z2 - z6
   z[-7] = k11 + k22;    // z1 - z5 - z2 + z6
}

static void imdctStep3_innerS_loopLd654(int n, float *e, int iOff, float *A, int baseN)
{
   int aOff = baseN >> 3;
   float A2 = A[0+aOff];
   float *z = e + iOff;
   float *base = z - 16 * n;

   while (z > base) {
      float k00,k11;

      k00   = z[-0] - z[-8];
      k11   = z[-1] - z[-9];
      z[-0] = z[-0] + z[-8];
      z[-1] = z[-1] + z[-9];
      z[-8] =  k00;
      z[-9] =  k11 ;

      k00    = z[ -2] - z[-10];
      k11    = z[ -3] - z[-11];
      z[ -2] = z[ -2] + z[-10];
      z[ -3] = z[ -3] + z[-11];
      z[-10] = (k00+k11) * A2;
      z[-11] = (k11-k00) * A2;

      k00    = z[-12] - z[ -4];  // reverse to avoid a unary negation
      k11    = z[ -5] - z[-13];
      z[ -4] = z[ -4] + z[-12];
      z[ -5] = z[ -5] + z[-13];
      z[-12] = k11;
      z[-13] = k00;

      k00    = z[-14] - z[ -6];  // reverse to avoid a unary negation
      k11    = z[ -7] - z[-15];
      z[ -6] = z[ -6] + z[-14];
      z[ -7] = z[ -7] + z[-15];
      z[-14] = (k00+k11) * A2;
      z[-15] = (k00-k11) * A2;

      iter_54(z);
      iter_54(z-8);
      z -= 16;
   }
}

static void inverseMdct(float *buffer, int n, vorb *f, int blocktype)
{
   int n2 = n >> 1, n4 = n >> 2, n8 = n >> 3, l;
   int ld;
   // @OPTIMIZE: reduce register pressure by using fewer variables?
   int savePoint = tempAllocSave(f);
   float *buf2 = (float *) tempAlloc(f, n2 * sizeof(*buf2));
   float *u=NULL,*v=NULL;
   // twiddle factors
   float *A = f->A[blocktype];

   // IMDCT algorithm from "The use of multirate filter banks for coding of high quality digital audio"
   // See notes about bugs in that paper in less-optimal implementation 'inverseMdctOld' after this function.

   // kernel from paper


   // merged:
   //   copy and reflect spectral data
   //   step 0

   // note that it turns out that the items added together during
   // this step are, in fact, being added to themselves (as reflected
   // by step 0). inexplicable inefficiency! this became obvious
   // once I combined the passes.

   // so there's a missing 'times 2' here (for adding X to itself).
   // this propagates through linearly to the end, where the numbers
   // are 1/2 too small, and need to be compensated for.

   {
      float *d,*e, *AA, *eStop;
      d = &buf2[n2-2];
      AA = A;
      e = &buffer[0];
      eStop = &buffer[n2];
      while (e != eStop) {
         d[1] = (e[0] * AA[0] - e[2]*AA[1]);
         d[0] = (e[0] * AA[1] + e[2]*AA[0]);
         d -= 2;
         AA += 2;
         e += 4;
      }

      e = &buffer[n2-3];
      while (d >= buf2) {
         d[1] = (-e[2] * AA[0] - -e[0]*AA[1]);
         d[0] = (-e[2] * AA[1] + -e[0]*AA[0]);
         d -= 2;
         AA += 2;
         e -= 4;
      }
   }

   // now we use symbolic names for these, so that we can
   // possibly swap their meaning as we change which operations
   // are in place

   u = buffer;
   v = buf2;

   // step 2    (paper output is w, now u)
   // this could be in place, but the data ends up in the wrong
   // place... _somebody_'s got to swap it, so this is nominated
   {
      float *AA = &A[n2-8];
      float *d0,*d1, *e0, *e1;

      e0 = &v[n4];
      e1 = &v[0];

      d0 = &u[n4];
      d1 = &u[0];

      while (AA >= A) {
         float v40_20, v41_21;

         v41_21 = e0[1] - e1[1];
         v40_20 = e0[0] - e1[0];
         d0[1]  = e0[1] + e1[1];
         d0[0]  = e0[0] + e1[0];
         d1[1]  = v41_21*AA[4] - v40_20*AA[5];
         d1[0]  = v40_20*AA[4] + v41_21*AA[5];

         v41_21 = e0[3] - e1[3];
         v40_20 = e0[2] - e1[2];
         d0[3]  = e0[3] + e1[3];
         d0[2]  = e0[2] + e1[2];
         d1[3]  = v41_21*AA[0] - v40_20*AA[1];
         d1[2]  = v40_20*AA[0] + v41_21*AA[1];

         AA -= 8;

         d0 += 4;
         d1 += 4;
         e0 += 4;
         e1 += 4;
      }
   }

   // step 3
   ld = ilog(n) - 1; // ilog is off-by-one from normal definitions

   // optimized step 3:

   // the original step3 loop can be nested r inside s or s inside r;
   // it's written originally as s inside r, but this is dumb when r
   // iterates many times, and s few. So I have two copies of it and
   // switch between them halfway.

   // this is iteration 0 of step 3
   imdctStep3_iter0_loop(n >> 4, u, n2-1-n4*0, -(n >> 3), A);
   imdctStep3_iter0_loop(n >> 4, u, n2-1-n4*1, -(n >> 3), A);

   // this is iteration 1 of step 3
   imdctStep3_innerR_loop(n >> 5, u, n2-1 - n8*0, -(n >> 4), A, 16);
   imdctStep3_innerR_loop(n >> 5, u, n2-1 - n8*1, -(n >> 4), A, 16);
   imdctStep3_innerR_loop(n >> 5, u, n2-1 - n8*2, -(n >> 4), A, 16);
   imdctStep3_innerR_loop(n >> 5, u, n2-1 - n8*3, -(n >> 4), A, 16);

   l=2;
   for (; l < (ld-3)>>1; ++l) {
      int k0 = n >> (l+2), k0_2 = k0>>1;
      int lim = 1 << (l+1);
      int i;
      for (i=0; i < lim; ++i)
         imdctStep3_innerR_loop(n >> (l+4), u, n2-1 - k0*i, -k0_2, A, 1 << (l+3));
   }

   for (; l < ld-6; ++l) {
      int k0 = n >> (l+2), k1 = 1 << (l+3), k0_2 = k0>>1;
      int rlim = n >> (l+6), r;
      int lim = 1 << (l+1);
      int iOff;
      float *A0 = A;
      iOff = n2-1;
      for (r=rlim; r > 0; --r) {
         imdctStep3_innerS_loop(lim, u, iOff, -k0_2, A0, k1, k0);
         A0 += k1*4;
         iOff -= 8;
      }
   }

   // iterations with count:
   //   ld-6,-5,-4 all interleaved together
   //       the big win comes from getting rid of needless flops
   //         due to the constants on pass 5 & 4 being all 1 and 0;
   //       combining them to be simultaneous to improve cache made little difference
   imdctStep3_innerS_loopLd654(n >> 5, u, n2-1, A, n);

   // output is u

   // step 4, 5, and 6
   // cannot be in-place because of step 5
   {
      uint16 *bitrev = f->bitReverse[blocktype];
      // weirdly, I'd have thought reading sequentially and writing
      // erratically would have been better than vice-versa, but in
      // fact that's not what my testing showed. (That is, with
      // j = bitreverse(i), do you read i and write j, or read j and write i.)

      float *d0 = &v[n4-4];
      float *d1 = &v[n2-4];
      while (d0 >= v) {
         int k4;

         k4 = bitrev[0];
         d1[3] = u[k4+0];
         d1[2] = u[k4+1];
         d0[3] = u[k4+2];
         d0[2] = u[k4+3];

         k4 = bitrev[1];
         d1[1] = u[k4+0];
         d1[0] = u[k4+1];
         d0[1] = u[k4+2];
         d0[0] = u[k4+3];

         d0 -= 4;
         d1 -= 4;
         bitrev += 2;
      }
   }
   // (paper output is u, now v)


   // data must be in buf2
   assert(v == buf2);

   // step 7   (paper output is v, now v)
   // this is now in place
   {
      float *C = f->C[blocktype];
      float *d, *e;

      d = v;
      e = v + n2 - 4;

      while (d < e) {
         float a02,a11,b0,b1,b2,b3;

         a02 = d[0] - e[2];
         a11 = d[1] + e[3];

         b0 = C[1]*a02 + C[0]*a11;
         b1 = C[1]*a11 - C[0]*a02;

         b2 = d[0] + e[ 2];
         b3 = d[1] - e[ 3];

         d[0] = b2 + b0;
         d[1] = b3 + b1;
         e[2] = b2 - b0;
         e[3] = b1 - b3;

         a02 = d[2] - e[0];
         a11 = d[3] + e[1];

         b0 = C[3]*a02 + C[2]*a11;
         b1 = C[3]*a11 - C[2]*a02;

         b2 = d[2] + e[ 0];
         b3 = d[3] - e[ 1];

         d[2] = b2 + b0;
         d[3] = b3 + b1;
         e[0] = b2 - b0;
         e[1] = b1 - b3;

         C += 4;
         d += 4;
         e -= 4;
      }
   }

   // data must be in buf2


   // step 8+decode   (paper output is X, now buffer)
   // this generates pairs of data a la 8 and pushes them directly through
   // the decode kernel (pushing rather than pulling) to avoid having
   // to make another pass later

   // this cannot POSSIBLY be in place, so we refer to the buffers directly

   {
      float *d0,*d1,*d2,*d3;

      float *B = f->B[blocktype] + n2 - 8;
      float *e = buf2 + n2 - 8;
      d0 = &buffer[0];
      d1 = &buffer[n2-4];
      d2 = &buffer[n2];
      d3 = &buffer[n-4];
      while (e >= v) {
         float p0,p1,p2,p3;

         p3 =  e[6]*B[7] - e[7]*B[6];
         p2 = -e[6]*B[6] - e[7]*B[7];

         d0[0] =   p3;
         d1[3] = - p3;
         d2[0] =   p2;
         d3[3] =   p2;

         p1 =  e[4]*B[5] - e[5]*B[4];
         p0 = -e[4]*B[4] - e[5]*B[5];

         d0[1] =   p1;
         d1[2] = - p1;
         d2[1] =   p0;
         d3[2] =   p0;

         p3 =  e[2]*B[3] - e[3]*B[2];
         p2 = -e[2]*B[2] - e[3]*B[3];

         d0[2] =   p3;
         d1[1] = - p3;
         d2[2] =   p2;
         d3[1] =   p2;

         p1 =  e[0]*B[1] - e[1]*B[0];
         p0 = -e[0]*B[0] - e[1]*B[1];

         d0[3] =   p1;
         d1[0] = - p1;
         d2[3] =   p0;
         d3[0] =   p0;

         B -= 8;
         e -= 8;
         d0 += 4;
         d2 += 4;
         d1 -= 4;
         d3 -= 4;
      }
   }

   tempFree(f,buf2);
   tempAllocRestore(f,savePoint);
}

#if 0
// this is the original version of the above code, if you want to optimize it from scratch
void inverseMdctNaive(float *buffer, int n)
{
   float s;
   float A[1 << 12], B[1 << 12], C[1 << 11];
   int i,k,k2,k4, n2 = n >> 1, n4 = n >> 2, n8 = n >> 3, l;
   int n3_4 = n - n4, ld;
   // how can they claim this only uses N words?!
   // oh, because they're only used sparsely, whoops
   float u[1 << 13], X[1 << 13], v[1 << 13], w[1 << 13];
   // set up twiddle factors

   for (k=k2=0; k < n4; ++k,k2+=2) {
      A[k2  ] = (float)  cos(4*k*M_PI/n);
      A[k2+1] = (float) -sin(4*k*M_PI/n);
      B[k2  ] = (float)  cos((k2+1)*M_PI/n/2);
      B[k2+1] = (float)  sin((k2+1)*M_PI/n/2);
   }
   for (k=k2=0; k < n8; ++k,k2+=2) {
      C[k2  ] = (float)  cos(2*(k2+1)*M_PI/n);
      C[k2+1] = (float) -sin(2*(k2+1)*M_PI/n);
   }

   // IMDCT algorithm from "The use of multirate filter banks for coding of high quality digital audio"
   // Note there are bugs in that pseudocode, presumably due to them attempting
   // to rename the arrays nicely rather than representing the way their actual
   // implementation bounces buffers back and forth. As a result, even in the
   // "some formulars corrected" version, a direct implementation fails. These
   // are noted below as "paper bug".

   // copy and reflect spectral data
   for (k=0; k < n2; ++k) u[k] = buffer[k];
   for (   ; k < n ; ++k) u[k] = -buffer[n - k - 1];
   // kernel from paper
   // step 1
   for (k=k2=k4=0; k < n4; k+=1, k2+=2, k4+=4) {
      v[n-k4-1] = (u[k4] - u[n-k4-1]) * A[k2]   - (u[k4+2] - u[n-k4-3])*A[k2+1];
      v[n-k4-3] = (u[k4] - u[n-k4-1]) * A[k2+1] + (u[k4+2] - u[n-k4-3])*A[k2];
   }
   // step 2
   for (k=k4=0; k < n8; k+=1, k4+=4) {
      w[n2+3+k4] = v[n2+3+k4] + v[k4+3];
      w[n2+1+k4] = v[n2+1+k4] + v[k4+1];
      w[k4+3]    = (v[n2+3+k4] - v[k4+3])*A[n2-4-k4] - (v[n2+1+k4]-v[k4+1])*A[n2-3-k4];
      w[k4+1]    = (v[n2+1+k4] - v[k4+1])*A[n2-4-k4] + (v[n2+3+k4]-v[k4+3])*A[n2-3-k4];
   }
   // step 3
   ld = ilog(n) - 1; // ilog is off-by-one from normal definitions
   for (l=0; l < ld-3; ++l) {
      int k0 = n >> (l+2), k1 = 1 << (l+3);
      int rlim = n >> (l+4), r4, r;
      int s2lim = 1 << (l+2), s2;
      for (r=r4=0; r < rlim; r4+=4,++r) {
         for (s2=0; s2 < s2lim; s2+=2) {
            u[n-1-k0*s2-r4] = w[n-1-k0*s2-r4] + w[n-1-k0*(s2+1)-r4];
            u[n-3-k0*s2-r4] = w[n-3-k0*s2-r4] + w[n-3-k0*(s2+1)-r4];
            u[n-1-k0*(s2+1)-r4] = (w[n-1-k0*s2-r4] - w[n-1-k0*(s2+1)-r4]) * A[r*k1]
                                - (w[n-3-k0*s2-r4] - w[n-3-k0*(s2+1)-r4]) * A[r*k1+1];
            u[n-3-k0*(s2+1)-r4] = (w[n-3-k0*s2-r4] - w[n-3-k0*(s2+1)-r4]) * A[r*k1]
                                + (w[n-1-k0*s2-r4] - w[n-1-k0*(s2+1)-r4]) * A[r*k1+1];
         }
      }
      if (l+1 < ld-3) {
         // paper bug: ping-ponging of u&w here is omitted
         memcpy(w, u, sizeof(u));
      }
   }

   // step 4
   for (i=0; i < n8; ++i) {
      int j = bitReverse(i) >> (32-ld+3);
      assert(j < n8);
      if (i == j) {
         // paper bug: original code probably swapped in place; if copying,
         //            need to directly copy in this case
         int i8 = i << 3;
         v[i8+1] = u[i8+1];
         v[i8+3] = u[i8+3];
         v[i8+5] = u[i8+5];
         v[i8+7] = u[i8+7];
      } else if (i < j) {
         int i8 = i << 3, j8 = j << 3;
         v[j8+1] = u[i8+1], v[i8+1] = u[j8 + 1];
         v[j8+3] = u[i8+3], v[i8+3] = u[j8 + 3];
         v[j8+5] = u[i8+5], v[i8+5] = u[j8 + 5];
         v[j8+7] = u[i8+7], v[i8+7] = u[j8 + 7];
      }
   }
   // step 5
   for (k=0; k < n2; ++k) {
      w[k] = v[k*2+1];
   }
   // step 6
   for (k=k2=k4=0; k < n8; ++k, k2 += 2, k4 += 4) {
      u[n-1-k2] = w[k4];
      u[n-2-k2] = w[k4+1];
      u[n3_4 - 1 - k2] = w[k4+2];
      u[n3_4 - 2 - k2] = w[k4+3];
   }
   // step 7
   for (k=k2=0; k < n8; ++k, k2 += 2) {
      v[n2 + k2 ] = ( u[n2 + k2] + u[n-2-k2] + C[k2+1]*(u[n2+k2]-u[n-2-k2]) + C[k2]*(u[n2+k2+1]+u[n-2-k2+1]))/2;
      v[n-2 - k2] = ( u[n2 + k2] + u[n-2-k2] - C[k2+1]*(u[n2+k2]-u[n-2-k2]) - C[k2]*(u[n2+k2+1]+u[n-2-k2+1]))/2;
      v[n2+1+ k2] = ( u[n2+1+k2] - u[n-1-k2] + C[k2+1]*(u[n2+1+k2]+u[n-1-k2]) - C[k2]*(u[n2+k2]-u[n-2-k2]))/2;
      v[n-1 - k2] = (-u[n2+1+k2] + u[n-1-k2] + C[k2+1]*(u[n2+1+k2]+u[n-1-k2]) - C[k2]*(u[n2+k2]-u[n-2-k2]))/2;
   }
   // step 8
   for (k=k2=0; k < n4; ++k,k2 += 2) {
      X[k]      = v[k2+n2]*B[k2  ] + v[k2+1+n2]*B[k2+1];
      X[n2-1-k] = v[k2+n2]*B[k2+1] - v[k2+1+n2]*B[k2  ];
   }

   // decode kernel to output
   // determined the following value experimentally
   // (by first figuring out what made inverseMdctSlow work); then matching that here
   // (probably vorbis encoder premultiplies by n or n/2, to save it on the decoder?)
   s = 0.5; // theoretically would be n4

   // [[[ note! the s value of 0.5 is compensated for by the B[] in the current code,
   //     so it needs to use the "old" B values to behave correctly, or else
   //     set s to 1.0 ]]]
   for (i=0; i < n4  ; ++i) buffer[i] = s * X[i+n4];
   for (   ; i < n3_4; ++i) buffer[i] = -s * X[n3_4 - i - 1];
   for (   ; i < n   ; ++i) buffer[i] = -s * X[i - n3_4];
}
#endif

static float *getWindow(vorb *f, int len)
{
   len <<= 1;
   if (len == f->blocksize_0) return f->window[0];
   if (len == f->blocksize_1) return f->window[1];
   return NULL;
}

#ifndef STB_VORBIS_NO_DEFER_FLOOR
typedef int16 YTYPE;
#else
typedef int YTYPE;
#endif
static int doFloor(vorb *f, Mapping *map, int i, int n, float *target, YTYPE *finalY, uint8 *step2_flag)
{
   int n2 = n >> 1;
   int s = map->chan[i].mux, floor;
   floor = map->submapFloor[s];
   if (f->floorTypes[floor] == 0) {
      return stbVorbisError(f, VORBIS_invalidStream);
   } else {
      Floor1 *g = &f->floorConfig[floor].floor1;
      int j,q;
      int lx = 0, ly = finalY[0] * g->floor1_multiplier;
      for (q=1; q < g->values; ++q) {
         j = g->sortedOrder[q];
         #ifndef STB_VORBIS_NO_DEFER_FLOOR
         if (finalY[j] >= 0)
         #else
         if (step2_flag[j])
         #endif
         {
            int hy = finalY[j] * g->floor1_multiplier;
            int hx = g->Xlist[j];
            if (lx != hx)
               drawLine(target, lx,ly, hx,hy, n2);
            CHECK(f);
            lx = hx, ly = hy;
         }
      }
      if (lx < n2) {
         // optimization of: drawLine(target, lx,ly, n,ly, n2);
         for (j=lx; j < n2; ++j)
            LINE_OP(target[j], inverseDbTable[ly]);
         CHECK(f);
      }
   }
   return TRUE;
}

// The meaning of "left" and "right"
//
// For a given frame:
//     we compute samples from 0..n
//     windowCenter is n/2
//     we'll window and mix the samples from leftStart to leftEnd with data from the previous frame
//     all of the samples from leftEnd to rightStart can be output without mixing; however,
//        this interval is 0-length except when transitioning between short and long frames
//     all of the samples from rightStart to rightEnd need to be mixed with the next frame,
//        which we don't have, so those get saved in a buffer
//     frame N's rightEnd-rightStart, the number of samples to mix with the next frame,
//        has to be the same as frame N+1's leftEnd-leftStart (which they are by
//        construction)

static int vorbisDecodeInitial(vorb *f, int *pLeftStart, int *pLeftEnd, int *pRightStart, int *pRightEnd, int *mode)
{
   Mode *m;
   int i, n, prev, next, windowCenter;
   f->channelBufferStart = f->channelBufferEnd = 0;

  retry:
   if (f->eof) return FALSE;
   if (!maybeStartPacket(f))
      return FALSE;
   // check packet type
   if (getBits(f,1) != 0) {
      if (IS_PUSH_MODE(f))
         return stbVorbisError(f,VORBIS_badPacketType);
      while (EOP != get8_packet(f));
      goto retry;
   }

   if (f->alloc.allocBuffer)
      assert(f->alloc.allocBufferLengthInBytes == f->tempOffset);

   i = getBits(f, ilog(f->modeCount-1));
   if (i == EOP) return FALSE;
   if (i >= f->modeCount) return FALSE;
   *mode = i;
   m = f->modeConfig + i;
   if (m->blockflag) {
      n = f->blocksize_1;
      prev = getBits(f,1);
      next = getBits(f,1);
   } else {
      prev = next = 0;
      n = f->blocksize_0;
   }

// WINDOWING

   windowCenter = n >> 1;
   if (m->blockflag && !prev) {
      *pLeftStart = (n - f->blocksize_0) >> 2;
      *pLeftEnd   = (n + f->blocksize_0) >> 2;
   } else {
      *pLeftStart = 0;
      *pLeftEnd   = windowCenter;
   }
   if (m->blockflag && !next) {
      *pRightStart = (n*3 - f->blocksize_0) >> 2;
      *pRightEnd   = (n*3 + f->blocksize_0) >> 2;
   } else {
      *pRightStart = windowCenter;
      *pRightEnd   = n;
   }

   return TRUE;
}

static int vorbisDecodePacketRest(vorb *f, int *len, Mode *m, int leftStart, int leftEnd, int rightStart, int rightEnd, int *pLeft)
{
   Mapping *map;
   int i,j,k,n,n2;
   int zeroChannel[256];
   int reallyZeroChannel[256];

// WINDOWING

   n = f->blocksize[m->blockflag];
   map = &f->mapping[m->mapping];

// FLOORS
   n2 = n >> 1;

   CHECK(f);

   for (i=0; i < f->channels; ++i) {
      int s = map->chan[i].mux, floor;
      zeroChannel[i] = FALSE;
      floor = map->submapFloor[s];
      if (f->floorTypes[floor] == 0) {
         return stbVorbisError(f, VORBIS_invalidStream);
      } else {
         Floor1 *g = &f->floorConfig[floor].floor1;
         if (getBits(f, 1)) {
            short *finalY;
            uint8 step2_flag[256];
            static int rangeList[4] = { 256, 128, 86, 64 };
            int range = rangeList[g->floor1_multiplier-1];
            int offset = 2;
            finalY = f->finalY[i];
            finalY[0] = getBits(f, ilog(range)-1);
            finalY[1] = getBits(f, ilog(range)-1);
            for (j=0; j < g->partitions; ++j) {
               int pclass = g->partitionClassList[j];
               int cdim = g->classDimensions[pclass];
               int cbits = g->classSubclasses[pclass];
               int csub = (1 << cbits)-1;
               int cval = 0;
               if (cbits) {
                  Codebook *c = f->codebooks + g->classMasterbooks[pclass];
                  DECODE(cval,f,c);
               }
               for (k=0; k < cdim; ++k) {
                  int book = g->subclassBooks[pclass][cval & csub];
                  cval = cval >> cbits;
                  if (book >= 0) {
                     int temp;
                     Codebook *c = f->codebooks + book;
                     DECODE(temp,f,c);
                     finalY[offset++] = temp;
                  } else
                     finalY[offset++] = 0;
               }
            }
            if (f->validBits == INVALID_BITS) goto error; // behavior according to spec
            step2_flag[0] = step2_flag[1] = 1;
            for (j=2; j < g->values; ++j) {
               int low, high, pred, highroom, lowroom, room, val;
               low = g->neighbors[j][0];
               high = g->neighbors[j][1];
               //neighbors(g->Xlist, j, &low, &high);
               pred = predictPoint(g->Xlist[j], g->Xlist[low], g->Xlist[high], finalY[low], finalY[high]);
               val = finalY[j];
               highroom = range - pred;
               lowroom = pred;
               if (highroom < lowroom)
                  room = highroom * 2;
               else
                  room = lowroom * 2;
               if (val) {
                  step2_flag[low] = step2_flag[high] = 1;
                  step2_flag[j] = 1;
                  if (val >= room)
                     if (highroom > lowroom)
                        finalY[j] = val - lowroom + pred;
                     else
                        finalY[j] = pred - val + highroom - 1;
                  else
                     if (val & 1)
                        finalY[j] = pred - ((val+1)>>1);
                     else
                        finalY[j] = pred + (val>>1);
               } else {
                  step2_flag[j] = 0;
                  finalY[j] = pred;
               }
            }

#ifdef STB_VORBIS_NO_DEFER_FLOOR
            doFloor(f, map, i, n, f->floorBuffers[i], finalY, step2_flag);
#else
            // defer final floor computation until _after_ residue
            for (j=0; j < g->values; ++j) {
               if (!step2_flag[j])
                  finalY[j] = -1;
            }
#endif
         } else {
           error:
            zeroChannel[i] = TRUE;
         }
         // So we just defer everything else to later

         // at this point we've decoded the floor into buffer
      }
   }
   CHECK(f);
   // at this point we've decoded all floors

   if (f->alloc.allocBuffer)
      assert(f->alloc.allocBufferLengthInBytes == f->tempOffset);

   // re-enable coupled channels if necessary
   memcpy(reallyZeroChannel, zeroChannel, sizeof(reallyZeroChannel[0]) * f->channels);
   for (i=0; i < map->couplingSteps; ++i)
      if (!zeroChannel[map->chan[i].magnitude] || !zeroChannel[map->chan[i].angle]) {
         zeroChannel[map->chan[i].magnitude] = zeroChannel[map->chan[i].angle] = FALSE;
      }

   CHECK(f);
// RESIDUE DECODE
   for (i=0; i < map->submaps; ++i) {
      float *residueBuffers[STB_VORBIS_MAX_CHANNELS];
      int r;
      uint8 doNotDecode[256];
      int ch = 0;
      for (j=0; j < f->channels; ++j) {
         if (map->chan[j].mux == i) {
            if (zeroChannel[j]) {
               doNotDecode[ch] = TRUE;
               residueBuffers[ch] = NULL;
            } else {
               doNotDecode[ch] = FALSE;
               residueBuffers[ch] = f->channelBuffers[j];
            }
            ++ch;
         }
      }
      r = map->submapResidue[i];
      decodeResidue(f, residueBuffers, ch, n2, r, doNotDecode);
   }

   if (f->alloc.allocBuffer)
      assert(f->alloc.allocBufferLengthInBytes == f->tempOffset);
   CHECK(f);

// INVERSE COUPLING
   for (i = map->couplingSteps-1; i >= 0; --i) {
      int n2 = n >> 1;
      float *m = f->channelBuffers[map->chan[i].magnitude];
      float *a = f->channelBuffers[map->chan[i].angle    ];
      for (j=0; j < n2; ++j) {
         float a2,m2;
         if (m[j] > 0)
            if (a[j] > 0)
               m2 = m[j], a2 = m[j] - a[j];
            else
               a2 = m[j], m2 = m[j] + a[j];
         else
            if (a[j] > 0)
               m2 = m[j], a2 = m[j] + a[j];
            else
               a2 = m[j], m2 = m[j] - a[j];
         m[j] = m2;
         a[j] = a2;
      }
   }
   CHECK(f);

   // finish decoding the floors
#ifndef STB_VORBIS_NO_DEFER_FLOOR
   for (i=0; i < f->channels; ++i) {
      if (reallyZeroChannel[i]) {
         memset(f->channelBuffers[i], 0, sizeof(*f->channelBuffers[i]) * n2);
      } else {
         doFloor(f, map, i, n, f->channelBuffers[i], f->finalY[i], NULL);
      }
   }
#else
   for (i=0; i < f->channels; ++i) {
      if (reallyZeroChannel[i]) {
         memset(f->channelBuffers[i], 0, sizeof(*f->channelBuffers[i]) * n2);
      } else {
         for (j=0; j < n2; ++j)
            f->channelBuffers[i][j] *= f->floorBuffers[i][j];
      }
   }
#endif

// INVERSE MDCT
   CHECK(f);
   for (i=0; i < f->channels; ++i)
      inverseMdct(f->channelBuffers[i], n, f, m->blockflag);
   CHECK(f);

   // this shouldn't be necessary, unless we exited on an error
   // and want to flush to get to the next packet
   flushPacket(f);

   if (f->firstDecode) {
      // assume we start so first non-discarded sample is sample 0
      // this isn't to spec, but spec would require us to read ahead
      // and decode the size of all current frames--could be done,
      // but presumably it's not a commonly used feature
      f->currentLoc = -n2; // start of first frame is positioned for discard
      // we might have to discard samples "from" the next frame too,
      // if we're lapping a large block then a small at the start?
      f->discardSamplesDeferred = n - rightEnd;
      f->currentLocValid = TRUE;
      f->firstDecode = FALSE;
   } else if (f->discardSamplesDeferred) {
      if (f->discardSamplesDeferred >= rightStart - leftStart) {
         f->discardSamplesDeferred -= (rightStart - leftStart);
         leftStart = rightStart;
         *pLeft = leftStart;
      } else {
         leftStart += f->discardSamplesDeferred;
         *pLeft = leftStart;
         f->discardSamplesDeferred = 0;
      }
   } else if (f->previousLength == 0 && f->currentLocValid) {
      // we're recovering from a seek... that means we're going to discard
      // the samples from this packet even though we know our position from
      // the last page header, so we need to update the position based on
      // the discarded samples here
      // but wait, the code below is going to add this in itself even
      // on a discard, so we don't need to do it here...
   }

   // check if we have ogg information about the sample # for this packet
   if (f->lastSegWhich == f->endSegWithKnownLoc) {
      // if we have a valid current loc, and this is final:
      if (f->currentLocValid && (f->pageFlag & PAGEFLAG_lastPage)) {
         uint32 currentEnd = f->knownLocForPacket;
         // then let's infer the size of the (probably) short final frame
         if (currentEnd < f->currentLoc + (rightEnd-leftStart)) {
            if (currentEnd < f->currentLoc) {
               // negative truncation, that's impossible!
               *len = 0;
            } else {
               *len = currentEnd - f->currentLoc;
            }
            *len += leftStart; // this doesn't seem right, but has no ill effect on my test files
            if (*len > rightEnd) *len = rightEnd; // this should never happen
            f->currentLoc += *len;
            return TRUE;
         }
      }
      // otherwise, just set our sample loc
      // guess that the ogg granule pos refers to the _middle_ of the
      // last frame?
      // set f->currentLoc to the position of leftStart
      f->currentLoc = f->knownLocForPacket - (n2-leftStart);
      f->currentLocValid = TRUE;
   }
   if (f->currentLocValid)
      f->currentLoc += (rightStart - leftStart);

   if (f->alloc.allocBuffer)
      assert(f->alloc.allocBufferLengthInBytes == f->tempOffset);
   *len = rightEnd;  // ignore samples after the window goes to 0
   CHECK(f);

   return TRUE;
}

static int vorbisDecodePacket(vorb *f, int *len, int *pLeft, int *pRight)
{
   int mode, leftEnd, rightEnd;
   if (!vorbisDecodeInitial(f, pLeft, &leftEnd, pRight, &rightEnd, &mode)) return 0;
   return vorbisDecodePacketRest(f, len, f->modeConfig + mode, *pLeft, leftEnd, *pRight, rightEnd, pLeft);
}

static int vorbisFinishFrame(stbVorbis *f, int len, int left, int right)
{
   int prev,i,j;
   // we use right&left (the start of the right- and left-window sin()-regions)
   // to determine how much to return, rather than inferring from the rules
   // (same result, clearer code); 'left' indicates where our sin() window
   // starts, therefore where the previous window's right edge starts, and
   // therefore where to start mixing from the previous buffer. 'right'
   // indicates where our sin() ending-window starts, therefore that's where
   // we start saving, and where our returned-data ends.

   // mixin from previous window
   if (f->previousLength) {
      int i,j, n = f->previousLength;
      float *w = getWindow(f, n);
      if (w == NULL) return 0;
      for (i=0; i < f->channels; ++i) {
         for (j=0; j < n; ++j)
            f->channelBuffers[i][left+j] =
               f->channelBuffers[i][left+j]*w[    j] +
               f->previousWindow[i][     j]*w[n-1-j];
      }
   }

   prev = f->previousLength;

   // last half of this data becomes previous window
   f->previousLength = len - right;

   // @OPTIMIZE: could avoid this copy by double-buffering the
   // output (flipping previousWindow with channelBuffers), but
   // then previousWindow would have to be 2x as large, and
   // channelBuffers couldn't be temp mem (although they're NOT
   // currently temp mem, they could be (unless we want to level
   // performance by spreading out the computation))
   for (i=0; i < f->channels; ++i)
      for (j=0; right+j < len; ++j)
         f->previousWindow[i][j] = f->channelBuffers[i][right+j];

   if (!prev)
      // there was no previous packet, so this data isn't valid...
      // this isn't entirely true, only the would-have-overlapped data
      // isn't valid, but this seems to be what the spec requires
      return 0;

   // truncate a short frame
   if (len < right) right = len;

   f->samplesOutput += right-left;

   return right - left;
}

static int vorbisPumpFirstFrame(stbVorbis *f)
{
   int len, right, left, res;
   res = vorbisDecodePacket(f, &len, &left, &right);
   if (res)
      vorbisFinishFrame(f, len, left, right);
   return res;
}

#ifndef STB_VORBIS_NO_PUSHDATA_API
static int isWholePacketPresent(stbVorbis *f)
{
   // make sure that we have the packet available before continuing...
   // this requires a full ogg parse, but we know we can fetch from f->stream

   // instead of coding this out explicitly, we could save the current read state,
   // read the next packet with get8() until end-of-packet, check f->eof, then
   // reset the state? but that would be slower, esp. since we'd have over 256 bytes
   // of state to restore (primarily the page segment table)

   int s = f->nextSeg, first = TRUE;
   uint8 *p = f->stream;

   if (s != -1) { // if we're not starting the packet with a 'continue on next page' flag
      for (; s < f->segmentCount; ++s) {
         p += f->segments[s];
         if (f->segments[s] < 255)               // stop at first short segment
            break;
      }
      // either this continues, or it ends it...
      if (s == f->segmentCount)
         s = -1; // set 'crosses page' flag
      if (p > f->streamEnd)                     return stbVorbisError(f, VORBIS_needMoreData);
      first = FALSE;
   }
   for (; s == -1;) {
      uint8 *q;
      int n;

      // check that we have the page header ready
      if (p + 26 >= f->streamEnd)               return stbVorbisError(f, VORBIS_needMoreData);
      // validate the page
      if (memcmp(p, oggPageHeader, 4))         return stbVorbisError(f, VORBIS_invalidStream);
      if (p[4] != 0)                             return stbVorbisError(f, VORBIS_invalidStream);
      if (first) { // the first segment must NOT have 'continuedPacket', later ones MUST
         if (f->previousLength)
            if ((p[5] & PAGEFLAG_continuedPacket))  return stbVorbisError(f, VORBIS_invalidStream);
         // if no previous length, we're resynching, so we can come in on a continued-packet,
         // which we'll just drop
      } else {
         if (!(p[5] & PAGEFLAG_continuedPacket)) return stbVorbisError(f, VORBIS_invalidStream);
      }
      n = p[26]; // segment counts
      q = p+27;  // q points to segment table
      p = q + n; // advance past header
      // make sure we've read the segment table
      if (p > f->streamEnd)                     return stbVorbisError(f, VORBIS_needMoreData);
      for (s=0; s < n; ++s) {
         p += q[s];
         if (q[s] < 255)
            break;
      }
      if (s == n)
         s = -1; // set 'crosses page' flag
      if (p > f->streamEnd)                     return stbVorbisError(f, VORBIS_needMoreData);
      first = FALSE;
   }
   return TRUE;
}
#endif // !STB_VORBIS_NO_PUSHDATA_API

static int startDecoder(vorb *f)
{
   uint8 header[6], x,y;
   int len,i,j,k, maxSubmaps = 0;
   int longestFloorlist=0;

   // first page, first packet
   f->firstDecode = TRUE;

   if (!startPage(f))                              return FALSE;
   // validate page flag
   if (!(f->pageFlag & PAGEFLAG_firstPage))       return stbVorbisError(f, VORBIS_invalidFirstPage);
   if (f->pageFlag & PAGEFLAG_lastPage)           return stbVorbisError(f, VORBIS_invalidFirstPage);
   if (f->pageFlag & PAGEFLAG_continuedPacket)    return stbVorbisError(f, VORBIS_invalidFirstPage);
   // check for expected packet length
   if (f->segmentCount != 1)                       return stbVorbisError(f, VORBIS_invalidFirstPage);
   if (f->segments[0] != 30) {
      // check for the Ogg skeleton fishead identifying header to refine our error
      if (f->segments[0] == 64 &&
          getn(f, header, 6) &&
          header[0] == 'f' &&
          header[1] == 'i' &&
          header[2] == 's' &&
          header[3] == 'h' &&
          header[4] == 'e' &&
          header[5] == 'a' &&
          get8(f)   == 'd' &&
          get8(f)   == '\0')                        return stbVorbisError(f, VORBIS_oggSkeletonNotSupported);
      else
                                                    return stbVorbisError(f, VORBIS_invalidFirstPage);
   }

   // read packet
   // check packet header
   if (get8(f) != VORBIS_packetId)                 return stbVorbisError(f, VORBIS_invalidFirstPage);
   if (!getn(f, header, 6))                         return stbVorbisError(f, VORBIS_unexpectedEof);
   if (!vorbisValidate(header))                    return stbVorbisError(f, VORBIS_invalidFirstPage);
   // vorbisVersion
   if (get32(f) != 0)                               return stbVorbisError(f, VORBIS_invalidFirstPage);
   f->channels = get8(f); if (!f->channels)         return stbVorbisError(f, VORBIS_invalidFirstPage);
   if (f->channels > STB_VORBIS_MAX_CHANNELS)       return stbVorbisError(f, VORBIS_tooManyChannels);
   f->sampleRate = get32(f); if (!f->sampleRate)  return stbVorbisError(f, VORBIS_invalidFirstPage);
   get32(f); // bitrateMaximum
   get32(f); // bitrateNominal
   get32(f); // bitrateMinimum
   x = get8(f);
   {
      int log0,log1;
      log0 = x & 15;
      log1 = x >> 4;
      f->blocksize_0 = 1 << log0;
      f->blocksize_1 = 1 << log1;
      if (log0 < 6 || log0 > 13)                       return stbVorbisError(f, VORBIS_invalidSetup);
      if (log1 < 6 || log1 > 13)                       return stbVorbisError(f, VORBIS_invalidSetup);
      if (log0 > log1)                                 return stbVorbisError(f, VORBIS_invalidSetup);
   }

   // framingFlag
   x = get8(f);
   if (!(x & 1))                                    return stbVorbisError(f, VORBIS_invalidFirstPage);

   // second packet!
   if (!startPage(f))                              return FALSE;

   if (!startPacket(f))                            return FALSE;
   do {
      len = nextSegment(f);
      skip(f, len);
      f->bytesInSeg = 0;
   } while (len);

   // third packet!
   if (!startPacket(f))                            return FALSE;

   #ifndef STB_VORBIS_NO_PUSHDATA_API
   if (IS_PUSH_MODE(f)) {
      if (!isWholePacketPresent(f)) {
         // convert error in ogg header to write type
         if (f->error == VORBIS_invalidStream)
            f->error = VORBIS_invalidSetup;
         return FALSE;
      }
   }
   #endif

   crc32_init(); // always init it, to avoid multithread race conditions

   if (get8_packet(f) != VORBIS_packetSetup)       return stbVorbisError(f, VORBIS_invalidSetup);
   for (i=0; i < 6; ++i) header[i] = get8_packet(f);
   if (!vorbisValidate(header))                    return stbVorbisError(f, VORBIS_invalidSetup);

   // codebooks

   f->codebookCount = getBits(f,8) + 1;
   f->codebooks = (Codebook *) setupMalloc(f, sizeof(*f->codebooks) * f->codebookCount);
   if (f->codebooks == NULL)                        return stbVorbisError(f, VORBIS_outofmem);
   memset(f->codebooks, 0, sizeof(*f->codebooks) * f->codebookCount);
   for (i=0; i < f->codebookCount; ++i) {
      uint32 *values;
      int ordered, sortedCount;
      int total=0;
      uint8 *lengths;
      Codebook *c = f->codebooks+i;
      CHECK(f);
      x = getBits(f, 8); if (x != 0x42)            return stbVorbisError(f, VORBIS_invalidSetup);
      x = getBits(f, 8); if (x != 0x43)            return stbVorbisError(f, VORBIS_invalidSetup);
      x = getBits(f, 8); if (x != 0x56)            return stbVorbisError(f, VORBIS_invalidSetup);
      x = getBits(f, 8);
      c->dimensions = (getBits(f, 8)<<8) + x;
      x = getBits(f, 8);
      y = getBits(f, 8);
      c->entries = (getBits(f, 8)<<16) + (y<<8) + x;
      ordered = getBits(f,1);
      c->sparse = ordered ? 0 : getBits(f,1);

      if (c->dimensions == 0 && c->entries != 0)    return stbVorbisError(f, VORBIS_invalidSetup);

      if (c->sparse)
         lengths = (uint8 *) setupTempMalloc(f, c->entries);
      else
         lengths = c->codewordLengths = (uint8 *) setupMalloc(f, c->entries);

      if (!lengths) return stbVorbisError(f, VORBIS_outofmem);

      if (ordered) {
         int currentEntry = 0;
         int currentLength = getBits(f,5) + 1;
         while (currentEntry < c->entries) {
            int limit = c->entries - currentEntry;
            int n = getBits(f, ilog(limit));
            if (currentLength >= 32) return stbVorbisError(f, VORBIS_invalidSetup);
            if (currentEntry + n > (int) c->entries) { return stbVorbisError(f, VORBIS_invalidSetup); }
            memset(lengths + currentEntry, currentLength, n);
            currentEntry += n;
            ++currentLength;
         }
      } else {
         for (j=0; j < c->entries; ++j) {
            int present = c->sparse ? getBits(f,1) : 1;
            if (present) {
               lengths[j] = getBits(f, 5) + 1;
               ++total;
               if (lengths[j] == 32)
                  return stbVorbisError(f, VORBIS_invalidSetup);
            } else {
               lengths[j] = NO_CODE;
            }
         }
      }

      if (c->sparse && total >= c->entries >> 2) {
         // convert sparse items to non-sparse!
         if (c->entries > (int) f->setupTempMemoryRequired)
            f->setupTempMemoryRequired = c->entries;

         c->codewordLengths = (uint8 *) setupMalloc(f, c->entries);
         if (c->codewordLengths == NULL) return stbVorbisError(f, VORBIS_outofmem);
         memcpy(c->codewordLengths, lengths, c->entries);
         setupTempFree(f, lengths, c->entries); // note this is only safe if there have been no intervening temp mallocs!
         lengths = c->codewordLengths;
         c->sparse = 0;
      }

      // compute the size of the sorted tables
      if (c->sparse) {
         sortedCount = total;
      } else {
         sortedCount = 0;
         #ifndef STB_VORBIS_NO_HUFFMAN_BINARY_SEARCH
         for (j=0; j < c->entries; ++j)
            if (lengths[j] > STB_VORBIS_FAST_HUFFMAN_LENGTH && lengths[j] != NO_CODE)
               ++sortedCount;
         #endif
      }

      c->sortedEntries = sortedCount;
      values = NULL;

      CHECK(f);
      if (!c->sparse) {
         c->codewords = (uint32 *) setupMalloc(f, sizeof(c->codewords[0]) * c->entries);
         if (!c->codewords)                  return stbVorbisError(f, VORBIS_outofmem);
      } else {
         unsigned int size;
         if (c->sortedEntries) {
            c->codewordLengths = (uint8 *) setupMalloc(f, c->sortedEntries);
            if (!c->codewordLengths)           return stbVorbisError(f, VORBIS_outofmem);
            c->codewords = (uint32 *) setupTempMalloc(f, sizeof(*c->codewords) * c->sortedEntries);
            if (!c->codewords)                  return stbVorbisError(f, VORBIS_outofmem);
            values = (uint32 *) setupTempMalloc(f, sizeof(*values) * c->sortedEntries);
            if (!values)                        return stbVorbisError(f, VORBIS_outofmem);
         }
         size = c->entries + (sizeof(*c->codewords) + sizeof(*values)) * c->sortedEntries;
         if (size > f->setupTempMemoryRequired)
            f->setupTempMemoryRequired = size;
      }

      if (!computeCodewords(c, lengths, c->entries, values)) {
         if (c->sparse) setupTempFree(f, values, 0);
         return stbVorbisError(f, VORBIS_invalidSetup);
      }

      if (c->sortedEntries) {
         // allocate an extra slot for sentinels
         c->sortedCodewords = (uint32 *) setupMalloc(f, sizeof(*c->sortedCodewords) * (c->sortedEntries+1));
         if (c->sortedCodewords == NULL) return stbVorbisError(f, VORBIS_outofmem);
         // allocate an extra slot at the front so that c->sortedValues[-1] is defined
         // so that we can catch that case without an extra if
         c->sortedValues    = ( int   *) setupMalloc(f, sizeof(*c->sortedValues   ) * (c->sortedEntries+1));
         if (c->sortedValues == NULL) return stbVorbisError(f, VORBIS_outofmem);
         ++c->sortedValues;
         c->sortedValues[-1] = -1;
         computeSortedHuffman(c, lengths, values);
      }

      if (c->sparse) {
         setupTempFree(f, values, sizeof(*values)*c->sortedEntries);
         setupTempFree(f, c->codewords, sizeof(*c->codewords)*c->sortedEntries);
         setupTempFree(f, lengths, c->entries);
         c->codewords = NULL;
      }

      computeAcceleratedHuffman(c);

      CHECK(f);
      c->lookupType = getBits(f, 4);
      if (c->lookupType > 2) return stbVorbisError(f, VORBIS_invalidSetup);
      if (c->lookupType > 0) {
         uint16 *mults;
         c->minimumValue = float32_unpack(getBits(f, 32));
         c->deltaValue = float32_unpack(getBits(f, 32));
         c->valueBits = getBits(f, 4)+1;
         c->sequenceP = getBits(f,1);
         if (c->lookupType == 1) {
            int values = lookup1_values(c->entries, c->dimensions);
            if (values < 0) return stbVorbisError(f, VORBIS_invalidSetup);
            c->lookupValues = (uint32) values;
         } else {
            c->lookupValues = c->entries * c->dimensions;
         }
         if (c->lookupValues == 0) return stbVorbisError(f, VORBIS_invalidSetup);
         mults = (uint16 *) setupTempMalloc(f, sizeof(mults[0]) * c->lookupValues);
         if (mults == NULL) return stbVorbisError(f, VORBIS_outofmem);
         for (j=0; j < (int) c->lookupValues; ++j) {
            int q = getBits(f, c->valueBits);
            if (q == EOP) { setupTempFree(f,mults,sizeof(mults[0])*c->lookupValues); return stbVorbisError(f, VORBIS_invalidSetup); }
            mults[j] = q;
         }

#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
         if (c->lookupType == 1) {
            int len, sparse = c->sparse;
            float last=0;
            // pre-expand the lookup1-style multiplicands, to avoid a divide in the inner loop
            if (sparse) {
               if (c->sortedEntries == 0) goto skip;
               c->multiplicands = (codetype *) setupMalloc(f, sizeof(c->multiplicands[0]) * c->sortedEntries * c->dimensions);
            } else
               c->multiplicands = (codetype *) setupMalloc(f, sizeof(c->multiplicands[0]) * c->entries        * c->dimensions);
            if (c->multiplicands == NULL) { setupTempFree(f,mults,sizeof(mults[0])*c->lookupValues); return stbVorbisError(f, VORBIS_outofmem); }
            len = sparse ? c->sortedEntries : c->entries;
            for (j=0; j < len; ++j) {
               unsigned int z = sparse ? c->sortedValues[j] : j;
               unsigned int div=1;
               for (k=0; k < c->dimensions; ++k) {
                  int off = (z / div) % c->lookupValues;
                  float val = mults[off];
                  val = mults[off]*c->deltaValue + c->minimumValue + last;
                  c->multiplicands[j*c->dimensions + k] = val;
                  if (c->sequenceP)
                     last = val;
                  if (k+1 < c->dimensions) {
                     if (div > UINT_MAX / (unsigned int) c->lookupValues) {
                        setupTempFree(f, mults,sizeof(mults[0])*c->lookupValues);
                        return stbVorbisError(f, VORBIS_invalidSetup);
                     }
                     div *= c->lookupValues;
                  }
               }
            }
            c->lookupType = 2;
         }
         else
#endif
         {
            float last=0;
            CHECK(f);
            c->multiplicands = (codetype *) setupMalloc(f, sizeof(c->multiplicands[0]) * c->lookupValues);
            if (c->multiplicands == NULL) { setupTempFree(f, mults,sizeof(mults[0])*c->lookupValues); return stbVorbisError(f, VORBIS_outofmem); }
            for (j=0; j < (int) c->lookupValues; ++j) {
               float val = mults[j] * c->deltaValue + c->minimumValue + last;
               c->multiplicands[j] = val;
               if (c->sequenceP)
                  last = val;
            }
         }
#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
        skip:;
#endif
         setupTempFree(f, mults, sizeof(mults[0])*c->lookupValues);

         CHECK(f);
      }
      CHECK(f);
   }

   // time domain transfers (notused)

   x = getBits(f, 6) + 1;
   for (i=0; i < x; ++i) {
      uint32 z = getBits(f, 16);
      if (z != 0) return stbVorbisError(f, VORBIS_invalidSetup);
   }

   // Floors
   f->floorCount = getBits(f, 6)+1;
   f->floorConfig = (Floor *)  setupMalloc(f, f->floorCount * sizeof(*f->floorConfig));
   if (f->floorConfig == NULL) return stbVorbisError(f, VORBIS_outofmem);
   for (i=0; i < f->floorCount; ++i) {
      f->floorTypes[i] = getBits(f, 16);
      if (f->floorTypes[i] > 1) return stbVorbisError(f, VORBIS_invalidSetup);
      if (f->floorTypes[i] == 0) {
         Floor0 *g = &f->floorConfig[i].floor0;
         g->order = getBits(f,8);
         g->rate = getBits(f,16);
         g->barkMapSize = getBits(f,16);
         g->amplitudeBits = getBits(f,6);
         g->amplitudeOffset = getBits(f,8);
         g->numberOfBooks = getBits(f,4) + 1;
         for (j=0; j < g->numberOfBooks; ++j)
            g->bookList[j] = getBits(f,8);
         return stbVorbisError(f, VORBIS_featureNotSupported);
      } else {
         stbv__floorOrdering p[31*8+2];
         Floor1 *g = &f->floorConfig[i].floor1;
         int maxClass = -1;
         g->partitions = getBits(f, 5);
         for (j=0; j < g->partitions; ++j) {
            g->partitionClassList[j] = getBits(f, 4);
            if (g->partitionClassList[j] > maxClass)
               maxClass = g->partitionClassList[j];
         }
         for (j=0; j <= maxClass; ++j) {
            g->classDimensions[j] = getBits(f, 3)+1;
            g->classSubclasses[j] = getBits(f, 2);
            if (g->classSubclasses[j]) {
               g->classMasterbooks[j] = getBits(f, 8);
               if (g->classMasterbooks[j] >= f->codebookCount) return stbVorbisError(f, VORBIS_invalidSetup);
            }
            for (k=0; k < 1 << g->classSubclasses[j]; ++k) {
               g->subclassBooks[j][k] = getBits(f,8)-1;
               if (g->subclassBooks[j][k] >= f->codebookCount) return stbVorbisError(f, VORBIS_invalidSetup);
            }
         }
         g->floor1_multiplier = getBits(f,2)+1;
         g->rangebits = getBits(f,4);
         g->Xlist[0] = 0;
         g->Xlist[1] = 1 << g->rangebits;
         g->values = 2;
         for (j=0; j < g->partitions; ++j) {
            int c = g->partitionClassList[j];
            for (k=0; k < g->classDimensions[c]; ++k) {
               g->Xlist[g->values] = getBits(f, g->rangebits);
               ++g->values;
            }
         }
         // precompute the sorting
         for (j=0; j < g->values; ++j) {
            p[j].x = g->Xlist[j];
            p[j].id = j;
         }
         qsort(p, g->values, sizeof(p[0]), pointCompare);
         for (j=0; j < g->values-1; ++j)
            if (p[j].x == p[j+1].x)
               return stbVorbisError(f, VORBIS_invalidSetup);
         for (j=0; j < g->values; ++j)
            g->sortedOrder[j] = (uint8) p[j].id;
         // precompute the neighbors
         for (j=2; j < g->values; ++j) {
            int low = 0,hi = 0;
            neighbors(g->Xlist, j, &low,&hi);
            g->neighbors[j][0] = low;
            g->neighbors[j][1] = hi;
         }

         if (g->values > longestFloorlist)
            longestFloorlist = g->values;
      }
   }

   // Residue
   f->residueCount = getBits(f, 6)+1;
   f->residueConfig = (Residue *) setupMalloc(f, f->residueCount * sizeof(f->residueConfig[0]));
   if (f->residueConfig == NULL) return stbVorbisError(f, VORBIS_outofmem);
   memset(f->residueConfig, 0, f->residueCount * sizeof(f->residueConfig[0]));
   for (i=0; i < f->residueCount; ++i) {
      uint8 residueCascade[64];
      Residue *r = f->residueConfig+i;
      f->residueTypes[i] = getBits(f, 16);
      if (f->residueTypes[i] > 2) return stbVorbisError(f, VORBIS_invalidSetup);
      r->begin = getBits(f, 24);
      r->end = getBits(f, 24);
      if (r->end < r->begin) return stbVorbisError(f, VORBIS_invalidSetup);
      r->partSize = getBits(f,24)+1;
      r->classifications = getBits(f,6)+1;
      r->classbook = getBits(f,8);
      if (r->classbook >= f->codebookCount) return stbVorbisError(f, VORBIS_invalidSetup);
      for (j=0; j < r->classifications; ++j) {
         uint8 highBits=0;
         uint8 lowBits=getBits(f,3);
         if (getBits(f,1))
            highBits = getBits(f,5);
         residueCascade[j] = highBits*8 + lowBits;
      }
      r->residueBooks = (short (*)[8]) setupMalloc(f, sizeof(r->residueBooks[0]) * r->classifications);
      if (r->residueBooks == NULL) return stbVorbisError(f, VORBIS_outofmem);
      for (j=0; j < r->classifications; ++j) {
         for (k=0; k < 8; ++k) {
            if (residueCascade[j] & (1 << k)) {
               r->residueBooks[j][k] = getBits(f, 8);
               if (r->residueBooks[j][k] >= f->codebookCount) return stbVorbisError(f, VORBIS_invalidSetup);
            } else {
               r->residueBooks[j][k] = -1;
            }
         }
      }
      // precompute the classifications[] array to avoid inner-loop mod/divide
      // call it 'classdata' since we already have r->classifications
      r->classdata = (uint8 **) setupMalloc(f, sizeof(*r->classdata) * f->codebooks[r->classbook].entries);
      if (!r->classdata) return stbVorbisError(f, VORBIS_outofmem);
      memset(r->classdata, 0, sizeof(*r->classdata) * f->codebooks[r->classbook].entries);
      for (j=0; j < f->codebooks[r->classbook].entries; ++j) {
         int classwords = f->codebooks[r->classbook].dimensions;
         int temp = j;
         r->classdata[j] = (uint8 *) setupMalloc(f, sizeof(r->classdata[j][0]) * classwords);
         if (r->classdata[j] == NULL) return stbVorbisError(f, VORBIS_outofmem);
         for (k=classwords-1; k >= 0; --k) {
            r->classdata[j][k] = temp % r->classifications;
            temp /= r->classifications;
         }
      }
   }

   f->mappingCount = getBits(f,6)+1;
   f->mapping = (Mapping *) setupMalloc(f, f->mappingCount * sizeof(*f->mapping));
   if (f->mapping == NULL) return stbVorbisError(f, VORBIS_outofmem);
   memset(f->mapping, 0, f->mappingCount * sizeof(*f->mapping));
   for (i=0; i < f->mappingCount; ++i) {
      Mapping *m = f->mapping + i;
      int mappingType = getBits(f,16);
      if (mappingType != 0) return stbVorbisError(f, VORBIS_invalidSetup);
      m->chan = (MappingChannel *) setupMalloc(f, f->channels * sizeof(*m->chan));
      if (m->chan == NULL) return stbVorbisError(f, VORBIS_outofmem);
      if (getBits(f,1))
         m->submaps = getBits(f,4)+1;
      else
         m->submaps = 1;
      if (m->submaps > maxSubmaps)
         maxSubmaps = m->submaps;
      if (getBits(f,1)) {
         m->couplingSteps = getBits(f,8)+1;
         if (m->couplingSteps > f->channels) return stbVorbisError(f, VORBIS_invalidSetup);
         for (k=0; k < m->couplingSteps; ++k) {
            m->chan[k].magnitude = getBits(f, ilog(f->channels-1));
            m->chan[k].angle = getBits(f, ilog(f->channels-1));
            if (m->chan[k].magnitude >= f->channels)        return stbVorbisError(f, VORBIS_invalidSetup);
            if (m->chan[k].angle     >= f->channels)        return stbVorbisError(f, VORBIS_invalidSetup);
            if (m->chan[k].magnitude == m->chan[k].angle)   return stbVorbisError(f, VORBIS_invalidSetup);
         }
      } else
         m->couplingSteps = 0;

      // reserved field
      if (getBits(f,2)) return stbVorbisError(f, VORBIS_invalidSetup);
      if (m->submaps > 1) {
         for (j=0; j < f->channels; ++j) {
            m->chan[j].mux = getBits(f, 4);
            if (m->chan[j].mux >= m->submaps)                return stbVorbisError(f, VORBIS_invalidSetup);
         }
      } else
         // @SPECIFICATION: this case is missing from the spec
         for (j=0; j < f->channels; ++j)
            m->chan[j].mux = 0;

      for (j=0; j < m->submaps; ++j) {
         getBits(f,8); // discard
         m->submapFloor[j] = getBits(f,8);
         m->submapResidue[j] = getBits(f,8);
         if (m->submapFloor[j] >= f->floorCount)      return stbVorbisError(f, VORBIS_invalidSetup);
         if (m->submapResidue[j] >= f->residueCount)  return stbVorbisError(f, VORBIS_invalidSetup);
      }
   }

   // Modes
   f->modeCount = getBits(f, 6)+1;
   for (i=0; i < f->modeCount; ++i) {
      Mode *m = f->modeConfig+i;
      m->blockflag = getBits(f,1);
      m->windowtype = getBits(f,16);
      m->transformtype = getBits(f,16);
      m->mapping = getBits(f,8);
      if (m->windowtype != 0)                 return stbVorbisError(f, VORBIS_invalidSetup);
      if (m->transformtype != 0)              return stbVorbisError(f, VORBIS_invalidSetup);
      if (m->mapping >= f->mappingCount)     return stbVorbisError(f, VORBIS_invalidSetup);
   }

   flushPacket(f);

   f->previousLength = 0;

   for (i=0; i < f->channels; ++i) {
      f->channelBuffers[i] = (float *) setupMalloc(f, sizeof(float) * f->blocksize_1);
      f->previousWindow[i] = (float *) setupMalloc(f, sizeof(float) * f->blocksize_1/2);
      f->finalY[i]          = (int16 *) setupMalloc(f, sizeof(int16) * longestFloorlist);
      if (f->channelBuffers[i] == NULL || f->previousWindow[i] == NULL || f->finalY[i] == NULL) return stbVorbisError(f, VORBIS_outofmem);
      memset(f->channelBuffers[i], 0, sizeof(float) * f->blocksize_1);
      #ifdef STB_VORBIS_NO_DEFER_FLOOR
      f->floorBuffers[i]   = (float *) setupMalloc(f, sizeof(float) * f->blocksize_1/2);
      if (f->floorBuffers[i] == NULL) return stbVorbisError(f, VORBIS_outofmem);
      #endif
   }

   if (!initBlocksize(f, 0, f->blocksize_0)) return FALSE;
   if (!initBlocksize(f, 1, f->blocksize_1)) return FALSE;
   f->blocksize[0] = f->blocksize_0;
   f->blocksize[1] = f->blocksize_1;

#ifdef STB_VORBIS_DIVIDE_TABLE
   if (integerDivideTable[1][1]==0)
      for (i=0; i < DIVTAB_NUMER; ++i)
         for (j=1; j < DIVTAB_DENOM; ++j)
            integerDivideTable[i][j] = i / j;
#endif

   // compute how much temporary memory is needed

   // 1.
   {
      uint32 imdctMem = (f->blocksize_1 * sizeof(float) >> 1);
      uint32 classifyMem;
      int i,maxPartRead=0;
      for (i=0; i < f->residueCount; ++i) {
         Residue *r = f->residueConfig + i;
         unsigned int actualSize = f->blocksize_1 / 2;
         unsigned int limitR_begin = r->begin < actualSize ? r->begin : actualSize;
         unsigned int limitR_end   = r->end   < actualSize ? r->end   : actualSize;
         int nRead = limitR_end - limitR_begin;
         int partRead = nRead / r->partSize;
         if (partRead > maxPartRead)
            maxPartRead = partRead;
      }
      #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
      classifyMem = f->channels * (sizeof(void*) + maxPartRead * sizeof(uint8 *));
      #else
      classifyMem = f->channels * (sizeof(void*) + maxPartRead * sizeof(int *));
      #endif

      // maximum reasonable partition size is f->blocksize_1

      f->tempMemoryRequired = classifyMem;
      if (imdctMem > f->tempMemoryRequired)
         f->tempMemoryRequired = imdctMem;
   }


   if (f->alloc.allocBuffer) {
      assert(f->tempOffset == f->alloc.allocBufferLengthInBytes);
      // check if there's enough temp memory so we don't error later
      if (f->setupOffset + sizeof(*f) + f->tempMemoryRequired > (unsigned) f->tempOffset)
         return stbVorbisError(f, VORBIS_outofmem);
   }

   // @TODO: stbVorbisSeekStart expects firstAudioPageOffset to point to a page
   // without PAGEFLAG_continuedPacket, so this either points to the first page, or
   // the page after the end of the headers. It might be cleaner to point to a page
   // in the middle of the headers, when that's the page where the first audio packet
   // starts, but we'd have to also correctly skip the end of any continued packet in
   // stbVorbisSeekStart.
   if (f->nextSeg == -1) {
      f->firstAudioPageOffset = stbVorbisGetFileOffset(f);
   } else {
      f->firstAudioPageOffset = 0;
   }

   return TRUE;
}

static void vorbisDeinit(stbVorbis *p)
{
   int i,j;
   if (p->residueConfig) {
      for (i=0; i < p->residueCount; ++i) {
         Residue *r = p->residueConfig+i;
         if (r->classdata) {
            for (j=0; j < p->codebooks[r->classbook].entries; ++j)
               setupFree(p, r->classdata[j]);
            setupFree(p, r->classdata);
         }
         setupFree(p, r->residueBooks);
      }
   }

   if (p->codebooks) {
      CHECK(p);
      for (i=0; i < p->codebookCount; ++i) {
         Codebook *c = p->codebooks + i;
         setupFree(p, c->codewordLengths);
         setupFree(p, c->multiplicands);
         setupFree(p, c->codewords);
         setupFree(p, c->sortedCodewords);
         // c->sortedValues[-1] is the first entry in the array
         setupFree(p, c->sortedValues ? c->sortedValues-1 : NULL);
      }
      setupFree(p, p->codebooks);
   }
   setupFree(p, p->floorConfig);
   setupFree(p, p->residueConfig);
   if (p->mapping) {
      for (i=0; i < p->mappingCount; ++i)
         setupFree(p, p->mapping[i].chan);
      setupFree(p, p->mapping);
   }
   CHECK(p);
   for (i=0; i < p->channels && i < STB_VORBIS_MAX_CHANNELS; ++i) {
      setupFree(p, p->channelBuffers[i]);
      setupFree(p, p->previousWindow[i]);
      #ifdef STB_VORBIS_NO_DEFER_FLOOR
      setupFree(p, p->floorBuffers[i]);
      #endif
      setupFree(p, p->finalY[i]);
   }
   for (i=0; i < 2; ++i) {
      setupFree(p, p->A[i]);
      setupFree(p, p->B[i]);
      setupFree(p, p->C[i]);
      setupFree(p, p->window[i]);
      setupFree(p, p->bitReverse[i]);
   }
   #ifndef STB_VORBIS_NO_STDIO
   if (p->closeOnFree) fclose(p->f);
   #endif
}

void stbVorbisClose(stbVorbis *p)
{
   if (p == NULL) return;
   vorbisDeinit(p);
   setupFree(p,p);
}

static void vorbisInit(stbVorbis *p, const stbVorbisAlloc *z)
{
   memset(p, 0, sizeof(*p)); // NULL out all malloc'd pointers to start
   if (z) {
      p->alloc = *z;
      p->alloc.allocBufferLengthInBytes &= ~7;
      p->tempOffset = p->alloc.allocBufferLengthInBytes;
   }
   p->eof = 0;
   p->error = VORBIS__noError;
   p->stream = NULL;
   p->codebooks = NULL;
   p->pageCrcTests = -1;
   #ifndef STB_VORBIS_NO_STDIO
   p->closeOnFree = FALSE;
   p->f = NULL;
   #endif
}

int stbVorbisGetSampleOffset(stbVorbis *f)
{
   if (f->currentLocValid)
      return f->currentLoc;
   else
      return -1;
}

stbVorbisInfo stbVorbisGetInfo(stbVorbis *f)
{
   stbVorbisInfo d;
   d.channels = f->channels;
   d.sampleRate = f->sampleRate;
   d.setupMemoryRequired = f->setupMemoryRequired;
   d.setupTempMemoryRequired = f->setupTempMemoryRequired;
   d.tempMemoryRequired = f->tempMemoryRequired;
   d.maxFrameSize = f->blocksize_1 >> 1;
   return d;
}

int stbVorbisGetError(stbVorbis *f)
{
   int e = f->error;
   f->error = VORBIS__noError;
   return e;
}

static stbVorbis * vorbisAlloc(stbVorbis *f)
{
   stbVorbis *p = (stbVorbis *) setupMalloc(f, sizeof(*p));
   return p;
}

#ifndef STB_VORBIS_NO_PUSHDATA_API

void stbVorbisFlushPushdata(stbVorbis *f)
{
   f->previousLength = 0;
   f->pageCrcTests  = 0;
   f->discardSamplesDeferred = 0;
   f->currentLocValid = FALSE;
   f->firstDecode = FALSE;
   f->samplesOutput = 0;
   f->channelBufferStart = 0;
   f->channelBufferEnd = 0;
}

static int vorbisSearchForPagePushdata(vorb *f, uint8 *data, int dataLen)
{
   int i,n;
   for (i=0; i < f->pageCrcTests; ++i)
      f->scan[i].bytesDone = 0;

   // if we have room for more scans, search for them first, because
   // they may cause us to stop early if their header is incomplete
   if (f->pageCrcTests < STB_VORBIS_PUSHDATA_CRC_COUNT) {
      if (dataLen < 4) return 0;
      dataLen -= 3; // need to look for 4-byte sequence, so don't miss
                     // one that straddles a boundary
      for (i=0; i < dataLen; ++i) {
         if (data[i] == 0x4f) {
            if (0==memcmp(data+i, oggPageHeader, 4)) {
               int j,len;
               uint32 crc;
               // make sure we have the whole page header
               if (i+26 >= dataLen || i+27+data[i+26] >= dataLen) {
                  // only read up to this page start, so hopefully we'll
                  // have the whole page header start next time
                  dataLen = i;
                  break;
               }
               // ok, we have it all; compute the length of the page
               len = 27 + data[i+26];
               for (j=0; j < data[i+26]; ++j)
                  len += data[i+27+j];
               // scan everything up to the embedded crc (which we must 0)
               crc = 0;
               for (j=0; j < 22; ++j)
                  crc = crc32_update(crc, data[i+j]);
               // now process 4 0-bytes
               for (   ; j < 26; ++j)
                  crc = crc32_update(crc, 0);
               // len is the total number of bytes we need to scan
               n = f->pageCrcTests++;
               f->scan[n].bytesLeft = len-j;
               f->scan[n].crcSoFar = crc;
               f->scan[n].goalCrc = data[i+22] + (data[i+23] << 8) + (data[i+24]<<16) + (data[i+25]<<24);
               // if the last frame on a page is continued to the next, then
               // we can't recover the sampleLoc immediately
               if (data[i+27+data[i+26]-1] == 255)
                  f->scan[n].sampleLoc = ~0;
               else
                  f->scan[n].sampleLoc = data[i+6] + (data[i+7] << 8) + (data[i+ 8]<<16) + (data[i+ 9]<<24);
               f->scan[n].bytesDone = i+j;
               if (f->pageCrcTests == STB_VORBIS_PUSHDATA_CRC_COUNT)
                  break;
               // keep going if we still have room for more
            }
         }
      }
   }

   for (i=0; i < f->pageCrcTests;) {
      uint32 crc;
      int j;
      int n = f->scan[i].bytesDone;
      int m = f->scan[i].bytesLeft;
      if (m > dataLen - n) m = dataLen - n;
      // m is the bytes to scan in the current chunk
      crc = f->scan[i].crcSoFar;
      for (j=0; j < m; ++j)
         crc = crc32_update(crc, data[n+j]);
      f->scan[i].bytesLeft -= m;
      f->scan[i].crcSoFar = crc;
      if (f->scan[i].bytesLeft == 0) {
         // does it match?
         if (f->scan[i].crcSoFar == f->scan[i].goalCrc) {
            // Houston, we have page
            dataLen = n+m; // consumption amount is wherever that scan ended
            f->pageCrcTests = -1; // drop out of page scan mode
            f->previousLength = 0; // decode-but-don't-output one frame
            f->nextSeg = -1;       // start a new page
            f->currentLoc = f->scan[i].sampleLoc; // set the current sample location
                                    // to the amount we'd have decoded had we decoded this page
            f->currentLocValid = f->currentLoc != ~0U;
            return dataLen;
         }
         // delete entry
         f->scan[i] = f->scan[--f->pageCrcTests];
      } else {
         ++i;
      }
   }

   return dataLen;
}

// return value: number of bytes we used
int stbVorbisDecodeFramePushdata(
         stbVorbis *f,                   // the file we're decoding
         const uint8 *data, int dataLen, // the memory available for decoding
         int *channels,                   // place to write number of float * buffers
         float ***output,                 // place to write float ** array of float * buffers
         int *samples                     // place to write number of output samples
     )
{
   int i;
   int len,right,left;

   if (!IS_PUSH_MODE(f)) return stbVorbisError(f, VORBIS_invalidApiMixing);

   if (f->pageCrcTests >= 0) {
      *samples = 0;
      return vorbisSearchForPagePushdata(f, (uint8 *) data, dataLen);
   }

   f->stream     = (uint8 *) data;
   f->streamEnd = (uint8 *) data + dataLen;
   f->error      = VORBIS__noError;

   // check that we have the entire packet in memory
   if (!isWholePacketPresent(f)) {
      *samples = 0;
      return 0;
   }

   if (!vorbisDecodePacket(f, &len, &left, &right)) {
      // save the actual error we encountered
      enum STBVorbisError error = f->error;
      if (error == VORBIS_badPacketType) {
         // flush and resynch
         f->error = VORBIS__noError;
         while (get8_packet(f) != EOP)
            if (f->eof) break;
         *samples = 0;
         return (int) (f->stream - data);
      }
      if (error == VORBIS_continuedPacketFlagInvalid) {
         if (f->previousLength == 0) {
            // we may be resynching, in which case it's ok to hit one
            // of these; just discard the packet
            f->error = VORBIS__noError;
            while (get8_packet(f) != EOP)
               if (f->eof) break;
            *samples = 0;
            return (int) (f->stream - data);
         }
      }
      // if we get an error while parsing, what to do?
      // well, it DEFINITELY won't work to continue from where we are!
      stbVorbisFlushPushdata(f);
      // restore the error that actually made us bail
      f->error = error;
      *samples = 0;
      return 1;
   }

   // success!
   len = vorbisFinishFrame(f, len, left, right);
   for (i=0; i < f->channels; ++i)
      f->outputs[i] = f->channelBuffers[i] + left;

   if (channels) *channels = f->channels;
   *samples = len;
   *output = f->outputs;
   return (int) (f->stream - data);
}

stbVorbis *stbVorbisOpenPushdata(
         const unsigned char *data, int dataLen, // the memory available for decoding
         int *dataUsed,              // only defined if result is not NULL
         int *error, const stbVorbisAlloc *alloc)
{
   stbVorbis *f, p;
   vorbisInit(&p, alloc);
   p.stream     = (uint8 *) data;
   p.streamEnd = (uint8 *) data + dataLen;
   p.pushMode  = TRUE;
   if (!startDecoder(&p)) {
      if (p.eof)
         *error = VORBIS_needMoreData;
      else
         *error = p.error;
      return NULL;
   }
   f = vorbisAlloc(&p);
   if (f) {
      *f = p;
      *dataUsed = (int) (f->stream - data);
      *error = 0;
      return f;
   } else {
      vorbisDeinit(&p);
      return NULL;
   }
}
#endif // STB_VORBIS_NO_PUSHDATA_API

unsigned int stbVorbisGetFileOffset(stbVorbis *f)
{
   #ifndef STB_VORBIS_NO_PUSHDATA_API
   if (f->pushMode) return 0;
   #endif
   if (USE_MEMORY(f)) return (unsigned int) (f->stream - f->streamStart);
   #ifndef STB_VORBIS_NO_STDIO
   return (unsigned int) (ftell(f->f) - f->fStart);
   #endif
}

#ifndef STB_VORBIS_NO_PULLDATA_API
//
// DATA-PULLING API
//

static uint32 vorbisFindPage(stbVorbis *f, uint32 *end, uint32 *last)
{
   for(;;) {
      int n;
      if (f->eof) return 0;
      n = get8(f);
      if (n == 0x4f) { // page header candidate
         unsigned int retryLoc = stbVorbisGetFileOffset(f);
         int i;
         // check if we're off the end of a fileSection stream
         if (retryLoc - 25 > f->streamLen)
            return 0;
         // check the rest of the header
         for (i=1; i < 4; ++i)
            if (get8(f) != oggPageHeader[i])
               break;
         if (f->eof) return 0;
         if (i == 4) {
            uint8 header[27];
            uint32 i, crc, goal, len;
            for (i=0; i < 4; ++i)
               header[i] = oggPageHeader[i];
            for (; i < 27; ++i)
               header[i] = get8(f);
            if (f->eof) return 0;
            if (header[4] != 0) goto invalid;
            goal = header[22] + (header[23] << 8) + (header[24]<<16) + (header[25]<<24);
            for (i=22; i < 26; ++i)
               header[i] = 0;
            crc = 0;
            for (i=0; i < 27; ++i)
               crc = crc32_update(crc, header[i]);
            len = 0;
            for (i=0; i < header[26]; ++i) {
               int s = get8(f);
               crc = crc32_update(crc, s);
               len += s;
            }
            if (len && f->eof) return 0;
            for (i=0; i < len; ++i)
               crc = crc32_update(crc, get8(f));
            // finished parsing probable page
            if (crc == goal) {
               // we could now check that it's either got the last
               // page flag set, OR it's followed by the capture
               // pattern, but I guess TECHNICALLY you could have
               // a file with garbage between each ogg page and recover
               // from it automatically? So even though that paranoia
               // might decrease the chance of an invalid decode by
               // another 2^32, not worth it since it would hose those
               // invalid-but-useful files?
               if (end)
                  *end = stbVorbisGetFileOffset(f);
               if (last) {
                  if (header[5] & 0x04)
                     *last = 1;
                  else
                     *last = 0;
               }
               setFileOffset(f, retryLoc-1);
               return 1;
            }
         }
        invalid:
         // not a valid page, so rewind and look for next one
         setFileOffset(f, retryLoc);
      }
   }
}


#define SAMPLE_unknown  0xffffffff

// seeking is implemented with a binary search, which narrows down the range to
// 64K, before using a linear search (because finding the synchronization
// pattern can be expensive, and the chance we'd find the end page again is
// relatively high for small ranges)
//
// two initial interpolation-style probes are used at the start of the search
// to try to bound either side of the binary search sensibly, while still
// working in O(log n) time if they fail.

static int getSeekPageInfo(stbVorbis *f, ProbedPage *z)
{
   uint8 header[27], lacing[255];
   int i,len;

   // record where the page starts
   z->pageStart = stbVorbisGetFileOffset(f);

   // parse the header
   getn(f, header, 27);
   if (header[0] != 'O' || header[1] != 'g' || header[2] != 'g' || header[3] != 'S')
      return 0;
   getn(f, lacing, header[26]);

   // determine the length of the payload
   len = 0;
   for (i=0; i < header[26]; ++i)
      len += lacing[i];

   // this implies where the page ends
   z->pageEnd = z->pageStart + 27 + header[26] + len;

   // read the last-decoded sample out of the data
   z->lastDecodedSample = header[6] + (header[7] << 8) + (header[8] << 16) + (header[9] << 24);

   // restore file state to where we were
   setFileOffset(f, z->pageStart);
   return 1;
}

// rarely used function to seek back to the preceding page while finding the
// start of a packet
static int goToPageBefore(stbVorbis *f, unsigned int limitOffset)
{
   unsigned int previousSafe, end;

   // now we want to seek back 64K from the limit
   if (limitOffset >= 65536 && limitOffset-65536 >= f->firstAudioPageOffset)
      previousSafe = limitOffset - 65536;
   else
      previousSafe = f->firstAudioPageOffset;

   setFileOffset(f, previousSafe);

   while (vorbisFindPage(f, &end, NULL)) {
      if (end >= limitOffset && stbVorbisGetFileOffset(f) < limitOffset)
         return 1;
      setFileOffset(f, end);
   }

   return 0;
}

// implements the search logic for finding a page and starting decoding. if
// the function succeeds, currentLocValid will be true and currentLoc will
// be less than or equal to the provided sample number (the closer the
// better).
static int seekToSampleCoarse(stbVorbis *f, uint32 sampleNumber)
{
   ProbedPage left, right, mid;
   int i, startSegWithKnownLoc, endPos, pageStart;
   uint32 delta, streamLength, padding, lastSampleLimit;
   double offset = 0.0, bytesPerSample = 0.0;
   int probe = 0;

   // find the last page and validate the target sample
   streamLength = stbVorbisStreamLengthInSamples(f);
   if (streamLength == 0)            return stbVorbisError(f, VORBIS_seekWithoutLength);
   if (sampleNumber > streamLength) return stbVorbisError(f, VORBIS_seekInvalid);

   // this is the maximum difference between the window-center (which is the
   // actual granule position value), and the right-start (which the spec
   // indicates should be the granule position (give or take one)).
   padding = ((f->blocksize_1 - f->blocksize_0) >> 2);
   if (sampleNumber < padding)
      lastSampleLimit = 0;
   else
      lastSampleLimit = sampleNumber - padding;

   left = f->pFirst;
   while (left.lastDecodedSample == ~0U) {
      // (untested) the first page does not have a 'lastDecodedSample'
      setFileOffset(f, left.pageEnd);
      if (!getSeekPageInfo(f, &left)) goto error;
   }

   right = f->pLast;
   assert(right.lastDecodedSample != ~0U);

   // starting from the start is handled differently
   if (lastSampleLimit <= left.lastDecodedSample) {
      if (stbVorbisSeekStart(f)) {
         if (f->currentLoc > sampleNumber)
            return stbVorbisError(f, VORBIS_seekFailed);
         return 1;
      }
      return 0;
   }

   while (left.pageEnd != right.pageStart) {
      assert(left.pageEnd < right.pageStart);
      // search range in bytes
      delta = right.pageStart - left.pageEnd;
      if (delta <= 65536) {
         // there's only 64K left to search - handle it linearly
         setFileOffset(f, left.pageEnd);
      } else {
         if (probe < 2) {
            if (probe == 0) {
               // first probe (interpolate)
               double dataBytes = right.pageEnd - left.pageStart;
               bytesPerSample = dataBytes / right.lastDecodedSample;
               offset = left.pageStart + bytesPerSample * (lastSampleLimit - left.lastDecodedSample);
            } else {
               // second probe (try to bound the other side)
               double error = ((double) lastSampleLimit - mid.lastDecodedSample) * bytesPerSample;
               if (error >= 0 && error <  8000) error =  8000;
               if (error <  0 && error > -8000) error = -8000;
               offset += error * 2;
            }

            // ensure the offset is valid
            if (offset < left.pageEnd)
               offset = left.pageEnd;
            if (offset > right.pageStart - 65536)
               offset = right.pageStart - 65536;

            setFileOffset(f, (unsigned int) offset);
         } else {
            // binary search for large ranges (offset by 32K to ensure
            // we don't hit the right page)
            setFileOffset(f, left.pageEnd + (delta / 2) - 32768);
         }

         if (!vorbisFindPage(f, NULL, NULL)) goto error;
      }

      for (;;) {
         if (!getSeekPageInfo(f, &mid)) goto error;
         if (mid.lastDecodedSample != ~0U) break;
         // (untested) no frames end on this page
         setFileOffset(f, mid.pageEnd);
         assert(mid.pageStart < right.pageStart);
      }

      // if we've just found the last page again then we're in a tricky file,
      // and we're close enough (if it wasn't an interpolation probe).
      if (mid.pageStart == right.pageStart) {
         if (probe >= 2 || delta <= 65536)
            break;
      } else {
         if (lastSampleLimit < mid.lastDecodedSample)
            right = mid;
         else
            left = mid;
      }

      ++probe;
   }

   // seek back to start of the last packet
   pageStart = left.pageStart;
   setFileOffset(f, pageStart);
   if (!startPage(f)) return stbVorbisError(f, VORBIS_seekFailed);
   endPos = f->endSegWithKnownLoc;
   assert(endPos >= 0);

   for (;;) {
      for (i = endPos; i > 0; --i)
         if (f->segments[i-1] != 255)
            break;

      startSegWithKnownLoc = i;

      if (startSegWithKnownLoc > 0 || !(f->pageFlag & PAGEFLAG_continuedPacket))
         break;

      // (untested) the final packet begins on an earlier page
      if (!goToPageBefore(f, pageStart))
         goto error;

      pageStart = stbVorbisGetFileOffset(f);
      if (!startPage(f)) goto error;
      endPos = f->segmentCount - 1;
   }

   // prepare to start decoding
   f->currentLocValid = FALSE;
   f->lastSeg = FALSE;
   f->validBits = 0;
   f->packetBytes = 0;
   f->bytesInSeg = 0;
   f->previousLength = 0;
   f->nextSeg = startSegWithKnownLoc;

   for (i = 0; i < startSegWithKnownLoc; i++)
      skip(f, f->segments[i]);

   // start decoding (optimizable - this frame is generally discarded)
   if (!vorbisPumpFirstFrame(f))
      return 0;
   if (f->currentLoc > sampleNumber)
      return stbVorbisError(f, VORBIS_seekFailed);
   return 1;

error:
   // try to restore the file to a valid state
   stbVorbisSeekStart(f);
   return stbVorbisError(f, VORBIS_seekFailed);
}

// the same as vorbisDecodeInitial, but without advancing
static int peekDecodeInitial(vorb *f, int *pLeftStart, int *pLeftEnd, int *pRightStart, int *pRightEnd, int *mode)
{
   int bitsRead, bytesRead;

   if (!vorbisDecodeInitial(f, pLeftStart, pLeftEnd, pRightStart, pRightEnd, mode))
      return 0;

   // either 1 or 2 bytes were read, figure out which so we can rewind
   bitsRead = 1 + ilog(f->modeCount-1);
   if (f->modeConfig[*mode].blockflag)
      bitsRead += 2;
   bytesRead = (bitsRead + 7) / 8;

   f->bytesInSeg += bytesRead;
   f->packetBytes -= bytesRead;
   skip(f, -bytesRead);
   if (f->nextSeg == -1)
      f->nextSeg = f->segmentCount - 1;
   else
      f->nextSeg--;
   f->validBits = 0;

   return 1;
}

int stbVorbisSeekFrame(stbVorbis *f, unsigned int sampleNumber)
{
   uint32 maxFrameSamples;

   if (IS_PUSH_MODE(f)) return stbVorbisError(f, VORBIS_invalidApiMixing);

   // fast page-level search
   if (!seekToSampleCoarse(f, sampleNumber))
      return 0;

   assert(f->currentLocValid);
   assert(f->currentLoc <= sampleNumber);

   // linear search for the relevant packet
   maxFrameSamples = (f->blocksize_1*3 - f->blocksize_0) >> 2;
   while (f->currentLoc < sampleNumber) {
      int leftStart, leftEnd, rightStart, rightEnd, mode, frameSamples;
      if (!peekDecodeInitial(f, &leftStart, &leftEnd, &rightStart, &rightEnd, &mode))
         return stbVorbisError(f, VORBIS_seekFailed);
      // calculate the number of samples returned by the next frame
      frameSamples = rightStart - leftStart;
      if (f->currentLoc + frameSamples > sampleNumber) {
         return 1; // the next frame will contain the sample
      } else if (f->currentLoc + frameSamples + maxFrameSamples > sampleNumber) {
         // there's a chance the frame after this could contain the sample
         vorbisPumpFirstFrame(f);
      } else {
         // this frame is too early to be relevant
         f->currentLoc += frameSamples;
         f->previousLength = 0;
         maybeStartPacket(f);
         flushPacket(f);
      }
   }
   // the next frame should start with the sample
   if (f->currentLoc != sampleNumber) return stbVorbisError(f, VORBIS_seekFailed);
   return 1;
}

int stbVorbisSeek(stbVorbis *f, unsigned int sampleNumber)
{
   if (!stbVorbisSeekFrame(f, sampleNumber))
      return 0;

   if (sampleNumber != f->currentLoc) {
      int n;
      uint32 frameStart = f->currentLoc;
      stbVorbisGetFrameFloat(f, &n, NULL);
      assert(sampleNumber > frameStart);
      assert(f->channelBufferStart + (int) (sampleNumber-frameStart) <= f->channelBufferEnd);
      f->channelBufferStart += (sampleNumber - frameStart);
   }

   return 1;
}

int stbVorbisSeekStart(stbVorbis *f)
{
   if (IS_PUSH_MODE(f)) { return stbVorbisError(f, VORBIS_invalidApiMixing); }
   setFileOffset(f, f->firstAudioPageOffset);
   f->previousLength = 0;
   f->firstDecode = TRUE;
   f->nextSeg = -1;
   return vorbisPumpFirstFrame(f);
}

unsigned int stbVorbisStreamLengthInSamples(stbVorbis *f)
{
   unsigned int restoreOffset, previousSafe;
   unsigned int end, lastPageLoc;

   if (IS_PUSH_MODE(f)) return stbVorbisError(f, VORBIS_invalidApiMixing);
   if (!f->totalSamples) {
      unsigned int last;
      uint32 lo,hi;
      char header[6];

      // first, store the current decode position so we can restore it
      restoreOffset = stbVorbisGetFileOffset(f);

      // now we want to seek back 64K from the end (the last page must
      // be at most a little less than 64K, but let's allow a little slop)
      if (f->streamLen >= 65536 && f->streamLen-65536 >= f->firstAudioPageOffset)
         previousSafe = f->streamLen - 65536;
      else
         previousSafe = f->firstAudioPageOffset;

      setFileOffset(f, previousSafe);
      // previousSafe is now our candidate 'earliest known place that seeking
      // to will lead to the final page'

      if (!vorbisFindPage(f, &end, &last)) {
         // if we can't find a page, we're hosed!
         f->error = VORBIS_cantFindLastPage;
         f->totalSamples = 0xffffffff;
         goto done;
      }

      // check if there are more pages
      lastPageLoc = stbVorbisGetFileOffset(f);

      // stop when the lastPage flag is set, not when we reach eof;
      // this allows us to stop short of a 'fileSection' end without
      // explicitly checking the length of the section
      while (!last) {
         setFileOffset(f, end);
         if (!vorbisFindPage(f, &end, &last)) {
            // the last page we found didn't have the 'last page' flag
            // set. whoops!
            break;
         }
         previousSafe = lastPageLoc+1;
         lastPageLoc = stbVorbisGetFileOffset(f);
      }

      setFileOffset(f, lastPageLoc);

      // parse the header
      getn(f, (unsigned char *)header, 6);
      // extract the absolute granule position
      lo = get32(f);
      hi = get32(f);
      if (lo == 0xffffffff && hi == 0xffffffff) {
         f->error = VORBIS_cantFindLastPage;
         f->totalSamples = SAMPLE_unknown;
         goto done;
      }
      if (hi)
         lo = 0xfffffffe; // saturate
      f->totalSamples = lo;

      f->pLast.pageStart = lastPageLoc;
      f->pLast.pageEnd   = end;
      f->pLast.lastDecodedSample = lo;

     done:
      setFileOffset(f, restoreOffset);
   }
   return f->totalSamples == SAMPLE_unknown ? 0 : f->totalSamples;
}

float stbVorbisStreamLengthInSeconds(stbVorbis *f)
{
   return stbVorbisStreamLengthInSamples(f) / (float) f->sampleRate;
}



int stbVorbisGetFrameFloat(stbVorbis *f, int *channels, float ***output)
{
   int len, right,left,i;
   if (IS_PUSH_MODE(f)) return stbVorbisError(f, VORBIS_invalidApiMixing);

   if (!vorbisDecodePacket(f, &len, &left, &right)) {
      f->channelBufferStart = f->channelBufferEnd = 0;
      return 0;
   }

   len = vorbisFinishFrame(f, len, left, right);
   for (i=0; i < f->channels; ++i)
      f->outputs[i] = f->channelBuffers[i] + left;

   f->channelBufferStart = left;
   f->channelBufferEnd   = left+len;

   if (channels) *channels = f->channels;
   if (output)   *output = f->outputs;
   return len;
}

#ifndef STB_VORBIS_NO_STDIO

stbVorbis * stbVorbisOpenFileSection(FILE *file, int closeOnFree, int *error, const stbVorbisAlloc *alloc, unsigned int length)
{
   stbVorbis *f, p;
   vorbisInit(&p, alloc);
   p.f = file;
   p.fStart = (uint32) ftell(file);
   p.streamLen   = length;
   p.closeOnFree = closeOnFree;
   if (startDecoder(&p)) {
      f = vorbisAlloc(&p);
      if (f) {
         *f = p;
         vorbisPumpFirstFrame(f);
         return f;
      }
   }
   if (error) *error = p.error;
   vorbisDeinit(&p);
   return NULL;
}

stbVorbis * stbVorbisOpenFile(FILE *file, int closeOnFree, int *error, const stbVorbisAlloc *alloc)
{
   unsigned int len, start;
   start = (unsigned int) ftell(file);
   fseek(file, 0, SEEK_END);
   len = (unsigned int) (ftell(file) - start);
   fseek(file, start, SEEK_SET);
   return stbVorbisOpenFileSection(file, closeOnFree, error, alloc, len);
}

stbVorbis * stbVorbisOpenFilename(const char *filename, int *error, const stbVorbisAlloc *alloc)
{
   FILE *f;
#if defined(_WIN32) && defined(__STDC_WANT_SECURE_LIB__)
   if (0 != fopenS(&f, filename, "rb"))
      f = NULL;
#else
   f = fopen(filename, "rb");
#endif
   if (f)
      return stbVorbisOpenFile(f, TRUE, error, alloc);
   if (error) *error = VORBIS_fileOpenFailure;
   return NULL;
}
#endif // STB_VORBIS_NO_STDIO

stbVorbis * stbVorbisOpenMemory(const unsigned char *data, int len, int *error, const stbVorbisAlloc *alloc)
{
   stbVorbis *f, p;
   if (data == NULL) return NULL;
   vorbisInit(&p, alloc);
   p.stream = (uint8 *) data;
   p.streamEnd = (uint8 *) data + len;
   p.streamStart = (uint8 *) p.stream;
   p.streamLen = len;
   p.pushMode = FALSE;
   if (startDecoder(&p)) {
      f = vorbisAlloc(&p);
      if (f) {
         *f = p;
         vorbisPumpFirstFrame(f);
         if (error) *error = VORBIS__noError;
         return f;
      }
   }
   if (error) *error = p.error;
   vorbisDeinit(&p);
   return NULL;
}

#ifndef STB_VORBIS_NO_INTEGER_CONVERSION
#define PLAYBACK_MONO     1
#define PLAYBACK_LEFT     2
#define PLAYBACK_RIGHT    4

#define L  (PLAYBACK_LEFT  | PLAYBACK_MONO)
#define C  (PLAYBACK_LEFT  | PLAYBACK_RIGHT | PLAYBACK_MONO)
#define R  (PLAYBACK_RIGHT | PLAYBACK_MONO)

static int8 channelPosition[7][6] =
{
   { 0 },
   { C },
   { L, R },
   { L, C, R },
   { L, R, L, R },
   { L, C, R, L, R },
   { L, C, R, L, R, C },
};


#ifndef STB_VORBIS_NO_FAST_SCALED_FLOAT
   typedef union {
      float f;
      int i;
   } floatConv;
   typedef char stbVorbisFloatSizeTest[sizeof(float)==4 && sizeof(int) == 4];
   #define FASTDEF(x) floatConv x
   // add (1<<23) to convert to int, then divide by 2^SHIFT, then add 0.5/2^SHIFT to round
   #define MAGIC(SHIFT) (1.5f * (1 << (23-SHIFT)) + 0.5f/(1 << SHIFT))
   #define ADDEND(SHIFT) (((150-SHIFT) << 23) + (1 << 22))
   #define FAST_SCALED_FLOAT_TO_INT(temp,x,s) (temp.f = (x) + MAGIC(s), temp.i - ADDEND(s))
   #define checkEndianness()
#else
   #define FAST_SCALED_FLOAT_TO_INT(temp,x,s) ((int) ((x) * (1 << (s))))
   #define checkEndianness()
   #define FASTDEF(x)
#endif

static void copySamples(short *dest, float *src, int len)
{
   int i;
   checkEndianness();
   for (i=0; i < len; ++i) {
      FASTDEF(temp);
      int v = FAST_SCALED_FLOAT_TO_INT(temp, src[i],15);
      if ((unsigned int) (v + 32768) > 65535)
         v = v < 0 ? -32768 : 32767;
      dest[i] = v;
   }
}

static void computeSamples(int mask, short *output, int numC, float **data, int dOffset, int len)
{
   #define BUFFER_SIZE  32
   float buffer[BUFFER_SIZE];
   int i,j,o,n = BUFFER_SIZE;
   checkEndianness();
   for (o = 0; o < len; o += BUFFER_SIZE) {
      memset(buffer, 0, sizeof(buffer));
      if (o + n > len) n = len - o;
      for (j=0; j < numC; ++j) {
         if (channelPosition[numC][j] & mask) {
            for (i=0; i < n; ++i)
               buffer[i] += data[j][dOffset+o+i];
         }
      }
      for (i=0; i < n; ++i) {
         FASTDEF(temp);
         int v = FAST_SCALED_FLOAT_TO_INT(temp,buffer[i],15);
         if ((unsigned int) (v + 32768) > 65535)
            v = v < 0 ? -32768 : 32767;
         output[o+i] = v;
      }
   }
}

static void computeStereoSamples(short *output, int numC, float **data, int dOffset, int len)
{
   #define BUFFER_SIZE  32
   float buffer[BUFFER_SIZE];
   int i,j,o,n = BUFFER_SIZE >> 1;
   // o is the offset in the source data
   checkEndianness();
   for (o = 0; o < len; o += BUFFER_SIZE >> 1) {
      // o2 is the offset in the output data
      int o2 = o << 1;
      memset(buffer, 0, sizeof(buffer));
      if (o + n > len) n = len - o;
      for (j=0; j < numC; ++j) {
         int m = channelPosition[numC][j] & (PLAYBACK_LEFT | PLAYBACK_RIGHT);
         if (m == (PLAYBACK_LEFT | PLAYBACK_RIGHT)) {
            for (i=0; i < n; ++i) {
               buffer[i*2+0] += data[j][dOffset+o+i];
               buffer[i*2+1] += data[j][dOffset+o+i];
            }
         } else if (m == PLAYBACK_LEFT) {
            for (i=0; i < n; ++i) {
               buffer[i*2+0] += data[j][dOffset+o+i];
            }
         } else if (m == PLAYBACK_RIGHT) {
            for (i=0; i < n; ++i) {
               buffer[i*2+1] += data[j][dOffset+o+i];
            }
         }
      }
      for (i=0; i < (n<<1); ++i) {
         FASTDEF(temp);
         int v = FAST_SCALED_FLOAT_TO_INT(temp,buffer[i],15);
         if ((unsigned int) (v + 32768) > 65535)
            v = v < 0 ? -32768 : 32767;
         output[o2+i] = v;
      }
   }
}

static void convertSamplesShort(int bufC, short **buffer, int bOffset, int dataC, float **data, int dOffset, int samples)
{
   int i;
   if (bufC != dataC && bufC <= 2 && dataC <= 6) {
      static int channelSelector[3][2] = { {0}, {PLAYBACK_MONO}, {PLAYBACK_LEFT, PLAYBACK_RIGHT} };
      for (i=0; i < bufC; ++i)
         computeSamples(channelSelector[bufC][i], buffer[i]+bOffset, dataC, data, dOffset, samples);
   } else {
      int limit = bufC < dataC ? bufC : dataC;
      for (i=0; i < limit; ++i)
         copySamples(buffer[i]+bOffset, data[i]+dOffset, samples);
      for (   ; i < bufC; ++i)
         memset(buffer[i]+bOffset, 0, sizeof(short) * samples);
   }
}

int stbVorbisGetFrameShort(stbVorbis *f, int numC, short **buffer, int numSamples)
{
   float **output = NULL;
   int len = stbVorbisGetFrameFloat(f, NULL, &output);
   if (len > numSamples) len = numSamples;
   if (len)
      convertSamplesShort(numC, buffer, 0, f->channels, output, 0, len);
   return len;
}

static void convertChannelsShortInterleaved(int bufC, short *buffer, int dataC, float **data, int dOffset, int len)
{
   int i;
   checkEndianness();
   if (bufC != dataC && bufC <= 2 && dataC <= 6) {
      assert(bufC == 2);
      for (i=0; i < bufC; ++i)
         computeStereoSamples(buffer, dataC, data, dOffset, len);
   } else {
      int limit = bufC < dataC ? bufC : dataC;
      int j;
      for (j=0; j < len; ++j) {
         for (i=0; i < limit; ++i) {
            FASTDEF(temp);
            float f = data[i][dOffset+j];
            int v = FAST_SCALED_FLOAT_TO_INT(temp, f,15);//data[i][dOffset+j],15);
            if ((unsigned int) (v + 32768) > 65535)
               v = v < 0 ? -32768 : 32767;
            *buffer++ = v;
         }
         for (   ; i < bufC; ++i)
            *buffer++ = 0;
      }
   }
}

int stbVorbisGetFrameShortInterleaved(stbVorbis *f, int numC, short *buffer, int numShorts)
{
   float **output;
   int len;
   if (numC == 1) return stbVorbisGetFrameShort(f,numC,&buffer, numShorts);
   len = stbVorbisGetFrameFloat(f, NULL, &output);
   if (len) {
      if (len*numC > numShorts) len = numShorts / numC;
      convertChannelsShortInterleaved(numC, buffer, f->channels, output, 0, len);
   }
   return len;
}

int stbVorbisGetSamplesShortInterleaved(stbVorbis *f, int channels, short *buffer, int numShorts)
{
   float **outputs;
   int len = numShorts / channels;
   int n=0;
   int z = f->channels;
   if (z > channels) z = channels;
   while (n < len) {
      int k = f->channelBufferEnd - f->channelBufferStart;
      if (n+k >= len) k = len - n;
      if (k)
         convertChannelsShortInterleaved(channels, buffer, f->channels, f->channelBuffers, f->channelBufferStart, k);
      buffer += k*channels;
      n += k;
      f->channelBufferStart += k;
      if (n == len) break;
      if (!stbVorbisGetFrameFloat(f, NULL, &outputs)) break;
   }
   return n;
}

int stbVorbisGetSamplesShort(stbVorbis *f, int channels, short **buffer, int len)
{
   float **outputs;
   int n=0;
   int z = f->channels;
   if (z > channels) z = channels;
   while (n < len) {
      int k = f->channelBufferEnd - f->channelBufferStart;
      if (n+k >= len) k = len - n;
      if (k)
         convertSamplesShort(channels, buffer, n, f->channels, f->channelBuffers, f->channelBufferStart, k);
      n += k;
      f->channelBufferStart += k;
      if (n == len) break;
      if (!stbVorbisGetFrameFloat(f, NULL, &outputs)) break;
   }
   return n;
}

#ifndef STB_VORBIS_NO_STDIO
int stbVorbisDecodeFilename(const char *filename, int *channels, int *sampleRate, short **output)
{
   int dataLen, offset, total, limit, error;
   short *data;
   stbVorbis *v = stbVorbisOpenFilename(filename, &error, NULL);
   if (v == NULL) return -1;
   limit = v->channels * 4096;
   *channels = v->channels;
   if (sampleRate)
      *sampleRate = v->sampleRate;
   offset = dataLen = 0;
   total = limit;
   data = (short *) malloc(total * sizeof(*data));
   if (data == NULL) {
      stbVorbisClose(v);
      return -2;
   }
   for (;;) {
      int n = stbVorbisGetFrameShortInterleaved(v, v->channels, data+offset, total-offset);
      if (n == 0) break;
      dataLen += n;
      offset += n * v->channels;
      if (offset + limit > total) {
         short *data2;
         total *= 2;
         data2 = (short *) realloc(data, total * sizeof(*data));
         if (data2 == NULL) {
            free(data);
            stbVorbisClose(v);
            return -2;
         }
         data = data2;
      }
   }
   *output = data;
   stbVorbisClose(v);
   return dataLen;
}
#endif // NO_STDIO

int stbVorbisDecodeMemory(const uint8 *mem, int len, int *channels, int *sampleRate, short **output)
{
   int dataLen, offset, total, limit, error;
   short *data;
   stbVorbis *v = stbVorbisOpenMemory(mem, len, &error, NULL);
   if (v == NULL) return -1;
   limit = v->channels * 4096;
   *channels = v->channels;
   if (sampleRate)
      *sampleRate = v->sampleRate;
   offset = dataLen = 0;
   total = limit;
   data = (short *) malloc(total * sizeof(*data));
   if (data == NULL) {
      stbVorbisClose(v);
      return -2;
   }
   for (;;) {
      int n = stbVorbisGetFrameShortInterleaved(v, v->channels, data+offset, total-offset);
      if (n == 0) break;
      dataLen += n;
      offset += n * v->channels;
      if (offset + limit > total) {
         short *data2;
         total *= 2;
         data2 = (short *) realloc(data, total * sizeof(*data));
         if (data2 == NULL) {
            free(data);
            stbVorbisClose(v);
            return -2;
         }
         data = data2;
      }
   }
   *output = data;
   stbVorbisClose(v);
   return dataLen;
}
#endif // STB_VORBIS_NO_INTEGER_CONVERSION

int stbVorbisGetSamplesFloatInterleaved(stbVorbis *f, int channels, float *buffer, int numFloats)
{
   float **outputs;
   int len = numFloats / channels;
   int n=0;
   int z = f->channels;
   if (z > channels) z = channels;
   while (n < len) {
      int i,j;
      int k = f->channelBufferEnd - f->channelBufferStart;
      if (n+k >= len) k = len - n;
      for (j=0; j < k; ++j) {
         for (i=0; i < z; ++i)
            *buffer++ = f->channelBuffers[i][f->channelBufferStart+j];
         for (   ; i < channels; ++i)
            *buffer++ = 0;
      }
      n += k;
      f->channelBufferStart += k;
      if (n == len)
         break;
      if (!stbVorbisGetFrameFloat(f, NULL, &outputs))
         break;
   }
   return n;
}

int stbVorbisGetSamplesFloat(stbVorbis *f, int channels, float **buffer, int numSamples)
{
   float **outputs;
   int n=0;
   int z = f->channels;
   if (z > channels) z = channels;
   while (n < numSamples) {
      int i;
      int k = f->channelBufferEnd - f->channelBufferStart;
      if (n+k >= numSamples) k = numSamples - n;
      if (k) {
         for (i=0; i < z; ++i)
            memcpy(buffer[i]+n, f->channelBuffers[i]+f->channelBufferStart, sizeof(float)*k);
         for (   ; i < channels; ++i)
            memset(buffer[i]+n, 0, sizeof(float) * k);
      }
      n += k;
      f->channelBufferStart += k;
      if (n == numSamples)
         break;
      if (!stbVorbisGetFrameFloat(f, NULL, &outputs))
         break;
   }
   return n;
}
#endif // STB_VORBIS_NO_PULLDATA_API

/* Version history
    1.17    - 2019-07-08 - fix CVE-2019-13217, -13218, -13219, -13220, -13221, -13222, -13223
                           found with Mayhem by ForAllSecure
    1.16    - 2019-03-04 - fix warnings
    1.15    - 2019-02-07 - explicit failure if Ogg Skeleton data is found
    1.14    - 2018-02-11 - delete bogus dealloca usage
    1.13    - 2018-01-29 - fix truncation of last frame (hopefully)
    1.12    - 2017-11-21 - limit residue begin/end to blocksize/2 to avoid large temp allocs in bad/corrupt files
    1.11    - 2017-07-23 - fix MinGW compilation
    1.10    - 2017-03-03 - more robust seeking; fix negative ilog(); clear error in openMemory
    1.09    - 2016-04-04 - back out 'avoid discarding last frame' fix from previous version
    1.08    - 2016-04-02 - fixed multiple warnings; fix setup memory leaks;
                           avoid discarding last frame of audio data
    1.07    - 2015-01-16 - fixed some warnings, fix mingw, const-correct API
                           some more crash fixes when out of memory or with corrupt files
    1.06    - 2015-08-31 - full, correct support for seeking API (Dougall Johnson)
                           some crash fixes when out of memory or with corrupt files
    1.05    - 2015-04-19 - don't define __forceinline if it's redundant
    1.04    - 2014-08-27 - fix missing const-correct case in API
    1.03    - 2014-08-07 - Warning fixes
    1.02    - 2014-07-09 - Declare qsort compare function _cdecl on windows
    1.01    - 2014-06-18 - fix stbVorbisGetSamplesFloat
    1.0     - 2014-05-26 - fix memory leaks; fix warnings; fix bugs in multichannel
                           (API change) report sample rate for decode-full-file funcs
    0.99996 - bracket #include <malloc.h> for macintosh compilation by Laurent Gomila
    0.99995 - use union instead of pointer-cast for fast-float-to-int to avoid alias-optimization problem
    0.99994 - change fast-float-to-int to work in single-precision FPU mode, remove endian-dependence
    0.99993 - remove assert that fired on legal files with empty tables
    0.99992 - rewind-to-start
    0.99991 - bugfix to stbVorbisGetSamplesShort by Bernhard Wodo
    0.9999 - (should have been 0.99990) fix no-CRT support, compiling as C++
    0.9998 - add a full-decode function with a memory source
    0.9997 - fix a bug in the read-from-FILE case in 0.9996 addition
    0.9996 - query length of vorbis stream in samples/seconds
    0.9995 - bugfix to another optimization that only happened in certain files
    0.9994 - bugfix to one of the optimizations that caused significant (but inaudible?) errors
    0.9993 - performance improvements; runs in 99% to 104% of time of reference implementation
    0.9992 - performance improvement of IMDCT; now performs close to reference implementation
    0.9991 - performance improvement of IMDCT
    0.999 - (should have been 0.9990) performance improvement of IMDCT
    0.998 - no-CRT support from Casey Muratori
    0.997 - bugfixes for bugs found by Terje Mathisen
    0.996 - bugfix: fast-huffman decode initialized incorrectly for sparse codebooks; fixing gives 10% speedup - found by Terje Mathisen
    0.995 - bugfix: fix to 'effective' overrun detection - found by Terje Mathisen
    0.994 - bugfix: garbage decode on final VQ symbol of a non-multiple - found by Terje Mathisen
    0.993 - bugfix: pushdata API required 1 extra byte for empty page (failed to consume final page if empty) - found by Terje Mathisen
    0.992 - fixes for MinGW warning
    0.991 - turn fast-float-conversion on by default
    0.990 - fix push-mode seek recovery if you seek into the headers
    0.98b - fix to bad release of 0.98
    0.98 - fix push-mode seek recovery; robustify float-to-int and support non-fast mode
    0.97 - builds under c++ (typecasting, don't use 'class' keyword)
    0.96 - somehow MY 0.95 was right, but the web one was wrong, so here's my 0.95 rereleased as 0.96, fixes a typo in the clamping code
    0.95 - clamping code for 16-bit functions
    0.94 - not publically released
    0.93 - fixed all-zero-floor case (was decoding garbage)
    0.92 - fixed a memory leak
    0.91 - conditional compiles to omit parts of the API and the infrastructure to support them: STB_VORBIS_NO_PULLDATA_API, STB_VORBIS_NO_PUSHDATA_API, STB_VORBIS_NO_STDIO, STB_VORBIS_NO_INTEGER_CONVERSION
    0.90 - first public release
*/

#endif // STB_VORBIS_HEADER_ONLY


/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
#endif
