/* This is an abstraction for dithering algorithms. */

typedef struct _RinkjDither RinkjDither;

struct _RinkjDither {
  void (*dither_line) (RinkjDither *self, unsigned char *dst, const unsigned char *src);
  void (*close) (RinkjDither *self);
};

void
rinkj_dither_line (RinkjDither *self, unsigned char *dst, const unsigned char *src);

void
rinkj_dither_close (RinkjDither *self);

