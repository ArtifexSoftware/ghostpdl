/* Table 2 */
typedef struct {
  bool MMR;
  int32_t GBW;
  int32_t GBH;
  int GBTEMPLATE;
  bool TPGDON;
  bool USESKIP;
  /* SKIP */
  byte gbat[8];
} Jbig2GenericRegionParams;

/* 6.2 */
int
jbig2_decode_generic_region(Jbig2Ctx *ctx,
			    int32_t seg_number,
			    const Jbig2GenericRegionParams *params,
			    Jbig2ArithState *as,
			    byte *gbreg,
			    Jbig2ArithCx *GB_stats);

/* 7.4 */
int
jbig2_immediate_generic_region(Jbig2Ctx *ctx, Jbig2SegmentHeader *sh,
			       const uint8_t *segment_data);

