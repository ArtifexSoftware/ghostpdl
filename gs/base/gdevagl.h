/* We need several tables, to hold the glyph names and a variable number of
 * Unicode code points.
 */

typedef struct single_glyph_list_s {
  const char *Glyph;
  unsigned short Unicode;
} single_glyph_list_t;

typedef struct double_glyph_list_s {
  const char *Glyph;
  unsigned short Unicode[2];
} double_glyph_list_t;

typedef struct treble_glyph_list_s {
  const char *Glyph;
  unsigned short Unicode[3];
} treble_glyph_list_t;

typedef struct quad_glyph_list_s {
  const char *Glyph;
  short Unicode[4];
} quad_glyph_list_t;
