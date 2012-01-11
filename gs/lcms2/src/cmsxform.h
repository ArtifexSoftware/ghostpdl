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
//   UNPACK(T,TO,FROM,SIZE) (Opt)   code to unpack input data (T = Transform
//                                  TO = buffer to unpack into, FROM = data,
//                                  SIZE = size of data)
//   PACK(T,FROM,TO,SIZE)   (Opt)   code to pack transformed input data
//                                  (T = Transform, FROM = transformed data,
//                                  TO = output buffer to pack into,
//                                  SIZE = size of data)
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
#define COMPARE(A,B) (((cmsUInt32Number *)(A))[0] != ((cmsUInt32Number *)(B))[0])
#elif INBYTES <= 8
// Either of these 2 defines should be fine. Need to look at the assembled
// code to see which is faster.
//#define COMPARE(A,B) (*((cmsUInt64Number *)(A)) != *((cmsUInt64Number *)(B)))
#define COMPARE(A,B) ((((cmsUInt32Number *)(A))[0] != ((cmsUInt32Number *)(B))[0]) || (((cmsUInt32Number *)(A))[1] != ((cmsUInt32Number *)(B))[1]))
#else
#undef INBYTES
#endif
#endif

#ifndef COMPARE
#define COMPARE(A,B) memcmp((A),(B), (sizeof(cmsUInt16Number)*cmsMAXCHANNELS))
#endif

#ifndef UNPACK
#ifdef UNPACKFN
#define UNPACK(T,TO,FROM,SIZE) \
                  do { (FROM) = UNPACKFN((T),(TO),(FROM),(SIZE)); } while (0)
#else
#define UNPACK(T,TO,FROM,SIZE) \
                  do { (FROM) = (T)->FromInput((T),(TO),(FROM),(SIZE)); } while (0)
#endif
#endif

#ifndef PACK
#ifdef PACKFN
#define PACK(T,FROM,TO,SIZE) \
                  do { (TO) = PACKFN((T),(FROM),(TO),(SIZE)); } while (0)
#else
#define PACK(T,FROM,TO,SIZE) \
                  do { (TO) = (T)->ToOutput((T),(FROM),(TO),(SIZE)); } while (0)
#endif
#endif

static
void FUNCTION_NAME(_cmsTRANSFORM* p,
                   const void* in,
                   void* out,
                   cmsUInt32Number Size)
{
    cmsUInt8Number* accum  = (cmsUInt8Number*) in;
    cmsUInt8Number* output = (cmsUInt8Number*) out;
    cmsUInt64Number wIn[cmsMAXCHANNELS*2*sizeof(cmsUInt16Number)/sizeof(cmsUInt64Number)];
#define wIn0 (&((cmsUInt16Number *)wIn)[0])
#define wIn1 (&((cmsUInt16Number *)wIn)[cmsMAXCHANNELS])
    cmsUInt16Number *currIn;
#ifdef CACHED
    cmsUInt16Number *prevIn;
#endif /* CACHED */
    cmsUInt16Number wOut[cmsMAXCHANNELS];
    cmsUInt32Number n = Size;
#ifdef GAMUTCHECK
    _cmsOPTeval16Fn evalGamut = p ->GamutCheck ->Eval16Fn;
#endif /* GAMUTCHECK */
    _cmsOPTeval16Fn eval = p ->Lut ->Eval16Fn;

    if (n == 0)
        return;

#ifdef CACHED
    // Empty buffers for quick memcmp
    memset(wIn1, 0, sizeof(cmsUInt16Number) * cmsMAXCHANNELS);

    // Get copy of zero cache
    memcpy(wIn0, p->Cache.CacheIn,  sizeof(cmsUInt16Number) * cmsMAXCHANNELS);
    memcpy(wOut, p->Cache.CacheOut, sizeof(cmsUInt16Number) * cmsMAXCHANNELS);

    // The caller guarantees us that the cache is always valid on entry; if
    // the representation is changed, the cache is reset.
    prevIn = wIn0;
#endif /* CACHED */
    currIn = wIn1;

    do { // prevIn == CacheIn, wOut = CacheOut
        UNPACK(p,currIn,accum,Size);
#ifdef CACHED
        if (COMPARE(currIn, prevIn))
#endif /* CACHED */
        {
#ifdef GAMUTCHECK
            cmsUInt16Number wOutOfGamut;

            evalGamut(currIn, &wOutOfGamut, p->GamutCheck->Data);
            if (wOutOfGamut >= 1)
                memcpy(wOut, Alarm, sizeof(cmsUInt16Number) * cmsMAXCHANNELS);
            else
#endif /* GAMUTCHECK */
                eval(currIn, wOut, p->Lut->Data);
#ifdef GAMUTCHECK
#endif /* GAMUTCHECK */
#ifdef CACHED
            {cmsUInt16Number *tmp = currIn; currIn = prevIn; prevIn = tmp;} // SWAP
#endif /* CACHED */
        }
        PACK(p,wOut,output,Size);
    } while (--n);
#ifdef CACHED
    memcpy(prevIn, p->Cache.CacheIn, sizeof(cmsUInt16Number) * cmsMAXCHANNELS);
    memcpy(wOut, p->Cache.CacheOut, sizeof(cmsUInt16Number) * cmsMAXCHANNELS);
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
