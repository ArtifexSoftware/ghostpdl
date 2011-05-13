//
// Little cms

// Chameleonic header file to instantiate different versions of the
// transform routines.
//
// As a bare minimum the following must be defined on entry:
//   FUNCTION_NAME             the name of the function
//
// In addition, a range of other symbols can be optionally defined on entry
// to make the generated code more efficient. All these symbols (and
// FUNCTION_NAME) will be automatically undefined at the end of the file so
// that repeated #includes of this file are made simple.
//
// If caching is wanted, define CACHED. (INBYTES will only have an effect
// if CACHED is defined).
//
// To reduce the amount of surplus memory checking done, set INBYTES to the
// number of bytes in an unpacked data chunk.
//
// If you know the code to be used to unpack (or pack, or both) data to/from
// the simple 16 bit transform input/output format, then you can choose
// to this directly by defining UNPACK/PACK macros as follows:
//   UNPACK(T,TO,FROM) (Opt)   code to unpack input data (T = Transform
//                             TO = buffer to unpack into, FROM = data)
//   PACK(T,FROM,TO)   (Opt)   code to pack transformed input data
//                             (T = Transform, FROM = transformed data,
//                             TO = output buffer to pack into)
//
// As an alternative to the above, if you know the function name that would
// be called, supply that in UNPACKFN and PACKFN and inlining compilers
// should hopefully do the hard work for you.
//   UNPACKFN          (Opt)   function to unpack input data
//   PACKFN            (Opt)   function to pack input data
//
// Finally, GAMUTCHECK can be predefined if a gamut check needs to be done.

#ifndef CACHED
#undef INBYTES
#endif

#ifdef INBYTES
#if INBYTES <= 4
#define COMPARE(A,B) (*((int *)(A)) != *((int *)(B)))
#elif INBYTES <= 8
// Either of these 2 defines should be fine. Need to look at the assembled
// code to see which is faster.
//#define COMPARE(A,B) (*((LCMSULONGLONG *)(A)) != *((LCMSULONGLONG *)(B)))
#define COMPARE(A,B) ((((int *)(A))[0] != ((int *)(B))[0]) || (((int *)(A))[1] != ((int *)(B))[1]))
#else
#undef INBYTES
#endif
#endif

#ifndef COMPARE
#define COMPARE(A,B) memcmp((A),(B), (sizeof(WORD)*MAXCHANNELS))
#endif

#ifndef UNPACK
#ifdef UNPACKFN
#define UNPACK(T,TO,FROM) \
                  do { (FROM) = UNPACKFN((T),(TO),(FROM)); } while (0)
#else
#define UNPACK(T,TO,FROM) \
                  do { (FROM) = (T)->FromInput((T),(TO),(FROM)); } while (0)
#endif
#endif

#ifndef PACK
#ifdef PACKFN
#define PACK(T,FROM,TO) \
                  do { (TO) = PACKFN((T),(FROM),(TO)); } while (0)
#else
#define PACK(T,FROM,TO) \
                  do { (TO) = (T)->ToOutput((T),(FROM),(TO)); } while (0)
#endif
#endif

static
void FUNCTION_NAME(_LPcmsTRANSFORM p,
                   LPVOID in,
                   LPVOID out,
                   unsigned int n)
{
    register LPBYTE accum;
    register LPBYTE output;
    LCMSULONGLONG wIn[sizeof(WORD)*MAXCHANNELS*2/sizeof(LCMSULONGLONG)];
#define wIn0 (&((WORD *)wIn)[0])
#define wIn1 (&((WORD *)wIn)[MAXCHANNELS])
    WORD *currIn;
#ifdef CACHED
    WORD *prevIn;
#endif /* CACHED */
    WORD wOut[MAXCHANNELS];
    LPLUT devLink = p->DeviceLink;
    accum  = (LPBYTE) in;
    output = (LPBYTE) out;

    if (n == 0)
        return;

#ifdef CACHED
    // Empty buffers for quick memcmp
    ZeroMemory(wIn1, sizeof(WORD) * MAXCHANNELS);

    LCMS_READ_LOCK(&p ->rwlock);
    CopyMemory(wIn0, p ->CacheIn,  sizeof(WORD) * MAXCHANNELS);
    CopyMemory(wOut, p ->CacheOut, sizeof(WORD) * MAXCHANNELS);
    LCMS_UNLOCK(&p ->rwlock);

    // The caller guarantees us that the cache is always valid on entry; if
    // the representation is changed, the cache is reset.

    prevIn = wIn0;
#endif /* CACHED */
    currIn = wIn1;

    // Try to speedup things on plain devicelinks
#ifndef GAMUTCHECK
    if (devLink->wFlags == LUT_HAS3DGRID) {
        do {
            UNPACK(p,currIn,accum);
#ifdef CACHED
            if (COMPARE(currIn, prevIn))
#endif /* CACHED */
            {
                devLink->CLut16params.Interp3D(currIn, wOut,
                                               devLink -> T,
                                               &devLink -> CLut16params);
#ifdef CACHED
                {WORD *tmp = currIn; currIn = prevIn; prevIn = tmp;}
#endif /* CACHED */
            }
            PACK(p,wOut,output);
        } while (--n);
    }
    else
#endif /* GAMUTCHECK */
    {
        do {
            UNPACK(p,currIn,accum);
#ifdef CACHED
            if (COMPARE(currIn, prevIn))
#endif /* CACHED */
            {
#ifdef GAMUTCHECK
                {
                    WORD wOutOfGamut;

                    cmsEvalLUT(p->GamutCheck, currIn, &wOutOfGamut);
                    if (wOutOfGamut >= 1) {
                        ZeroMemory(wOut, sizeof(WORD) * MAXCHANNELS);

                        wOut[0] = _cmsAlarmR;
                        wOut[1] = _cmsAlarmG;
                        wOut[2] = _cmsAlarmB;
                    } else {
#endif /* GAMUTCHECK */
                        cmsEvalLUT(devLink, currIn, wOut);
#ifdef GAMUTCHECK
                    }
                }
#endif /* GAMUTCHECK */
#ifdef CACHED
                {WORD *tmp = currIn; currIn = prevIn; prevIn = tmp;}
#endif /* CACHED */
            }
            PACK(p,wOut,output);
        } while (--n);
    }
#ifdef CACHED
    LCMS_WRITE_LOCK(&p ->rwlock);
    CopyMemory(p->CacheIn,  prevIn, sizeof(WORD) * MAXCHANNELS);
    CopyMemory(p->CacheOut, wOut,   sizeof(WORD) * MAXCHANNELS);
    LCMS_UNLOCK(&p ->rwlock);
#endif /* CACHED */
}

#undef wIn0
#undef wIn1

#undef FUNCTION_NAME
#undef COMPARE
#undef INBYTES
#undef UNPACK
#undef PACK
#undef UNPACKFN
#undef PACKFN
#undef GAMUTCHECK
#undef CACHED
