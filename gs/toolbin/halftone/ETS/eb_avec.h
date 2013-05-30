typedef struct _eb_ctx_avec eb_ctx_avec;
typedef struct _eb_srcbuf eb_srcbuf;

struct _eb_ctx_avec {
    int xs;
    float *luts[4];
    vector float *iir_line;
    vector unsigned int *r_line;
    vector unsigned int *a_line;
    vector unsigned int *b_line;
    char *skip_line;
    vector float e;
    vector float e_i_1;
    vector unsigned int r;
    vector unsigned int a;
    vector unsigned int b;
    vector unsigned int aspect2;
    vector float ehi;
    vector float elo;
    vector float ohi;
    vector float r_mul;
    vector float kernel;
    vector float imscale1;
    vector float imscale2;
    vector float foff;
    vector float rbmul;
    vector float rsbase;
};

struct _eb_srcbuf {
    vector float im[16];
    vector float rb[16];
    vector float rs[16];
};

void eb_avec_core(eb_ctx_avec *ctx, vector unsigned char *destbuf[4],
	     eb_srcbuf *srcbuf, int offset, const signed char *tmbase);

void eb_avec_rev_rs(eb_ctx_avec *ctx, int offset);

int
eb_avec_prep_srcbuf(eb_ctx_avec *ctx, int n_planes,
		    eb_srcbuf *srcbuf, const ET_SrcPixel *const *src,
		    int off);
