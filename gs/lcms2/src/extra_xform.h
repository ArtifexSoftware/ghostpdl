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
// If caching is wanted, define CACHED.
//
// To reduce the amount of surplus memory checking done, set INBYTES to the
// number of bytes in an unpacked data chunk. INBYTES will only have an
// effect if CACHED or NO_UNPACK
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
// If the data happens to be in the correct input format anyway, we can skip
// unpacking it entirely and just use it direct.
//   NO_UNPACK         (Opt)   if defined, transform direct from the input
//                             data. INBYTES must be defined.
//
// If the data happens to be in the correct output format anyway, we can skip
// packing it entirely and just transform it direct into the buffer.
//   NO_PACK           (Opt)   if defined, transform direct to the output
//                             data. OUTBYTES must be defined.
//   COPY_MATCHED(FROM,TO)(Opt)if defined, copy output values from FROM to
//                             TO. Used in the case CACHED case where the
//                             cache matches and we have to copy forwards.
//
// Finally, GAMUTCHECK can be predefined if a gamut check needs to be done.

#ifdef INBYTES
// If we are unpacking, then we know the unpacked buffer is aligned suitably
// for us to be able to do 'int' based checks. Faster than calling memcpy.
#if INBYTES <= 4 && !defined(NO_UNPACK)
#define COMPARE(A,B) (((cmsUInt32Number *)(A))[0] != ((cmsUInt32Number *)(B))[0])
#elif INBYTES <= 8 && !defined(NO_UNPACK)
// Either of these 2 defines should be fine. Need to look at the assembled
// code to see which is faster.
//#define COMPARE(A,B) (*((cmsUInt64Number *)(A)) != *((cmsUInt64Number *)(B)))
#define COMPARE(A,B) ((((cmsUInt32Number *)(A))[0] != ((cmsUInt32Number *)(B))[0]) || (((cmsUInt32Number *)(A))[1] != ((cmsUInt32Number *)(B))[1]))
#elif INBYTES == 2
#define COMPARE(A,B) ((A)[0] != (B)[0])
#elif INBYTES == 6
#define COMPARE(A,B) (((A)[0] != (B)[0]) || ((A)[1] != (B)[1]) || ((A)[2] != (B)[2]))
#elif INBYTES == 8
#define COMPARE(A,B) (((A)[0] != (B)[0]) || ((A)[1] != (B)[1]) || ((A)[2] != (B)[2]) || ((A)[3] != (B)[3]))
#endif
#else
// Otherwise, set INBYTES to be the maximum size it could possibly be.
#define INBYTES (sizeof(cmsUInt16Number)*cmsMAXCHANNELS)
#endif

#ifndef COMPARE
#define COMPARE(A,B) memcmp((A),(B), INBYTES)
#endif

#if   defined(UNPACK)
// Nothing to do, UNPACK is already defined
#elif defined(NO_UNPACK)
#define UNPACK(T,TO,FROM,STRIDE) do { } while (0)
#elif defined(UNPACKFN)
#define UNPACK(T,TO,FROM,STRIDE) \
    do { (FROM) = UNPACKFN((T),(TO),(FROM),(STRIDE)); } while (0)
#elif defined(XFORM_FLOAT)
#define UNPACK(T,TO,FROM,STRIDE) \
    do { (FROM) = (T)->FromInputFloat((T),(TO),(FROM),(STRIDE)); } while (0)
#else
#define UNPACK(T,TO,FROM,STRIDE) \
    do { (FROM) = (T)->FromInput((T),(TO),(FROM),(STRIDE)); } while (0)
#endif

#if defined(PACK)
// Nothing to do, PACK is already defined
#elif defined(NO_PACK)
#define PACK(T,FROM,TO,STRIDE) \
     do { (FROM) += ((OUTBYTES)/sizeof(XFORM_TYPE)); } while (0)
#elif defined(PACKFN)
#define PACK(T,FROM,TO,STRIDE) \
     do { (TO) = PACKFN((T),(FROM),(TO),(STRIDE)); } while (0)
#elif defined(XFORM_FLOAT)
#define PACK(T,FROM,TO,STRIDE) \
     do { (TO) = (T)->ToOutputFloat((T),(FROM),(TO),(STRIDE)); } while (0)
#else
#define PACK(T,FROM,TO,STRIDE) \
     do { (TO) = (T)->ToOutput((T),(FROM),(TO),(STRIDE)); } while (0)
#endif

#if defined(NO_PACK) && !defined(COPY_MATCHED)
#if (defined(XFORM_FLOAT) && OUTBYTES == 4) || OUTBYTES == 2
#define COPY_MATCHED(FROM,TO) ((TO)[0] = (FROM)[0])
#elif (defined(XFORM_FLOAT) && OUTBYTES == 8) || OUTBYTES == 4
#define COPY_MATCHED(FROM,TO) ((TO)[0] = (FROM)[0],(TO)[1] = (FROM)[1])
#elif (defined(XFORM_FLOAT) && OUTBYTES == 12) || OUTBYTES == 6
#define COPY_MATCHED(FROM,TO) ((TO)[0] = (FROM)[0],(TO)[1] = (FROM)[1],(TO)[2] = (FROM)[2])
#elif (defined(XFORM_FLOAT) && OUTBYTES == 16) || OUTBYTES == 8
#define COPY_MATCHED(FROM,TO) ((TO)[0] = (FROM)[0],(TO)[1] = (FROM)[1],(TO)[2] = (FROM)[2],(TO)[3] = (FROM)[3])
#else
#define COPY_MATCHED(FROM,TO) memcpy((TO),(FROM),(OUTBYTES))
#endif
#endif

#ifdef XFORM_FLOAT
#define XFORM_TYPE cmsFloat32Number
#else
#define XFORM_TYPE cmsUInt16Number
#endif

static
void FUNCTION_NAME(_cmsTRANSFORM* p,
                   const void* in,
                   void* out,
                   cmsUInt32Number Size,
                   cmsUInt32Number Stride)
{
    cmsUInt8Number* accum  = (cmsUInt8Number*) in;
    cmsUInt8Number* output = (cmsUInt8Number*) out;
#ifndef NO_UNPACK
#ifdef XFORM_FLOAT
    cmsFloat32Number wIn[cmsMAXCHANNELS*2];
#else
    cmsUInt64Number wIn[cmsMAXCHANNELS*2*sizeof(cmsUInt16Number)/sizeof(cmsUInt64Number)];
#endif
#define wIn0 (&((XFORM_TYPE *)wIn)[0])
#define wIn1 (&((XFORM_TYPE *)wIn)[cmsMAXCHANNELS])
#endif
    XFORM_TYPE *currIn;
#ifdef CACHED
    XFORM_TYPE *prevIn;
#endif /* CACHED */
#ifdef NO_PACK
    XFORM_TYPE *wOut = (XFORM_TYPE *)out;
    XFORM_TYPE *prevOut = (XFORM_TYPE *)p->Cache.CacheOut;
#else
    XFORM_TYPE wOut[cmsMAXCHANNELS];
#endif
    cmsUInt32Number n = Size;
#ifdef GAMUTCHECK
    _cmsOPTeval16Fn evalGamut = p ->GamutCheck ->Eval16Fn;
#endif /* GAMUTCHECK */
#ifdef XFORM_FLOAT
    _cmsPipelineEvalFloatFn eval = p -> Lut -> EvalFloatFn;
    const cmsPipeline *data = p->Lut;
#else
    _cmsOPTeval16Fn eval = p ->Lut -> Eval16Fn;
    void *data = p -> Lut -> Data;
#endif

    if (n == 0)
        return;

#ifdef NO_UNPACK
    currIn = (XFORM_TYPE *)accum;
    prevIn = (XFORM_TYPE *)p->Cache.CacheIn;
#else
#ifdef CACHED
    // Empty buffers for quick memcmp
    memset(wIn1, 0, sizeof(XFORM_TYPE) * cmsMAXCHANNELS);

    // Get copy of zero cache
    memcpy(wIn0, p->Cache.CacheIn,  sizeof(XFORM_TYPE) * cmsMAXCHANNELS);
    memcpy(wOut, p->Cache.CacheOut, sizeof(XFORM_TYPE) * cmsMAXCHANNELS);

    // The caller guarantees us that the cache is always valid on entry; if
    // the representation is changed, the cache is reset.
    prevIn = wIn0;
#endif /* CACHED */
    currIn = wIn1;
#endif

    do { // prevIn == CacheIn, wOut = CacheOut
        UNPACK(p,currIn,accum,Stride);
#ifdef CACHED
        if (COMPARE(currIn, prevIn))
#endif /* CACHED */
        {
#ifdef GAMUTCHECK
#ifdef XFORM_FLOAT
            cmsFloat32Number OutOfGamut;

            // Evaluate gamut marker.
            cmsPipelineEvalFloat( currIn, &OutOfGamut, p ->GamutCheck);

            // Is current color out of gamut?
            if (OutOfGamut > 0.0) {

                // Certainly, out of gamut
                for (j=0; j < cmsMAXCHANNELS; j++)
                    fOut[j] = -1.0;

            }
            else
#else
            cmsUInt16Number wOutOfGamut;

            evalGamut(currIn, &wOutOfGamut, p->GamutCheck->Data);
            if (wOutOfGamut >= 1)
                /* RJW: Could be faster? copy once to a local buffer? */
                cmsGetAlarmCodes(wOut); 
            else
#endif /* FLOAT_XFORM */
#endif /* GAMUTCHECK */
                eval(currIn, wOut, data);
#ifdef NO_UNPACK
#ifdef CACHED
            prevIn = currIn;
#endif
            currIn = (XFORM_TYPE *)(((char *)currIn) + INBYTES);
#else
#ifdef CACHED
            {XFORM_TYPE *tmp = currIn; currIn = prevIn; prevIn = tmp;} // SWAP
#endif /* CACHED */
#endif /* NO_UNPACK */
        }
#ifdef NO_PACK
        else
            COPY_MATCHED(prevOut,wOut);
        prevOut = wOut;
#endif
        PACK(p,wOut,output,Stride);
    } while (--n);
#ifdef CACHED
#ifdef NO_UNPACK
    memcpy(p->Cache.CacheOut,prevIn,INBYTES);
#else
    memcpy(prevIn, p->Cache.CacheIn, sizeof(XFORM_TYPE) * cmsMAXCHANNELS);
#endif
#ifdef NO_PACK
    COPY_MATCHED(prevOut,p->Cache.CacheOut);
#else
    memcpy(wOut, p->Cache.CacheOut, sizeof(XFORM_TYPE) * cmsMAXCHANNELS);
#endif /* NO_PACK */
#endif /* CACHED */
}

#undef wIn0
#undef wIn1
#undef XFORM_TYPE
#undef XFORM_FLOAT

#undef FUNCTION_NAME
#undef COMPARE
#undef INBYTES
#undef OUTBYTES
#undef UNPACK
#undef NO_UNPACK
#undef PACK
#undef NO_PACK
#undef UNPACKFN
#undef PACKFN
#undef GAMUTCHECK
#undef CACHED
#undef COPY_MATCHED
