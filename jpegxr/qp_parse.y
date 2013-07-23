
%{

/*************************************************************************
*
* This software module was originally contributed by Microsoft
* Corporation in the course of development of the
* ITU-T T.832 | ISO/IEC 29199-2 ("JPEG XR") format standard for
* reference purposes and its performance may not have been optimized.
*
* This software module is an implementation of one or more
* tools as specified by the JPEG XR standard.
*
* ITU/ISO/IEC give You a royalty-free, worldwide, non-exclusive
* copyright license to copy, distribute, and make derivative works
* of this software module or modifications thereof for use in
* products claiming conformance to the JPEG XR standard as
* specified by ITU-T T.832 | ISO/IEC 29199-2.
*
* ITU/ISO/IEC give users the same free license to this software
* module or modifications thereof for research purposes and further
* ITU/ISO/IEC standardization.
*
* Those intending to use this software module in products are advised
* that its use may infringe existing patents. ITU/ISO/IEC have no
* liability for use of this software module or modifications thereof.
*
* Copyright is not released for products that do not conform to
* to the JPEG XR standard as specified by ITU-T T.832 |
* ISO/IEC 29199-2.
*
* Microsoft Corporation retains full right to modify and use the code
* for its own purpose, to assign or donate the code to a third party,
* and to inhibit third parties from using the code for products that
* do not conform to the JPEG XR standard as specified by ITU-T T.832 |
* ISO/IEC 29199-2.
* 
* This copyright notice must be included in all copies or derivative
* works.
* 
* Copyright (c) ITU-T/ISO/IEC 2008.
**********************************************************************/

#ifdef _MSC_VER
#pragma comment (user,"$Id: qp_parse.y,v 1.8 2008/03/20 18:10:29 steve Exp $")
#else
#ident "$Id: qp_parse.y,v 1.8 2008/03/20 18:10:29 steve Exp $"
#endif

# include  "jpegxr.h"
# include  <stdlib.h>
# include  <assert.h>
# define YYINCLUDED_STDLIB_H

 extern int qp_lex(void);
 static void yyerror(const char*txt);

 static int errors = 0;

   /* Parse state variables. */

   /* The image we are getting QP values for */
 static jxr_image_t cur_image;
   /* Some characteristics of that image. */
 static unsigned tile_columns;
 static unsigned tile_rows;

   /* The array of all the tiles to be calculated. */
 static struct jxr_tile_qp* tile_qp = 0;

   /* Tile currently in progress. */
 static unsigned cur_tilex = 0;
 static unsigned cur_tiley = 0;
 static struct jxr_tile_qp*cur_tile = 0;

 static unsigned mb_in_tile = 0;

 static unsigned cur_channel = 0;

%}

%union {
      unsigned number;

      struct {
	    unsigned char*data;
	    unsigned count;
      } map_list;

      struct sixteen_nums {
	    unsigned char num;
	    unsigned char qp[16];
      } qp_set;
}


  /* Keywords */
%token K_DC K_CHANNEL K_HP K_INDEPENDENT K_LP K_SEPARATE K_TILE K_UNIFORM
  /* Other tokens */
%token <number> NUMBER

%type <qp_set>   lphp_qp_list
%type <map_list> map_list 

%%

start : tile_list ;

tile_list
  : tile_list tile
  | tile
  ;

  /* Describe the QP for a specific tile in the image, with the tile
     position given in tile-column/tile-row coordinates. */
tile
  : K_TILE '(' NUMBER ',' NUMBER ')'
      { cur_tilex = $3,
        cur_tiley = $5;
        assert(cur_tilex < tile_columns);
        assert(cur_tiley < tile_rows);
        cur_tile = tile_qp + cur_tiley*tile_columns + cur_tilex;
	mb_in_tile = jxr_get_TILE_WIDTH(cur_image, cur_tilex) * jxr_get_TILE_HEIGHT(cur_image, cur_tiley);
      }
    '{' tile_comp_mode tile_body '}'
      { /* Check the sanity of the calculated tile. */
	int idx;
	switch (cur_tile->component_mode) {
	    case JXR_CM_UNIFORM:
	      break;
	    case JXR_CM_SEPARATE:
	      if (cur_tile->channel[0].num_lp != cur_tile->channel[1].num_lp) {
		    fprintf(stderr, "channel-0 and channel-1 LP counts don't match\n");
		    errors += 1;
	      }
	      if (cur_tile->channel[0].num_hp != cur_tile->channel[1].num_hp) {
		    fprintf(stderr, "channel-0 and channel-1 HP counts don't match\n");
		    errors += 1;
	      }
	      if (jxr_get_IMAGE_CHANNELS(cur_image) == 1) {
		    fprintf(stderr, "Gray (1-channel) tiles must be channel mode UNIFORM\n");
		    errors += 1;
	      }
	      break;
	    case JXR_CM_INDEPENDENT:
	      for (idx = 1 ; idx < jxr_get_IMAGE_CHANNELS(cur_image) ; idx += 1) {
		    if (cur_tile->channel[0].num_lp != cur_tile->channel[idx].num_lp) {
			  fprintf(stderr, "channel-0 and channel-%d LP counts don't match\n", idx);
			  errors += 1;
		    }
		    if (cur_tile->channel[0].num_hp != cur_tile->channel[idx].num_hp) {
			  fprintf(stderr, "channel-0 and channel-%d HP counts don't match\n", idx);
			  errors += 1;
		    }
	      }
	      if (jxr_get_IMAGE_CHANNELS(cur_image) == 1) {
		    fprintf(stderr, "Gray (1-channel) tiles must be channel mode UNIFORM\n");
		    errors += 1;
	      }
	      break;
	    case JXR_CM_Reserved:
	      assert(0);
	      break;
	}
      }
  ;

tile_comp_mode
  : K_UNIFORM     { cur_tile->component_mode = JXR_CM_UNIFORM; }
  | K_SEPARATE    { cur_tile->component_mode = JXR_CM_SEPARATE; }
  | K_INDEPENDENT { cur_tile->component_mode = JXR_CM_INDEPENDENT; }
  ;

tile_body
  : tile_body tile_item
  | tile_item
  ;

tile_item
  : K_CHANNEL NUMBER
      { cur_channel = $2;
        assert(cur_channel < 16);
	cur_tile->channel[cur_channel].num_lp = 0;
	cur_tile->channel[cur_channel].num_hp = 0;
      }
    '{' channel_body '}'

  | K_LP '[' map_list ']'
      { cur_tile->lp_map = $3.data;
        if ($3.count != mb_in_tile) {
	      errors += 1;
	      fprintf(stderr, "parse qp: In tile %u,%u, expected %u lp blocks, got %u.\n",
		      cur_tilex, cur_tiley, mb_in_tile, $3.count);
        }
      }

  | K_HP '[' map_list ']'
      { cur_tile->hp_map = $3.data;
        if ($3.count != mb_in_tile) {
	      errors += 1;
	      fprintf(stderr, "parse qp: In tile %u,%u, expected %u lp blocks, got %u.\n",
		      cur_tilex, cur_tiley, mb_in_tile, $3.count);
        }
      }
  ;

channel_body
  : channel_body channel_item
  | channel_item
  ;

channel_item
  : K_DC '{' NUMBER '}'
      { cur_tile->channel[cur_channel].dc_qp = $3; }

  | K_LP '{' lphp_qp_list '}'
      { cur_tile->channel[cur_channel].num_lp = $3.num;
        int idx;
	for (idx = 0 ; idx < $3.num ; idx += 1)
	      cur_tile->channel[cur_channel].lp_qp[idx] = $3.qp[idx];
      }

  | K_HP '{' lphp_qp_list '}'
      { cur_tile->channel[cur_channel].num_hp = $3.num;
        int idx;
	for (idx = 0 ; idx < $3.num ; idx += 1)
	      cur_tile->channel[cur_channel].hp_qp[idx] = $3.qp[idx];
      }

  ;

lphp_qp_list
  : lphp_qp_list optional_comma NUMBER
      { unsigned cnt = $1.num;
        if (cnt >= 16) {
	      fprintf(stderr, "Too many (>16) QP values in QP list.\n");
	      errors += 1;
	} else {
	      assert(cnt < 16);
	      $1.qp[cnt] = $3;
	      $1.num += 1;
	}
	$$ = $1;
      }
  | NUMBER
      { $$.num = 1;
        $$.qp[0] = $1;
      }
  ;

optional_comma: ',' | ;

map_list
  : map_list NUMBER
      { if ($1.count >= mb_in_tile) {
	      fprintf(stderr, "Too many map items for tile!\n");
	      errors += 1;
        } else {
	      $1.data[$1.count++] = $2;
	}
        $$ = $1;
      }

  | NUMBER
      { assert(mb_in_tile >= 1);
        $$.data = (unsigned char*)calloc(mb_in_tile, 1);
        $$.count = 1;
        $$.data[0] = $1;
      }
  ;

%%

extern void qp_restart(FILE*fd);

int qp_parse_file(FILE*fd, jxr_image_t image)
{
      cur_image = image;
      tile_columns = jxr_get_TILE_COLUMNS(image);
      tile_rows = jxr_get_TILE_ROWS(image);

      tile_qp = (struct jxr_tile_qp *) calloc(tile_columns*tile_rows, sizeof(*tile_qp));
      assert(tile_qp);

#if defined(DETAILED_DEBUG)
      printf("qp_parse: tile_columns=%u, tile_rows=%u\n", tile_columns, tile_rows);
#endif

      qp_restart(fd);
      yyparse();

#if defined(DETAILED_DEBUG)
      printf("qp_parse: parse done with %u errors\n", errors);
#endif

      if (errors) return -1;

      jxr_set_QP_DISTRIBUTED(image, tile_qp);
      return 0;
}



static void yyerror(const char*txt)
{
      fprintf(stderr, "parsing QP file: %s\n", txt);
      errors += 1;
}
