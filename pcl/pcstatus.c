/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcstatus.c */
/* PCL5 status readback commands */
#include "memory_.h"
#include "stdio_.h"
#include <stdarg.h>		/* how to make this portable? */
#include "string_.h"
#include "pcommand.h"
#include "pcfont.h"
#include "pcstate.h"
#include "stream.h"

#define STATUS_BUFFER_SIZE 10000

/* Internal routines */

/* Read out from the status buffer. */
/* Return the number of bytes read. */
uint
pcl_status_read(byte *data, uint max_data, pcl_state_t *pcls)
{	uint count = min(max_data,
			 pcls->status.write_pos - pcls->status.read_pos);
	if ( count )
	  memcpy(data, pcls->status.buffer + pcls->status.read_pos, count);
	pcls->status.read_pos += count;
	if ( pcls->status.read_pos == pcls->status.write_pos )
	  { gs_free_object(pcls->memory, pcls->status.buffer, "status buffer");
	    pcls->status.write_pos = pcls->status.read_pos = 0;
	  }
	return count;
}

/* Write a string on a stream. */
private void
stputs(stream *s, const char *str)
{	uint ignore_count;
	sputs(s, (const byte *)str, strlen(str), &ignore_count);
}

/* printf on a stream. */
/**** THIS SHOULD BE IN THE STREAM PACKAGE. ****/
void
stprintf(stream *s, const char *fmt, ...)
{	uint count;
	va_list args;
	char buf[1024];

	va_start(args, fmt);
	count = vsprintf(buf, fmt, args);
	sputs(s, (const byte *)buf, count, &count);
}

/* Set up a stream for writing into the status buffer. */
private void
status_begin(stream *s, pcl_state_t *pcls)
{	byte *buffer = pcls->status.buffer;

	if ( pcls->status.read_pos > 0 )
	  { memmove(buffer, buffer + pcls->status.read_pos,
		    pcls->status.write_pos - pcls->status.read_pos);
	    pcls->status.write_pos -= pcls->status.read_pos;
	    pcls->status.read_pos = 0;
	  }
	if ( buffer == 0 )
	  { buffer = gs_alloc_bytes(pcls->memory, STATUS_BUFFER_SIZE,
				    "status buffer");
	    pcls->status.buffer = buffer;
	  }
	if ( buffer == 0 )
	  swrite_string(s, pcls->status.internal_buffer,
			sizeof(pcls->status.internal_buffer));
	else
	  swrite_string(s, buffer, gs_object_size(pcls->memory, buffer));
	sseek(s, pcls->status.write_pos);
	stputs(s, "PCL\r\n");
}

/* Add an ID to a list being written. */
private void
status_put_id(stream *s, const char *id)
{	/* HACK: we know that there's at least one character in the buffer. */
	if ( *s->cursor.w.ptr == '\n' )
	  { /* We haven't started the list yet. */
	    stprintf(s, "IDLIST=\"%s", id);
	  }
	else
	  { stprintf(s, ",%s", id);
	  }
}

/* Finish writing an ID list. */
private void
status_end_id_list(stream *s)
{	/* HACK: we know that there's at least one character in the buffer. */
	if ( *s->cursor.w.ptr != '\n' )
	  stputs(s, "\"\r\n");
}

/* Output a number, at most two decimal places, but trimming trailing 0's
 * and possibly the ".".  Want to match HP's output as closely as we can. */
private void
status_put_floating(stream *s, double v)
{	/* Figure the format--easier than printing and chipping out the
	 * chars we need. */
	int vf = (int)(v * 100 + ((v < 0)? -0.5: 0.5));
	if ( vf / 100 * 100 == vf )
	    stprintf(s, "%d", vf / 100);
	else if ( vf / 10 * 10 == vf )
	    stprintf(s, "%.1f", v);
	else
	    stprintf(s, "%.2f", v);
}

/* Print font status information. */
/* font_set = -1 for font list, 0 or 1 for current font. */
private void
status_put_font(stream *s, const pcl_state_t *pcls,
  uint font_id, uint internal_id,
  pl_font_t *plfont, int font_set, bool extended)
{	char paren = (font_set > 0 ? ')' : '(');
	bool proportional = plfont->params.proportional_spacing;

	/* first escape sequence: symbol-set selection */
	stputs(s, "SELECT=\"");
	if ( pl_font_is_bound(plfont) || font_set > 0 )
	  { /* Bound or current font, put out the symbol set. */
	    uint symbol_set = font_set > 0?
	      pcls->font_selection[font_set].params.symbol_set:
	      plfont->params.symbol_set;
	    stprintf(s, "<Esc>%c%u%c", paren, symbol_set >> 5,
		     (symbol_set & 31) + 'A' - 1);
	  }

	/* second escape sequence: font selection */
	stprintf(s, "<Esc>%cs%dp", paren, proportional);
	if ( plfont->scaling_technology == plfst_bitmap )
	  { /* Bitmap font */
	    status_put_floating(s, plfont->params.pitch_100ths / 100.0);
	    stputs(s, "h");
	    status_put_floating(s, plfont->params.height_4ths / 4.0);
	    stputs(s, "v");
	  }
	else
	  { /* Scalable font: output depends on whether selected */
	    if ( font_set > 0 )
	      { /* If selected, we have to cheat and reach up for info;
		 * plfont is below where the scaled values exist. */
		if ( proportional )
		  { status_put_floating(s,
		      pcls->font_selection[font_set].params.height_4ths / 4.0);
		    stputs(s, "h");
		  }
		else
		  { status_put_floating(s,
		      pcls->font_selection[font_set].params.pitch_100ths /
			  100.0);
		    stputs(s, "v");
		  }
	      }
	    else
	      {
		stputs(s, proportional? "__v": "__h");
	      }
	  }
	stprintf(s, "%ds%db%uT", plfont->params.style,
	    plfont->params.stroke_weight, plfont->params.typeface_family);
	if ( plfont->storage & pcds_downloaded )
	  stprintf(s, "<Esc>%c%uX", paren, font_id);
	stputs(s, "\"\r\n");
	if ( !pl_font_is_bound(plfont) && font_set < 0 )
	  { /* Unbound font, not current, put out compatible symbol sets.
	     * Current fonts show the symbol set bound to them, above. */
	    /* XXX This needs the big ugly symset compatibility table. */
	  }
	if ( extended )
	  { /* Put out the "internal ID number". */
	    if ( plfont->storage & pcds_temporary )
	      stputs(s, "DEFID=NONE\r\n");
	    else
	      {
		stputs(s, "DEFID=\"");
		if ( plfont->storage & pcds_all_cartridges )
		  { int c;
		    int n = (plfont->storage & pcds_all_cartridges) >>
			pcds_cartridge_shift;
		    
		    /* pick out the bit index of the cartridge */
		    for (c = 0; (n & 1) == 0; c++)
		      n >>= 1;
		    stprintf(s, "C%d ", c);
		  }
		else if ( plfont->storage & pcds_all_simms )
		  { int m;
		    int n = (plfont->storage & pcds_all_simms) >>
			pcds_simm_shift;

		    /* pick out the bit index of the SIMM */
		    for (m = 0; (n & 1) == 0; m++)
		      n >>= 1;
		    stprintf(s, "M%d ", m);
		  }
		else
		    /* internal _vs_ permanent soft */
		    stputs(s, (plfont->storage & pcds_internal)? "I ": "S ");
		stprintf(s, "%d\"\r\n", internal_id);
	      }

	    /* XXX Put out the font name - we need a way to get the name
	     * for fonts that weren't downloaded, hence lack the known
	     * header field. */
	    if ( (plfont->storage & pcds_downloaded) &&
		plfont->header != NULL )
	      { /* Wire in the size of the FontName field (16)--it can't
	         * change anyway, and this saves work. */
		pcl_font_header_t *hdr = (pcl_font_header_t *)(plfont->header);

		stprintf(s, "NAME=\"%.16s\"\r\n", hdr->FontName);
	      }
	  }
}

/* Finish writing status. */
/* If we overflowed the buffer, store an error message. */
private void
status_end(stream *s, pcl_state_t *pcls)
{	if ( sendwp(s) )
	  { /* Overrun.  Scan back to the last EOL that leaves us */
	    /* enough room for the error line. */
	    static const char *error_line = "ERROR=INTERNAL ERROR\r\n";
	    int error_size = strlen(error_line) + 1;
	    uint limit = gs_object_size(pcls->memory, pcls->status.buffer);
	    uint wpos = stell(s);
	    
	    while ( limit - wpos < error_size ||
		    pcls->status.buffer[wpos - 1] != '\n'
		  )
	      --wpos;
	    s->end_status = 0;	/**** SHOULDN'T BE NECESSARY ****/
	    sseek(s, wpos);
	    stputs(s, error_line);
	  }
	sputc(s, FF);
	pcls->status.write_pos = stell(s);
}

/* Status readouts */
/* storage = 0 means currently selected, otherwise it is a mask. */

private int
status_do_fonts(stream *s, const pcl_state_t *pcls,
  pcl_data_storage_t storage, bool extended)
{	gs_const_string key;
	void *value;
	pl_dict_enum_t denum;

	pl_dict_enum_begin(&pcls->soft_fonts, &denum);
	while ( pl_dict_enum_next(&denum, &key, &value) )
	  { uint id = (key.data[0] << 8) + key.data[1];
	    if ( (((pl_font_t *)value)->storage & storage) != 0 ||
		 (storage == 0 && pcls->font == value)
	       )
	      status_put_font(s, pcls, id, 0, (pl_font_t *)value,
		  (storage != 0 ? -1 : pcls->font_selected),  extended);
	  }
	return 0;
}

private int
status_fonts(stream *s, const pcl_state_t *pcls,
  pcl_data_storage_t storage)
{	return status_do_fonts(s, pcls, storage, false);
}

private int
status_macros(stream *s, const pcl_state_t *pcls,
  pcl_data_storage_t storage)
{	gs_const_string key;
	void *value;
	pl_dict_enum_t denum;

	if ( storage == 0 )
	  return 0;		/* no "currently selected" macro */
	pl_dict_enum_begin(&pcls->macros, &denum);
	while ( pl_dict_enum_next(&denum, &key, &value) )
	  if ( ((pcl_entity_t *)value)->storage & storage )
	    { char id_string[6];
	      sprintf(id_string, "%u", (key.data[0] << 8) + key.data[1]);
	      status_put_id(s, id_string);
	    }
	status_end_id_list(s);
	return 0;
}

private int
status_patterns(stream *s, const pcl_state_t *pcls,
  pcl_data_storage_t storage)
{	gs_const_string key;
	void *value;
	pl_dict_enum_t denum;

	pl_dict_enum_begin(&pcls->patterns, &denum);
	while ( pl_dict_enum_next(&denum, &key, &value) )
	  { uint id = (key.data[0] << 8) + key.data[1];
	    if ( (((pcl_entity_t *)value)->storage & storage) != 0 ||
		 (storage == 0 &&
		  pcls->pattern_type == pcpt_user_defined &&
		  id_value(pcls->current_pattern_id) == id)
	       )
	      { char id_string[6];
	        sprintf(id_string, "%u", id);
		status_put_id(s, id_string);
	      }
	  }
	status_end_id_list(s);
	return 0;
}

private int
status_symbol_sets(stream *s, const pcl_state_t *pcls,
  pcl_data_storage_t storage)
{	gs_const_string key;
	void *value;
	pl_dict_enum_t denum;

	if ( storage == 0 )
	  return 0;		/* no "currently selected" symbol set */
	/**** DOESN'T FILTER PROPERLY, SEE p.16-22 ****/
	pl_dict_enum_begin(&pcls->symbol_sets, &denum);
	while ( pl_dict_enum_next(&denum, &key, &value) )
	  if ( ((pcl_entity_t *)value)->storage & storage )
	    { uint id = (key.data[0] << 8) + key.data[1];
	      char id_string[6];
	      sprintf(id_string, "%u%c", id >> 5, (id & 31) + 64);
	      status_put_id(s, id_string);
	    }
	status_end_id_list(s);
	return 0;
}

private int
status_fonts_extended(stream *s, const pcl_state_t *pcls,
  pcl_data_storage_t storage)
{	return status_do_fonts(s, pcls, storage, true);
}

private int (*status_write[])(P3(stream *s, const pcl_state_t *pcls,
				 pcl_data_storage_t storage)) = {
  status_fonts, status_macros, status_patterns, status_symbol_sets,
  status_fonts_extended
};

/* Commands */

private int /* ESC * s <enum> T */ 
pcl_set_readback_loc_type(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->location_type = uint_arg(pargs);
	return 0;
}

private int /* ESC * s <enum> U */ 
pcl_set_readback_loc_unit(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->location_unit = uint_arg(pargs);
	return 0;
}

private int /* ESC * s <enum> I */ 
pcl_inquire_readback_entity(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);
	int unit = pcls->location_unit;
	stream st;
	static const char *entity_types[] = {
	  "FONTS", "MACROS", "PATTERNS", "SYMBOLSETS", "FONTS EXTENDED"
	};
	pcl_data_storage_t storage;
	int code = 0;
	long pos;

	if ( i > 4 )
	  return e_Range;
	status_begin(&st, pcls);
	stprintf(&st, "INFO %s\r\n", entity_types[i]);
	switch ( pcls->location_type )
	  {
	  case 0:		/* invalid location */
	    code = -1;
	    break;
	  case 1:		/* currently selected */
	    storage = 0;	/* indicates currently selected */
	    break;
	  case 2:		/* all locations */
	    storage = ~0;
	    break;
	  case 3:		/* internal */
	    if ( unit != 0 )
	      { code = -1;
	        break;
	      }
	    storage = pcds_internal;
	    break;
	  case 4:		/* downloaded */
	    if ( unit > 2 )
	      code = -1;
	    else
	      { static const pcl_data_storage_t dl_masks[] =
		  { pcds_downloaded, pcds_temporary, pcds_permanent
		  };
		storage = dl_masks[unit];
	      }
	    break;
	  case 5:		/* cartridges */
	    if ( unit == 0 )
	      storage = pcds_all_cartridges;
	    else if ( unit <= pcds_cartridge_max )
	      storage = 1 << (pcds_cartridge_shift + unit - 1);
	    else
	      code = -1;
	    break;
	  case 6:		/* SIMMs */
	    if ( unit == 0 )
	      storage = pcds_all_simms;
	    else if ( unit <= pcds_simm_max )
	      storage = 1 << (pcds_simm_shift + unit - 1);
	    else
	      code = -1;
	    break;
	  default:
	    stputs(&st, "ERROR=INVALID ENTITY\r\n");
	    break;
	  }
	if ( code >= 0 )
	  { pos = stell(&st);
	    code = (*status_write[i])(&st, pcls, storage);
	    if ( code >= 0 )
	      { if ( stell(&st) == pos )
		  stputs(&st, "ERROR=NONE\r\n");
	        else if ( storage == 0 )	/* currently selected */
		  stprintf(&st, "LOCTYPE=%d\r\nLOCUNIT=%d\r\n",
			   pcls->location_type, unit);
	      }
	  }
	if ( code < 0 )
	  stputs(&st, "ERROR=INVALID LOCATION\r\n");
	status_end(&st, pcls);
	return 0;
}

private int /* ESC * s 1 M */ 
pcl_free_space(pcl_args_t *pargs, pcl_state_t *pcls)
{	stream st;

	status_begin(&st, pcls);
	stprintf(&st, "INFO MEMORY\r\n");
	if ( int_arg(pargs) != 1 )
	  stprintf(&st, "ERROR=INVALID UNIT\r\n");
	else
	  { gs_memory_status_t mstat;
	    gs_memory_status(pcls->memory, &mstat);
	    if ( pcls->memory != &gs_memory_default )
	      { gs_memory_status_t dstat;
	        gs_memory_status(&gs_memory_default, &dstat);
		mstat.allocated += dstat.allocated;
		mstat.used += dstat.used;
	      }
	    stprintf(&st, "TOTAL=%ld\r\n", mstat.allocated - mstat.used);
	    /* We don't currently have an API for determining */
	    /* the largest contiguous block. */
	    /**** RETURN SOMETHING RANDOM ****/
	    stprintf(&st, "LARGEST=%ld\r\n",
		     (mstat.allocated - mstat.used) >> 2);
	  }
	status_end(&st, pcls);
	return 0;
}

private int /* ESC & r <bool> F */ 
pcl_flush_all_pages(pcl_args_t *pargs, pcl_state_t *pcls)
{	switch ( uint_arg(pargs) )
	  {
	  case 0:
	    { /* Flush all complete pages. */
	      /* This is a driver function.... */
	      return 0;
	    }
	  case 1:
	    { /* Flush all pages, including an incomplete one. */
	      return pcl_end_page(pcls, false);
	    }
	  default:
	    return e_Range;
	  }
}

private int /* ESC * s <int_id> X */ 
pcl_echo(pcl_args_t *pargs, pcl_state_t *pcls)
{	stream st;

	status_begin(&st, pcls);
	stprintf(&st, "ECHO %d\r\n", int_arg(pargs));
	status_end(&st, pcls);
	return 0;
}

/* Initialization */
private int
pcstatus_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
	  {'s', 'T', {pcl_set_readback_loc_type, pca_neg_error|pca_big_error}},
	  {'s', 'U', {pcl_set_readback_loc_unit, pca_neg_error|pca_big_error}},
	  {'s', 'I', {pcl_inquire_readback_entity, pca_neg_error|pca_big_error}},
	  {'s', 'M', {pcl_free_space, pca_neg_ok|pca_big_ok}},
	END_CLASS
	DEFINE_CLASS_COMMAND_ARGS('&', 'r', 'F', pcl_flush_all_pages, pca_neg_error|pca_big_error)
	DEFINE_CLASS_COMMAND_ARGS('*', 's', 'X', pcl_echo, pca_neg_ok|pca_big_error)
	return 0;
}
private void
pcstatus_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { if ( type & pcl_reset_initial )
	      { pcls->status.buffer = 0;
	        pcls->status.write_pos = 0;
		pcls->status.read_pos = 0;
	      }
	    pcls->location_type = 0;
	    pcls->location_unit = 0;
	  }
}

const pcl_init_t pcstatus_init = {
  pcstatus_do_init, pcstatus_do_reset
};
