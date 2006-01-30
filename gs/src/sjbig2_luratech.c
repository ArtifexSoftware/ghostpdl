/* Copyright (C) 2005-2006 artofcode LLC.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id: sjbig2.c,v 1.7 2005/06/09 07:15:07 giles Exp $ */
/* jbig2decode filter implementation -- hooks in luratech JBIG2 */

#include "memory_.h"
#include "malloc_.h"  /* should use a gs mem pointer */
#include "gserrors.h"
#include "gserror.h"
#include "gdebug.h"
#include "strimpl.h"
#include "sjbig2_luratech.h"

#include <ldf_jb2.h>

/* JBIG2Decode stream implementation using the Luratech library */

/* if linking against a SDK build that requires a separate license key,
   you can change the following undefs to defines and set them here. */
/***
#ifndef JB2_LICENSE_NUM_1
# undef JB2_LICENSE_NUM_1 
#endif
#ifndef JB2_LICENSE_NUM_2
# undef JB2_LICENSE_NUM_2 
#endif
***/

/* The /JBIG2Decode filter is a fairly memory intensive one to begin with,
   Furthermore, as a PDF 1.4 feature, we can assume a fairly large 
   (host-level) machine. We therefore dispense with the normal 
   Ghostscript memory discipline and let the library allocate all its 
   resources on the heap. The pointers to these are not enumerated and 
   so will not be garbage collected. We rely on our release() proc being 
   called to deallocate state.
 */
 /* TODO: check allocations for integer overflow */

private_st_jbig2decode_state();	/* creates a gc object for our state, defined in sjbig2.h */

#define JBIG2DECODE_BUFFER_SIZE 4096

/* our implementation of the "parsed" /JBIG2Globals filter parameter */
typedef struct s_jbig2decode_global_data_s {
    unsigned char *data;
    unsigned long size;
} s_jbig2decode_global_data;

/* create a global data struct and copy data into it */
public int
s_jbig2decode_make_global_data(byte *data, uint size, void **result)
{
    s_jbig2decode_global_data *global = NULL;

    global = malloc(sizeof(*global));
    if (global == NULL) return gs_error_VMerror;

    global->data = malloc(size);
    if (global->data == NULL) {
	free(global);
	return gs_error_VMerror;
    }
    memcpy(global->data, data, size);
    global->size = size;

    *result = global;
    return 0;
}

/* free a global data struct and its data */
public void
s_jbig2decode_free_global_data(void *data)
{
    s_jbig2decode_global_data *global = (s_jbig2decode_global_data*)data;

    if (global->size && global->data) {
	free(global->data);
	global->size = 0;
    }
    free(global);
}

/* store a global ctx pointer in our state structure */
public int
s_jbig2decode_set_global_data(stream_state *ss, void *data)
{
    stream_jbig2decode_state *state = (stream_jbig2decode_state*)ss;
    s_jbig2decode_global_data *global = (s_jbig2decode_global_data*)data;

    if (state != NULL) {
      if (global != NULL) {
        state->global_data = global->data;
        state->global_size = global->size;
        return 0;
      } else {
	state->global_data = NULL;
	state->global_size = 0;
	return 0;
      }
    }
    
    return gs_error_VMerror;
}

/* invert the bits in a buffer */
/* jbig2 and postscript have different senses of what pixel
   value is black, so we must invert the image */
private void
s_jbig2decode_invert_buffer(unsigned char *buf, int length)
{
    int i;
    
    for (i = 0; i < length; i++)
        *buf++ ^= 0xFF;
}

/** callbacks passed to the luratech library */

/* memory allocator */
private void * JB2_Callback
s_jbig2_alloc(unsigned long size, void *userdata)
{
    void *result = malloc(size);
    return result;
}

/* memory release */
private JB2_Error JB2_Callback
s_jbig2_free(void *ptr, void *userdata)
{
    free(ptr);
    return cJB2_Error_OK;
}

/* error callback for jbig2 codec */
private void JB2_Callback
s_jbig2_message(const char *message, JB2_Message_Level level, void *userdata)
{
    const char *type;

    if (message == NULL) return;
    if (message[0] == '\0') return;

    switch (level) {
	case cJB2_Message_Information:
            type = "info"; break;;
	case cJB2_Message_Warning:
	    type = "WARNING"; break;;
	case cJB2_Message_Error:
	    type = "ERROR"; break;;
	default:
	    type = "unknown message"; break;;
    }

    if (level == cJB2_Message_Error) {
	dprintf2("Luratech JBIG2 %s %s\n", type, message);
    } else {
	if_debug2('s', "[s]Luratech JBIG2 %s %s\n", type, message);
    }

    return;
}

/* compressed read callback for jbig2 codec */
private JB2_Size_T JB2_Callback
s_jbig2_read(unsigned char *buffer, 
	JB2_Size_T offset, JB2_Size_T size, void *userdata)
{
    stream_jbig2decode_state *const state = (stream_jbig2decode_state *) userdata;
    long available;

    /* return data from the Globals stream */
    if (offset < state->global_size) {
	available = state->global_size - offset;
	if (available > size) available = size;
	memcpy(buffer, state->global_data + offset, available);
	return available;
    }

    /* else return data from the image stream */
    offset -= state->global_size;
    available = state->infill - offset;
    if (available > size) available = size;
    if (available <= 0) return 0;

    memcpy(buffer, state->inbuf + offset, available);
    return available;
}

/* uncompressed write callback for jbig2 codec */
private JB2_Error JB2_Callback
s_jbig2_write(unsigned char *buffer,
		unsigned long row, unsigned long width,
		unsigned long bbp, void *userdata)
{
    stream_jbig2decode_state *const state = (stream_jbig2decode_state *) userdata;
    unsigned char *line = state->image + row*state->stride;
    long available = ((width - 1) >> 3) + 1; 

    if (row >= state->height) {
	dlprintf2("jbig2decode: output for row index %lu of %lu called!\n", row, state->height);
	return cJB2_Error_Invalid_Index;
    }

    memcpy(line, buffer, available);
    s_jbig2decode_invert_buffer(line, available);

    return cJB2_Error_OK;
}


private int
s_jbig2decode_inbuf(stream_jbig2decode_state *state, stream_cursor_read * pr)
{
    long in_size = pr->limit - pr->ptr;

    /* allocate the input buffer if needed */
    if (state->inbuf == NULL) {
        state->inbuf = malloc(JBIG2DECODE_BUFFER_SIZE);
        if (state->inbuf == NULL) return gs_error_VMerror;
        state->insize = JBIG2DECODE_BUFFER_SIZE;
        state->infill = 0;
    }

    /* grow the input buffer if needed */
    if (state->insize < state->infill + in_size) {
        unsigned char *new;
        unsigned long new_size = state->insize;

        while (new_size < state->infill + in_size)
            new_size = new_size << 1;

        if_debug1('s', "[s]jbig2decode growing input buffer to %lu bytes\n",
                new_size);
        new = realloc(state->inbuf, new_size);
        if (new == NULL) return gs_error_VMerror;

        state->inbuf = new;
        state->insize = new_size;
    }

    /* copy the available input into our buffer */
    /* note that the gs stream library uses offset-by-one
        indexing of its buffers while we use zero indexing */
    memcpy(state->inbuf + state->infill, pr->ptr + 1, in_size);
    state->infill += in_size;
    pr->ptr += in_size;

    return 0;
}

/* initialize the steam.
   this involves allocating the context structures, and
   initializing the global context from the /JBIG2Globals object reference
 */
private int
s_jbig2decode_init(stream_state * ss)
{
    stream_jbig2decode_state *const state = (stream_jbig2decode_state *) ss;

    state->doc = NULL;
    state->inbuf = NULL;
    state->insize = 0;
    state->infill = 0;
    state->image = NULL;
    state->width = 0;
    state->height = 0;
    state->row = 0;
    state->stride = 0;
    state->error = 0;
    state->offset = 0;

    return 0; /* todo: check for allocation failure */
}

/* process a section of the input and return any decoded data.
   see strimpl.h for return codes.
 */
private int
s_jbig2decode_process(stream_state * ss, stream_cursor_read * pr,
		  stream_cursor_write * pw, bool last)
{
    stream_jbig2decode_state *const state = (stream_jbig2decode_state *) ss;
    long in_size = pr->limit - pr->ptr;
    long out_size = pw->limit - pw->ptr;
    long available;
    JB2_Error error;
    JB2_Scaling_Factor scale = {1,1};
    JB2_Rect rect = {0,0,0,0};
    ulong result;
    int status = last;
    
    if (in_size > 0) {
        /* buffer all available input for the decoder */
	result = s_jbig2decode_inbuf(state, pr);
	if (result) return ERRC;
    }

    if (last && out_size > 0) {

	if (state->doc == NULL) {

	    /* initialize the codec state and pass our callbacks */
	    error = JB2_Document_Start( &(state->doc),
		s_jbig2_alloc, ss,	/* alloc and its data */
		s_jbig2_free, ss, 	/* free and its data */
		s_jbig2_read, ss,	/* read callback and data */
		s_jbig2_message, ss);   /* message callback and data */
	    if (error != cJB2_Error_OK) return ERRC;

#if defined(JB2_LICENSE_NUM_1) && defined(JB2_LICENSE_NUM_2)
	    /* set the license keys if appropriate */
	    error = JB2_Document_Set_License(state->doc, 
		JB2_LICENSE_NUM_1, JB2_LICENSE_NUM_2);
	    if (error != cJB2_Error_OK) return ERRC;
#endif
	    /* decode relevent image parameters */
	    error = JB2_Document_Set_Page(state->doc, 0);
	    if (error != cJB2_Error_OK) return ERRC;
	    error = JB2_Document_Get_Property(state->doc, 
		cJB2_Prop_Page_Width, &result);
	    state->width = result;
	    error = JB2_Document_Get_Property(state->doc, 
		cJB2_Prop_Page_Height, &result);
	    if (error != cJB2_Error_OK) return ERRC;
	    state->height = result;
	    state->stride = ((state->width - 1) >> 3) + 1;
	    if_debug2('s', "[s]jbig2decode page is %ldx%ld; allocating image\n", state->width, state->height);
	    state->image = malloc(state->height*state->stride);

	    /* start image decode */
	    error = JB2_Document_Decompress_Page(state->doc, scale, rect,
		s_jbig2_write, ss);
	    if (error != cJB2_Error_OK) return ERRC;

	}

	/* copy any buffered image data out */
	available = state->stride*state->height - state->offset;
	if (available > 0) {
	    out_size = (out_size > available) ? available : out_size;
	    memcpy(pw->ptr+1, state->image + state->offset, out_size);
	    state->offset += out_size;
	    pw->ptr += out_size;
	}
	/* more data to output? */
	available = state->stride*state->height - state->offset;
	if (available > 0) return 1; 
	else return EOFC;
    }
    
    /* handle fatal decoding errors reported through our callback */
    if (state->error) return ERRC;
    
    return status;
}

/* stream release.
   free all our decoder state.
 */
private void
s_jbig2decode_release(stream_state *ss)
{
    stream_jbig2decode_state *const state = (stream_jbig2decode_state *) ss;

    if (state->doc) {
	JB2_Document_End(&(state->doc));
        if (state->inbuf) free(state->inbuf);
	if (state->image) free(state->image);
    }
    /* the interpreter calls jbig2decode_free_global_data() separately */
}

/* stream template */
const stream_template s_jbig2decode_template = {
    &st_jbig2decode_state, 
    s_jbig2decode_init,
    s_jbig2decode_process,
    1, 1, /* min in and out buffer sizes we can handle --should be ~32k,64k for efficiency? */
    s_jbig2decode_release
};
