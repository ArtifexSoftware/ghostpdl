/* Copyright (C) 2001-2024 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Cos object support */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gsutil.h"		/* for bytes_compare */
#include "gdevpdfx.h"
#include "gdevpdfo.h"
#include "strimpl.h"
#include "sa85x.h"
#include "sarc4.h"
#include "sstring.h"

#define CHECK(expr)\
  BEGIN if ((code = (expr)) < 0) return code; END

/* ---------------- Structure definitions ---------------- */

/*
 * Define the generic structure for elements of arrays and
 * dictionaries/streams.
 */
#define cos_element_common(etype)\
    etype *next
struct cos_element_s {
    cos_element_common(cos_element_t);
};
#define private_st_cos_element()	/* in gdevpdfo.c */\
  gs_private_st_ptrs1(st_cos_element, cos_element_t, "cos_element_t",\
    cos_element_enum_ptrs, cos_element_reloc_ptrs, next)
#define cos_element_num_ptrs 1

/*
 * Define the structure for a piece of stream contents.
 */
struct cos_stream_piece_s {
    cos_element_common(cos_stream_piece_t);
    gs_offset_t position;		/* in streams file */
    uint size;
};
#define private_st_cos_stream_piece()	/* in gdevpdfo.c */\
  gs_private_st_suffix_add0_local(st_cos_stream_piece, cos_stream_piece_t,\
    "cos_stream_piece_t", cos_element_enum_ptrs, cos_element_reloc_ptrs,\
    st_cos_element)

/*
 * Define Cos arrays, dictionaries, and streams.
 */
     /* array */
struct cos_array_element_s {
    cos_element_common(cos_array_element_t);
    long index;
    cos_value_t value;
};
#define private_st_cos_array_element()	/* in gdevpdfo.c */\
  gs_private_st_composite(st_cos_array_element, cos_array_element_t,\
    "cos_array_element_t", cos_array_element_enum_ptrs, cos_array_element_reloc_ptrs)
    /* dict */
struct cos_dict_element_s {
    cos_element_common(cos_dict_element_t);
    gs_string key;
    bool owns_key;	/* if false, key is shared, do not trace or free */
    cos_value_t value;
};
#define private_st_cos_dict_element()	/* in gdevpdfo.c */\
  gs_private_st_composite(st_cos_dict_element, cos_dict_element_t,\
    "cos_dict_element_t", cos_dict_element_enum_ptrs, cos_dict_element_reloc_ptrs)

/* GC descriptors */
private_st_cos_element();
private_st_cos_stream_piece();
private_st_cos_object();
private_st_cos_value();
private_st_cos_array_element();
private_st_cos_dict_element();

/* GC procedures */
static
ENUM_PTRS_WITH(cos_value_enum_ptrs, cos_value_t *pcv) return 0;
 case 0:
    switch (pcv->value_type) {
    case COS_VALUE_SCALAR:
        return ENUM_STRING(&pcv->contents.chars);
    case COS_VALUE_CONST:
        break;
    case COS_VALUE_OBJECT:
    case COS_VALUE_RESOURCE:
        return ENUM_OBJ(pcv->contents.object);
    }
    return 0;
ENUM_PTRS_END
static
RELOC_PTRS_WITH(cos_value_reloc_ptrs, cos_value_t *pcv)
{
    switch (pcv->value_type) {
    case COS_VALUE_SCALAR:
        RELOC_STRING_VAR(pcv->contents.chars);
    case COS_VALUE_CONST:
        break;
    case COS_VALUE_OBJECT:
    case COS_VALUE_RESOURCE:
        RELOC_VAR(pcv->contents.object);
        break;
    }
}
RELOC_PTRS_END
static
ENUM_PTRS_WITH(cos_array_element_enum_ptrs, cos_array_element_t *pcae)
{
    return (index < cos_element_num_ptrs ?
            ENUM_USING_PREFIX(st_cos_element, 0) :
            ENUM_USING(st_cos_value, &pcae->value, sizeof(cos_value_t),
                       index - cos_element_num_ptrs));
}
ENUM_PTRS_END
static
RELOC_PTRS_WITH(cos_array_element_reloc_ptrs, cos_array_element_t *pcae)
{
    RELOC_PREFIX(st_cos_element);
    RELOC_USING(st_cos_value, &pcae->value, sizeof(cos_value_t));
}
RELOC_PTRS_END
static
ENUM_PTRS_WITH(cos_dict_element_enum_ptrs, cos_dict_element_t *pcde)
{
    return (index < cos_element_num_ptrs ?
            ENUM_USING_PREFIX(st_cos_element, 0) :
            (index -= cos_element_num_ptrs) > 0 ?
            ENUM_USING(st_cos_value, &pcde->value, sizeof(cos_value_t),
                       index - 1) :
            pcde->owns_key ? ENUM_STRING(&pcde->key) : ENUM_OBJ(NULL));
}
ENUM_PTRS_END
static
RELOC_PTRS_WITH(cos_dict_element_reloc_ptrs, cos_dict_element_t *pcde)
{
    RELOC_PREFIX(st_cos_element);
    if (pcde->owns_key)
        RELOC_STRING_VAR(pcde->key);
    RELOC_USING(st_cos_value, &pcde->value, sizeof(cos_value_t));
}
RELOC_PTRS_END

/* ---------------- Generic support ---------------- */

/* Initialize a just-allocated cos object. */
static void
cos_object_init(cos_object_t *pco, gx_device_pdf *pdev,
                const cos_object_procs_t *procs)
{
    if (pco) {
        pco->cos_procs = procs;
        pco->id = 0;
        pco->elements = 0;
        pco->pieces = 0;
        pco->mem = pdev->pdf_memory;
        pco->pres = 0;
        pco->is_open = true;
        pco->is_graphics = false;
        pco->written = false;
        pco->length = 0;
        pco->input_strm = 0;
        pco->md5_valid = 0;
        pco->stream_md5_valid = 0;
        memset(&pco->hash, 0x00, 16);
    }
}

/* Get the allocator for a Cos object. */
gs_memory_t *
cos_object_memory(const cos_object_t *pco)
{
    return pco->mem;
}

/* Change a generic cos object into one of a specific type. */
int
cos_become(cos_object_t *pco, cos_type_t cotype)
{
    if (cos_type(pco) != cos_type_generic)
        return_error(gs_error_typecheck);
    cos_type(pco) = cotype;
    return 0;
}

/* Release a cos object. */
cos_proc_release(cos_release);	/* check prototype */
void
cos_release(cos_object_t *pco, client_name_t cname)
{
    pco->cos_procs->release(pco, cname);
}

/* Free a cos object. */
void
cos_free(cos_object_t *pco, client_name_t cname)
{
    cos_release(pco, cname);
    gs_free_object(cos_object_memory(pco), pco, cname);
}

/* Write a cos object on the output. */
cos_proc_write(cos_write);	/* check prototype */
int
cos_write(const cos_object_t *pco, gx_device_pdf *pdev, gs_id object_id)
{
    return pco->cos_procs->write(pco, pdev, object_id);
}

/* Write a cos object as a PDF object. */
int
cos_stream_write(const cos_object_t *pco, gx_device_pdf *pdev, gs_id object_id);

int
cos_write_object(cos_object_t *pco, gx_device_pdf *pdev, pdf_resource_type_t type)
{
    int code;

    if (pco->id == 0 || pco->written)
        return_error(gs_error_Fatal);
    if (pco->cos_procs->write == cos_stream_write)
        pdf_open_separate_noObjStm(pdev, pco->id, type);
    else
        pdf_open_separate(pdev, pco->id, type);
    code = cos_write(pco, pdev, pco->id);
    if (pco->cos_procs->write == cos_stream_write)
        pdf_end_separate_noObjStm(pdev, type);
    else
        pdf_end_separate(pdev, type);
    pco->written = true;
    return code;
}

/* Make a value to store into a composite object. */
const cos_value_t *
cos_string_value(cos_value_t *pcv, const byte *data, uint size)
{
    /*
     * It's OK to break const here, because the value will be copied
     * before being stored in the collection.
     */
    pcv->contents.chars.data = (byte *)data;
    pcv->contents.chars.size = size;
    pcv->value_type = COS_VALUE_SCALAR;
    return pcv;
}
const cos_value_t *
cos_c_string_value(cos_value_t *pcv, const char *str)
{
    /*
     * We shouldn't break const here, because the value will not be copied
     * or freed (or traced), but that would require a lot of bothersome
     * casting elsewhere.
     */
    pcv->contents.chars.data = (byte *)str;
    pcv->contents.chars.size = strlen(str);
    pcv->value_type = COS_VALUE_CONST;
    return pcv;
}
const cos_value_t *
cos_object_value(cos_value_t *pcv, cos_object_t *pco)
{
    pcv->contents.object = pco;
    pcv->value_type = COS_VALUE_OBJECT;
    return pcv;
}
const cos_value_t *
cos_resource_value(cos_value_t *pcv, cos_object_t *pco)
{
    pcv->contents.object = pco;
    pcv->value_type = COS_VALUE_RESOURCE;
    return pcv;
}

/* Free a value. */
void
cos_value_free(const cos_value_t *pcv, gs_memory_t *mem,
               client_name_t cname)
{
    switch (pcv->value_type) {
    case COS_VALUE_SCALAR:
        gs_free_string(mem, pcv->contents.chars.data,
                       pcv->contents.chars.size, cname);
    case COS_VALUE_CONST:
        break;
    case COS_VALUE_OBJECT:
        /* Free the object if this is the only reference to it. */
        if (pcv->contents.object != NULL) /* see cos_dict_objects_delete. */
            if (!pcv->contents.object->id)
                cos_free(pcv->contents.object, cname);
    case COS_VALUE_RESOURCE:
        break;
    }
}

/* Write a value on the output. */
static int
cos_value_write_spaced(const cos_value_t *pcv, gx_device_pdf *pdev,
                       bool do_space, gs_id object_id)
{
    stream *s = pdev->strm;

    switch (pcv->value_type) {
    case COS_VALUE_SCALAR:
    case COS_VALUE_CONST:
        if (do_space)
            switch (pcv->contents.chars.data[0]) {
            case '/': case '(': case '<': break;
            default: stream_putc(s, ' ');
            }
        return pdf_write_value(pdev, pcv->contents.chars.data,
                        pcv->contents.chars.size, object_id);
    case COS_VALUE_RESOURCE:
        pprintld1(s, "/R%ld", pcv->contents.object->id);
        break;
    case COS_VALUE_OBJECT: {
        cos_object_t *pco = pcv->contents.object;

        if (!pco->id) {
            if (do_space &&
                !(pco->cos_procs == cos_type_array ||
                  pco->cos_procs == cos_type_dict)
                ) {
                /* Arrays and dictionaries (only) are self-delimiting. */
                stream_putc(s, ' ');
            }
            return cos_write(pco, pdev, object_id);
        }
        if (do_space)
            stream_putc(s, ' ');
        pprintld1(s, "%ld 0 R", pco->id);
        if (pco->cos_procs == cos_type_reference)
            pco->id = 0;
        break;
    }
    default:			/* can't happen */
        return_error(gs_error_Fatal);
    }
    return 0;
}
int
cos_value_write(const cos_value_t *pcv, gx_device_pdf *pdev)
{
    return cos_value_write_spaced(pcv, pdev, false, 0);
}

/* Copy a value if necessary for putting into an array or dictionary. */
static int
cos_copy_element_value(cos_value_t *pcv, gs_memory_t *mem,
                       const cos_value_t *pvalue, bool copy)
{
    *pcv = *pvalue;
    if (pvalue->value_type == COS_VALUE_SCALAR && copy) {
        byte *value_data = gs_alloc_string(mem, pvalue->contents.chars.size,
                                           "cos_copy_element_value");

        if (value_data == 0)
            return_error(gs_error_VMerror);
        memcpy(value_data, pvalue->contents.chars.data,
               pvalue->contents.chars.size);
        pcv->contents.chars.data = value_data;
    }
    return 0;
}

/* Release a value copied for putting, if the operation fails. */
static void
cos_uncopy_element_value(cos_value_t *pcv, gs_memory_t *mem, bool copy)
{
    if (pcv->value_type == COS_VALUE_SCALAR && copy)
        gs_free_string(mem, pcv->contents.chars.data, pcv->contents.chars.size,
                       "cos_uncopy_element_value");
}

static int cos_value_hash(cos_value_t *pcv0, gs_md5_state_t *md5, gs_md5_byte_t *hash, gx_device_pdf *pdev)
{
    int code;
    switch (pcv0->value_type) {
        case COS_VALUE_SCALAR:
        case COS_VALUE_CONST:
            gs_md5_append(md5, pcv0->contents.chars.data, pcv0->contents.chars.size);
            break;

        case COS_VALUE_OBJECT:
            code = pcv0->contents.object->cos_procs->hash(pcv0->contents.object, md5, hash, pdev);
            if (code < 0)
                return code;
            break;
        case COS_VALUE_RESOURCE:
            break;
    }
    return 0;
}

/* ---------------- Specific object types ---------------- */

/* ------ Generic objects ------ */

static cos_proc_release(cos_reference_release);
static cos_proc_release(cos_generic_release);
static cos_proc_write(cos_generic_write);
static cos_proc_equal(cos_generic_equal);
static cos_proc_hash(cos_generic_hash);
const cos_object_procs_t cos_generic_procs = {
    cos_generic_release, cos_generic_write, cos_generic_equal, cos_generic_hash
};
const cos_object_procs_t cos_reference_procs = {
    cos_reference_release, cos_generic_write, cos_generic_equal, cos_generic_hash
};

cos_object_t *
cos_object_alloc(gx_device_pdf *pdev, client_name_t cname)
{
    gs_memory_t *mem = pdev->pdf_memory;
    cos_object_t *pco =
        gs_alloc_struct(mem, cos_object_t, &st_cos_object, cname);

    cos_object_init(pco, pdev, &cos_generic_procs);
    return pco;
}

cos_object_t *
cos_reference_alloc(gx_device_pdf *pdev, client_name_t cname)
{
    gs_memory_t *mem = pdev->pdf_memory;
    cos_object_t *pco =
        gs_alloc_struct(mem, cos_object_t, &st_cos_object, cname);

    cos_object_init(pco, pdev, &cos_reference_procs);
    return pco;
}

static void
cos_reference_release(cos_object_t *pco, client_name_t cname)
{
    /* Do nothing. */
}

static void
cos_generic_release(cos_object_t *pco, client_name_t cname)
{
    /* Do nothing. */
}

static int
cos_generic_write(const cos_object_t *pco, gx_device_pdf *pdev, gs_id object_id)
{
    return_error(gs_error_Fatal);
}

static int
cos_generic_equal(const cos_object_t *pco0, const cos_object_t *pco1, gx_device_pdf *pdev)
{
    return_error(gs_error_Fatal);
}

static int
cos_generic_hash(const cos_object_t *pco0, gs_md5_state_t *md5, gs_md5_byte_t *hash, gx_device_pdf *pdev)
{
    return_error(gs_error_Fatal);
}

/* ------ Arrays ------ */

static cos_proc_release(cos_array_release);
static cos_proc_write(cos_array_write);
static cos_proc_equal(cos_array_equal);
static cos_proc_hash(cos_array_hash);
const cos_object_procs_t cos_array_procs = {
    cos_array_release, cos_array_write, cos_array_equal, cos_array_hash
};

cos_array_t *
cos_array_alloc(gx_device_pdf *pdev, client_name_t cname)
{
    gs_memory_t *mem = pdev->pdf_memory;
    cos_array_t *pca =
        gs_alloc_struct(mem, cos_array_t, &st_cos_object, cname);

    cos_object_init((cos_object_t *)pca, pdev, &cos_array_procs);
    return pca;
}

cos_array_t *
cos_array_from_floats(gx_device_pdf *pdev, const float *pf, uint size,
                      client_name_t cname)
{
    cos_array_t *pca = cos_array_alloc(pdev, cname);
    uint i;

    if (pca == 0)
        return 0;
    for (i = 0; i < size; ++i) {
        int code = cos_array_add_real(pca, pf[i]);

        if (code < 0) {
            COS_FREE(pca, cname);
            return 0;
        }
    }
    return pca;
}

static void
cos_array_release(cos_object_t *pco, client_name_t cname)
{
    cos_array_t *const pca = (cos_array_t *)pco;
    cos_array_element_t *cur;
    cos_array_element_t *next;

    for (cur = pca->elements; cur; cur = next) {
        next = cur->next;
        cos_value_free(&cur->value, cos_object_memory(pco), cname);
        gs_free_object(cos_object_memory(pco), cur, cname);
    }
    pca->elements = 0;
}

static cos_array_element_t *cos_array_reorder(const cos_array_t *pca,
                                               cos_array_element_t *first);
static int
cos_array_write(const cos_object_t *pco, gx_device_pdf *pdev, gs_id object_id)
{
    stream *s = pdev->strm;
    const cos_array_t *const pca = (const cos_array_t *)pco;
    cos_array_element_t *first = cos_array_reorder(pca, NULL);
    cos_array_element_t *pcae;
    uint last_index = 0, Element_Count = 0;

    stream_puts(s, "[");
    for (pcae = first; pcae; ++last_index, pcae = pcae->next) {
        Element_Count++;

        if(pdev->PDFA != 0 && Element_Count > 8191) {
            switch (pdev->PDFACompatibilityPolicy) {
            case 0:
                emprintf(pdev->memory,
                    "Too many entries in array,\n max 8191 in PDF/A, reverting to normal PDF output\n");
                pdev->AbortPDFAX = true;
                pdev->PDFA = 0;
                break;
            case 1:
                emprintf(pdev->memory,
                     "Too many entries in array,\n max 8191 in PDF/A. Cannot simply elide dictionary, reverting to normal output\n");
                pdev->AbortPDFAX = true;
                pdev->PDFA = 0;
                break;
            case 2:
                emprintf(pdev->memory,
                     "Too many entries in array,\n max 8191 in PDF/A. aborting conversion\n");
                /* Careful here, only certain errors will bubble up
                 * through the text processing.
                 */
                return_error(gs_error_limitcheck);
                break;
            default:
                emprintf(pdev->memory,
                     "Too many entries in array,\n max 8191 in PDF/A. Unrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                pdev->AbortPDFAX = true;
                pdev->PDFA = 0;
                break;
            }
        }
        if (pcae != first)
            stream_putc(s, '\n');
        for (; pcae->index > last_index; ++last_index)
            stream_puts(s, "null\n");
        cos_value_write_spaced(&pcae->value, pdev, false, object_id);
    }
    DISCARD(cos_array_reorder(pca, first));
    stream_puts(s, "]");
    if (pdev->PDFA != 0)
        stream_puts(s, "\n");
    return 0;
}

static int
cos_array_equal(const cos_object_t *pco0, const cos_object_t *pco1, gx_device_pdf *pdev)
{
    const cos_array_t *const pca0 = (const cos_array_t *)pco0;
    const cos_array_t *const pca1 = (const cos_array_t *)pco1;
    int code;

    if (!pca0->md5_valid) {
        gs_md5_init((gs_md5_state_t *)&pca0->md5);
        code = cos_array_hash(pco0, (gs_md5_state_t *)&pca0->md5, (gs_md5_byte_t *)pca0->hash, pdev);
        if (code < 0)
            return code;
        gs_md5_finish((gs_md5_state_t *)&pca0->md5, (gs_md5_byte_t *)pca0->hash);
        ((cos_object_t *)pca0)->md5_valid = true;
    }
    if (!pca1->md5_valid) {
        gs_md5_init((gs_md5_state_t *)&pca1->md5);
        code = cos_array_hash(pco1, (gs_md5_state_t *)&pca1->md5, (gs_md5_byte_t *)pca1->hash, pdev);
        if (code < 0)
            return code;
        gs_md5_finish((gs_md5_state_t *)&pca1->md5, (gs_md5_byte_t *)pca1->hash);
        ((cos_object_t *)pca1)->md5_valid = true;
    }
    if (memcmp(&pca0->hash, &pca1->hash, 16) != 0)
        return false;

    return true;
}

static int
cos_array_hash(const cos_object_t *pco0, gs_md5_state_t *md5, gs_md5_byte_t *hash, gx_device_pdf *pdev)
{
    const cos_array_t *const pca0 = (const cos_array_t *)pco0;
    cos_array_element_t *first0 = pca0->elements;
    cos_array_element_t *pcae0;
    int code;

    for (pcae0 = first0; pcae0;pcae0 = pcae0->next) {
        code = cos_value_hash(&pcae0->value, md5, hash, pdev);
        if (code < 0)
            return code;
    }
    return 0;
}

/* Put/add an element in/to an array. */
int
cos_array_put(cos_array_t *pca, int64_t index, const cos_value_t *pvalue)
{
    gs_memory_t *mem = COS_OBJECT_MEMORY(pca);
    cos_value_t value;
    int code = cos_copy_element_value(&value, mem, pvalue, true);

    if (code >= 0) {
        code = cos_array_put_no_copy(pca, index, &value);
        if (code < 0)
            cos_uncopy_element_value(&value, mem, true);
    }
    pca->md5_valid = false;
    return code;
}
int
cos_array_put_no_copy(cos_array_t *pca, int64_t index, const cos_value_t *pvalue)
{
    gs_memory_t *mem = COS_OBJECT_MEMORY(pca);
    cos_array_element_t **ppcae = &pca->elements;
    cos_array_element_t *pcae;
    cos_array_element_t *next;

    while ((next = *ppcae) != 0 && next->index > index)
        ppcae = &next->next;
    if (next && next->index == index) {
        /* We're replacing an existing element. */
        cos_value_free(&next->value, mem,
                       "cos_array_put(old value)");
        pcae = next;
    } else {
        /* Create a new element. */
        pcae = gs_alloc_struct(mem, cos_array_element_t, &st_cos_array_element,
                               "cos_array_put(element)");
        if (pcae == 0)
            return_error(gs_error_VMerror);
        pcae->index = index;
        pcae->next = next;
        *ppcae = pcae;
    }
    pcae->value = *pvalue;
    pca->md5_valid = false;
    return 0;
}
static long
cos_array_next_index(const cos_array_t *pca)
{
    return (pca->elements ? pca->elements->index + 1 : 0L);
}
int
cos_array_add(cos_array_t *pca, const cos_value_t *pvalue)
{
    pca->md5_valid = false;
    return cos_array_put(pca, cos_array_next_index(pca), pvalue);
}
int
cos_array_add_no_copy(cos_array_t *pca, const cos_value_t *pvalue)
{
    pca->md5_valid = false;
    return cos_array_put_no_copy(pca, cos_array_next_index(pca), pvalue);
}
int
cos_array_add_c_string(cos_array_t *pca, const char *str)
{
    cos_value_t value;

    return cos_array_add(pca, cos_c_string_value(&value, str));
}
int
cos_array_add_int(cos_array_t *pca, int i)
{
    char str[sizeof(int) * 8 / 3 + 3]; /* sign, rounding, 0 terminator */
    cos_value_t v;

    gs_snprintf(str, sizeof(str), "%d", i);
    return cos_array_add(pca, cos_string_value(&v, (byte *)str, strlen(str)));
}
int
cos_array_add_real(cos_array_t *pca, double r)
{
    byte str[50];		/****** ADHOC ******/
    stream s;
    cos_value_t v;

    s_init(&s, NULL);
    swrite_string(&s, str, sizeof(str));
    pprintg1(&s, "%g", r);
    return cos_array_add(pca, cos_string_value(&v, str, stell(&s)));
}
int
cos_array_add_object(cos_array_t *pca, cos_object_t *pco)
{
    cos_value_t value;

    value.contents.chars.size = 0; /* Quiet a warning appeared with MSVC6 inline optimization. */
    return cos_array_add(pca, cos_object_value(&value, pco));
}

/*
 * Remove and return the last element of an array.  Since this is intended
 * specifically for arrays used as stacks, it gives an error if there is a
 * gap in indices between the last element and the element before it.
 */
int
cos_array_unadd(cos_array_t *pca, cos_value_t *pvalue)
{
    cos_array_element_t *pcae = pca->elements;

    if (pcae == 0 ||
        pcae->index != (pcae->next == 0 ? 0 : pcae->next->index + 1)
        )
        return_error(gs_error_rangecheck);
    *pvalue = pcae->value;
    pca->elements = pcae->next;
    gs_free_object(COS_OBJECT_MEMORY(pca), pcae, "cos_array_unadd");
    pca->md5_valid = false;
    return 0;
}

/* Get the first / next element for enumerating an array. */
const cos_array_element_t *
cos_array_element_first(const cos_array_t *pca)
{
    return pca->elements;
}
const cos_array_element_t *
cos_array_element_next(const cos_array_element_t *pca, int64_t *pindex,
                       const cos_value_t **ppvalue)
{
    *pindex = pca->index;
    *ppvalue = &pca->value;
    return pca->next;
}

/*
 * Reorder the elements of an array for writing or after writing.  Usage:
 *	first_element = cos_array_reorder(pca, NULL);
 *	...
 *	cos_array_reorder(pca, first_element);
 */
static cos_array_element_t *
cos_array_reorder(const cos_array_t *pca, cos_array_element_t *first)
{
    cos_array_element_t *last;
    cos_array_element_t *next;
    cos_array_element_t *pcae;

    for (pcae = (first ? first : pca->elements), last = NULL; pcae;
         pcae = next)
        next = pcae->next, pcae->next = last, last = pcae;
    return last;
}

/* ------ Dictionaries ------ */

static cos_proc_release(cos_dict_release);
static cos_proc_write(cos_dict_write);
static cos_proc_equal(cos_dict_equal);
static cos_proc_hash(cos_dict_hash);
const cos_object_procs_t cos_dict_procs = {
    cos_dict_release, cos_dict_write, cos_dict_equal, cos_dict_hash
};

cos_dict_t *
cos_dict_alloc(gx_device_pdf *pdev, client_name_t cname)
{
    gs_memory_t *mem = pdev->pdf_memory;
    cos_dict_t *pcd =
        gs_alloc_struct(mem, cos_dict_t, &st_cos_object, cname);

    cos_object_init((cos_object_t *)pcd, pdev, &cos_dict_procs);
    return pcd;
}

static void
cos_dict_element_free(cos_dict_t *pcd, cos_dict_element_t *pcde,
                      client_name_t cname)
{
    gs_memory_t *mem = COS_OBJECT_MEMORY(pcd);

    cos_value_free(&pcde->value, mem, cname);
    if (pcde->owns_key)
        gs_free_string(mem, pcde->key.data, pcde->key.size, cname);
    gs_free_object(mem, pcde, cname);
}

static int
cos_dict_delete(cos_dict_t *pcd, const byte *key_data, uint key_size)
{
    cos_dict_element_t *pcde = pcd->elements, *prev = 0;

    for (; pcde; pcde = pcde->next) {
        if (!bytes_compare(key_data, key_size, pcde->key.data, pcde->key.size)) {
            if (prev != 0)
                prev->next = pcde->next;
            else
                pcd->elements = pcde->next;
            cos_dict_element_free(pcd, pcde, "cos_dict_delete");
            return 0;
        }
        prev = pcde;
    }
    return -1;
}
int
cos_dict_delete_c_key(cos_dict_t *pcd, const char *key)
{
    return cos_dict_delete(pcd, (const byte *)key, strlen(key));
}

static void
cos_dict_release(cos_object_t *pco, client_name_t cname)
{
    cos_dict_t *const pcd = (cos_dict_t *)pco;
    cos_dict_element_t *cur;
    cos_dict_element_t *next;

    for (cur = pcd->elements; cur; cur = next) {
        next = cur->next;
        cos_dict_element_free(pcd, cur, cname);
    }
    pcd->elements = 0;
}

/* Write the elements of a dictionary. */
static int
cos_elements_write(stream *s, const cos_dict_element_t *pcde,
                   gx_device_pdf *pdev, bool do_space, gs_id object_id)
{
    int Element_Count = 0;

    if (pcde) {
        /* Temporarily replace the output stream in pdev. */
        stream *save = pdev->strm;

        pdev->strm = s;
        for (;;) {
            gs_id object_id1 = (pdev->NoEncrypt.size == 0 ||
                                bytes_compare(pdev->NoEncrypt.data, pdev->NoEncrypt.size,
                                    pcde->key.data, pcde->key.size)
                                ? object_id : (gs_id)-1);

            Element_Count++;

            if(pdev->PDFA != 0 && Element_Count > 4095) {
                switch (pdev->PDFACompatibilityPolicy) {
                    case 0:
                        emprintf(pdev->memory,
                             "Too many entries in dictionary,\n max 4095 in PDF/A, reverting to normal PDF output\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFA = 0;
                        break;
                    case 1:
                        emprintf(pdev->memory,
                             "Too many entries in dictionary,\n max 4095 in PDF/A. Cannot simply elide dictionary, reverting to normal output\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFA = 0;
                        break;
                    case 2:
                        emprintf(pdev->memory,
                             "Too many entries in dictionary,\n max 4095 in PDF/A. aborting conversion\n");
                        /* Careful here, only certain errors will bubble up
                         * through the text processing.
                         */
                        return_error(gs_error_limitcheck);
                        break;
                    default:
                        emprintf(pdev->memory,
                             "Too many entries in dictionary,\n max 4095 in PDF/A. Unrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFA = 0;
                        break;
                }
            }

            pdf_write_value(pdev, pcde->key.data, pcde->key.size, object_id1);
            cos_value_write_spaced(&pcde->value, pdev, true, object_id1);
            pcde = pcde->next;
            if (pcde || do_space)
                stream_putc(s, '\n');
            if (!pcde)
                break;
        }
        pdev->strm = save;
    }
    return 0;
}
int
cos_dict_elements_write(const cos_dict_t *pcd, gx_device_pdf *pdev)
{
    return cos_elements_write(pdev->strm, pcd->elements, pdev, true, pcd->id);
}

static int
cos_dict_write(const cos_object_t *pco, gx_device_pdf *pdev, gs_id object_id)
{
    stream *s = pdev->strm;

    stream_puts(s, "<<");
    cos_elements_write(s, ((const cos_dict_t *)pco)->elements, pdev, false, object_id);
    stream_puts(s, ">>");
    if (pdev->PDFA != 0)
        stream_puts(s, "\n");
    return 0;
}

static int find_first_dict_entry(const cos_dict_t *d, const cos_dict_element_t **element)
{
    const cos_dict_element_t *pcde = d->elements, *First;
    int code, length, length1, length2, offset1 = 0, offset2 = 0, i;

    *element = 0L;

    First = pcde;

    /*
     * If the name has any 'unusual' characters, it is 'escaped' by starting with NULLs
     * I suspect we no longer need that, but here we remove the escaping NULLs
     */
    for (i = 0;First->key.data[i] == 0x00; i++)
        ;

    length1 = First->key.size - i;
    offset1 = i;

    if (First->key.data[offset1] == '/') {
        length1 -= 1;
        offset1 += 1;
    } else {
        if (First->key.data[offset1] == '(') {
            length1 = First->key.size - 2;
            offset1 = 1;
        } else {
            return_error(gs_error_typecheck);
        }
    }

    pcde = pcde->next;
    while (pcde){
        for (i = 0;pcde->key.data[i] == 0x00; i++)
            ;
        length2 = pcde->key.size - i;
        offset2 = i;

        if (pcde->key.data[offset2] == '/') {
            length2 -= 1;
            offset2 += 1;
        } else {
            if (pcde->key.data[offset2] == '(') {
                length2 = pcde->key.size - 2;
                offset2 = 1;
            } else {
                return_error(gs_error_typecheck);
            }
        }

        if (length2 < length1)
            length = length2;
        else
            length = length1;
        code = strncmp((const char *)&pcde->key.data[offset2], (const char *)&First->key.data[offset1], length);
        if (code == 0) {
            if (length2 < length1) {
                First = pcde;
                length1 = length2;
                offset1 = offset2;
            }
        } else if (code < 0) {
            First = pcde;
            length1 = length2;
            offset1 = offset2;
        }
        pcde = pcde->next;
    }
    *element = First;
    return 0;
}

static int find_next_dict_entry(const cos_dict_t *d, const cos_dict_element_t **element)
{
    const cos_dict_element_t *pcde = d->elements, *Current = *element, *Next = 0L;
    int code, length, length1, length2, length3, offset1 = 0, offset2 = 0, offset3 = 0, i;

    /*
     * If the name has any 'unusual' characters, it is 'escaped' by starting with NULLs
     * I suspect we no longer need that, but here we remove the escaping NULLs
     */
    for (i = 0;Current->key.data[i] == 0x00; i++)
        ;
    length1 = Current->key.size - i;
    offset1 = i;
    if (Current->key.data[offset1] == '/') {
        length1 -= 1;
        offset1 += 1;
    } else {
        if (Current->key.data[0] == '(') {
            length1 -= 2;
            offset1 = 1;
        } else {
            return_error(gs_error_typecheck);
        }
    }

    while (pcde) {
        for (i = 0;pcde->key.data[i] == 0x00; i++)
            ;
        length2 = pcde->key.size - i;
        offset2 = i;
        if (pcde->key.data[offset2] == '/') {
            length2 -= 1;
            offset2 += 1;
        } else {
            if (pcde->key.data[0] == '(') {
                length2 -= 2;
                offset2 = 1;
            } else {
                return_error(gs_error_typecheck);
            }
        }

        if (length2 < length1)
            length = length2;
        else
            length = length1;
        code = strncmp((const char *)&pcde->key.data[offset2], (const char *)&Current->key.data[offset1], length);
        if (code > 0 || (code == 0 && length2 > length1)) {
            if (Next) {
                if (length3 < length2)
                    length = length3;
                else
                    length = length2;
                code = strncmp((const char *)&pcde->key.data[offset2], (const char *)&Next->key.data[offset3], length);
                if (code < 0 || (code == 0 && length3 > length2)) {
                    Next = pcde;
                    length3 = length2;
                    offset3 = offset2;
                }
            } else {
                Next = pcde;
                for (i = 0;Next->key.data[i] == 0x00; i++)
                    ;
                length3 = Next->key.size - i;
                offset3 = i;
                if (Next->key.data[offset3] == '/') {
                    length3 -= 1;
                    offset3 += 1;
                } else {
                    if (Next->key.data[0] == '(') {
                        length3 -= 2;
                        offset3 += 1;
                    } else {
                        return_error(gs_error_typecheck);
                    }
                }
            }
        }
        pcde = pcde->next;
    }
    *element = Next;
    return 0;
}

static int find_last_dict_entry(const cos_dict_t *d, const cos_dict_element_t **element)
{
    const cos_dict_element_t *pcde = d->elements, *Last, *Next;

    *element = 0L;

    Next = Last = pcde;

    do {
        Last = Next;
        find_next_dict_entry(d, &Next);
    } while (Next);

    *element = Last;

    return 0;
}
static int write_key_as_string_encrypted(const gx_device_pdf *pdev, const byte *str, uint size, gs_id object_id)
{
    stream sout;
    stream_PSSD_state st;
    stream_state so;
    byte bufo[100], *buffer;
    stream_arcfour_state sarc4;

    buffer = gs_alloc_bytes(pdev->pdf_memory, size, "encryption buffer");
    if (buffer == 0L)
        return 0;

    if (pdf_encrypt_init(pdev, object_id, &sarc4) < 0) {
        gs_free_object(pdev->pdf_memory, buffer, "Free encryption buffer");
        /* The interface can't pass an error. */
        stream_write(pdev->strm, str, size);
        return size;
    }
    s_init_state((stream_state *)&st, &s_PSSD_template, NULL);
    s_init(&sout, NULL);
    s_init_state(&so, &s_PSSE_template, NULL);
    s_init_filter(&sout, &so, bufo, sizeof(bufo), pdev->strm);
    stream_putc(pdev->strm, '(');
    memcpy(buffer, str, size);
    s_arcfour_process_buffer(&sarc4, buffer, size);
    stream_write(&sout, buffer, size);
    /* Another case where we use sclose() and not s_close_filters(), because the
     * buffer we supplied to s_init_filter is a heap based C object, so we
     * must not free it.
     */
    sclose(&sout); /* Writes ')'. */
    gs_free_object(pdev->pdf_memory, buffer, "Free encryption buffer");
    return 0;
}

static int write_key_as_string(const gx_device_pdf *pdev, stream *s, const cos_dict_element_t *element, gs_id object_id)
{
    int i, length, offset;

    /*
     * If the name has any 'unusual' characters, it is 'escaped' by starting with NULLs
     * I suspect we no longer need that, but here we remove the escaping NULLs
     */
    if (element->key.data[0] == 0x00) {
        for (i = 0;element->key.data[i] == 0x00; i++)
            ;
        length = element->key.size - (i + 1);
        offset = i;
    } else {
        offset = 0;
        length = element->key.size;
    }

    if (element->key.data[offset] == '/') {
        offset++;
        length--;
        if (!pdev->KeyLength || object_id == (gs_id)-1) {
            spputc(s, '(');
            stream_write(s, &element->key.data[offset], length);
            spputc(s, ')');
        }
        else
            write_key_as_string_encrypted(pdev, &element->key.data[offset], length, object_id);
    } else {
        if (!pdev->KeyLength || object_id == (gs_id)-1)
            stream_write(s, element->key.data, element->key.size);
        else
            write_key_as_string_encrypted(pdev, &element->key.data[1], element->key.size - 2, object_id);
    }
    return 0;
}

int
cos_write_dict_as_ordered_array(cos_object_t *pco, gx_device_pdf *pdev, pdf_resource_type_t type)
{
    stream *s;
    int code;
    const cos_dict_t *d;
    const cos_dict_element_t *pcde, *First, *Last;

    if (cos_type(pco) != cos_type_dict)
        return_error(gs_error_typecheck);

    d = (const cos_dict_t *)pco;

    if (pco->id == 0 || pco->written)
        return_error(gs_error_Fatal);
    pdf_open_separate(pdev, pco->id, type);

    s = pdev->strm;
    pcde = d->elements;
    if (!pcde){
        stream_puts(s, "<<>>\n");
        pdf_end_separate(pdev, type);
        return 0;
    }

    code = find_first_dict_entry(d, &First);
    if (code < 0) {
        pdf_end_separate(pdev, type);
        return code;
    }

    code = find_last_dict_entry(d, &Last);
    if (code < 0) {
        pdf_end_separate(pdev, type);
        return code;
    }

    stream_puts(s, "<<\n/Limits [\n");
    write_key_as_string(pdev, s, First, pco->id);
    stream_puts(s, "\n");
    write_key_as_string(pdev, s, Last, pco->id);
    stream_puts(s, "\n]\n");
    stream_puts(s, "/Names [");
    do {
        stream_puts(s, "\n");
        write_key_as_string(pdev, s, First, pco->id);
        cos_value_write_spaced(&First->value, pdev, true, -1);
        find_next_dict_entry(d, &First);
    } while (First);
    stream_puts(s, "]\n>>\n");

    pdf_end_separate(pdev, type);
    pco->written = true;
    return code;
}

/* Write/delete definitions of named objects. */
/* This is a special-purpose facility for pdf_close. */
int
cos_dict_objects_write(const cos_dict_t *pcd, gx_device_pdf *pdev)
{
    const cos_dict_element_t *pcde = pcd->elements;

    for (; pcde; pcde = pcde->next)
        if (COS_VALUE_IS_OBJECT(&pcde->value) &&
            pcde->value.contents.object->id  &&
            !pcde->value.contents.object->written /* ForOPDFRead only. */)
            cos_write_object(pcde->value.contents.object, pdev, resourceOther);
    return 0;
}
int
cos_dict_objects_delete(cos_dict_t *pcd)
{
    cos_dict_element_t *pcde = pcd->elements;

    /*
     * Delete duplicate references to prevent a dual object freeing.
     * Delete the objects' IDs so that freeing the dictionary will
     * free them.
     */
    for (; pcde; pcde = pcde->next) {
        if (pcde->value.contents.object) {
            cos_dict_element_t *pcde1 = pcde->next;

            for (; pcde1; pcde1 = pcde1->next)
                if (pcde->value.contents.object == pcde1->value.contents.object)
                    pcde1->value.contents.object = NULL;
            pcde->value.contents.object->id = 0;
        }
    }
    return 0;
}

/* Put an element in a dictionary. */
#define DICT_COPY_KEY 1
#define DICT_COPY_VALUE 2
#define DICT_FREE_KEY 4
#define DICT_COPY_ALL (DICT_COPY_KEY | DICT_COPY_VALUE | DICT_FREE_KEY)
static int
cos_dict_put_copy(cos_dict_t *pcd, const byte *key_data, uint key_size,
                  const cos_value_t *pvalue, int flags)
{
    gs_memory_t *mem = COS_OBJECT_MEMORY(pcd);
    cos_dict_element_t **ppcde = &pcd->elements;
    cos_dict_element_t *pcde;
    cos_dict_element_t *next;
    cos_value_t value;
    int code;

    while ((next = *ppcde) != 0 &&
           bytes_compare(next->key.data, next->key.size, key_data, key_size)
           )
        ppcde = &next->next;
    if (next) {
        /* We're replacing an existing element. */
        if ((pvalue->value_type == COS_VALUE_SCALAR ||
             pvalue->value_type == COS_VALUE_CONST) &&
            pvalue->value_type == next->value.value_type &&
            !bytes_compare(pvalue->contents.chars.data, pvalue->contents.chars.size,
                next->value.contents.chars.data, next->value.contents.chars.size))
            return 0; /* Same as old value. */
        if ((pvalue->value_type == COS_VALUE_OBJECT ||
             pvalue->value_type == COS_VALUE_RESOURCE) &&
            pvalue->value_type == next->value.value_type &&
            pvalue->contents.object == next->value.contents.object)
            return 0; /* Same as old value. */
        code = cos_copy_element_value(&value, mem, pvalue,
                                      (flags & DICT_COPY_VALUE) != 0);
        if (code < 0)
            return code;
        cos_value_free(&next->value, mem,
                       "cos_dict_put(old value)");
        pcde = next;
    } else {
        /* Create a new element. */
        byte *copied_key_data;

        if (flags & DICT_COPY_KEY) {
            copied_key_data = gs_alloc_string(mem, key_size,
                                              "cos_dict_put(key)");
            if (copied_key_data == 0)
                return_error(gs_error_VMerror);
            memcpy(copied_key_data, key_data, key_size);
        } else
            copied_key_data = (byte *)key_data;	/* OK to break const */
        pcde = gs_alloc_struct(mem, cos_dict_element_t, &st_cos_dict_element,
                               "cos_dict_put(element)");
        code = cos_copy_element_value(&value, mem, pvalue,
                                      (flags & DICT_COPY_VALUE) != 0);
        if (pcde == 0 || code < 0) {
            if (code >= 0)
                cos_uncopy_element_value(&value, mem,
                                         (flags & DICT_COPY_VALUE) != 0);
            gs_free_object(mem, pcde, "cos_dict_put(element)");
            if (flags & DICT_COPY_KEY)
                gs_free_string(mem, copied_key_data, key_size,
                               "cos_dict_put(key)");
            return (code < 0 ? code : gs_note_error(gs_error_VMerror));
        }
        pcde->key.data = copied_key_data;
        pcde->key.size = key_size;
        pcde->owns_key = (flags & DICT_FREE_KEY) != 0;
        pcde->next = next;
        *ppcde = pcde;
    }
    pcde->value = value;
    pcd->md5_valid = false;
    return 0;
}
int
cos_dict_put(cos_dict_t *pcd, const byte *key_data, uint key_size,
             const cos_value_t *pvalue)
{
    return cos_dict_put_copy(pcd, key_data, key_size, pvalue, DICT_COPY_ALL);
}
int
cos_dict_put_no_copy(cos_dict_t *pcd, const byte *key_data, uint key_size,
                     const cos_value_t *pvalue)
{
    return cos_dict_put_copy(pcd, key_data, key_size, pvalue,
                             DICT_COPY_KEY | DICT_FREE_KEY);
}
int
cos_dict_put_c_key(cos_dict_t *pcd, const char *key, const cos_value_t *pvalue)
{
    return cos_dict_put_copy(pcd, (const byte *)key, strlen(key), pvalue,
                             DICT_COPY_VALUE);
}
int
cos_dict_put_c_key_string(cos_dict_t *pcd, const char *key,
                          const byte *data, uint size)
{
    cos_value_t value;

    cos_string_value(&value, data, size);
    return cos_dict_put_c_key(pcd, key, &value);
}
int
cos_dict_put_c_key_int(cos_dict_t *pcd, const char *key, int value)
{
    char str[sizeof(int) * 8 / 3 + 3]; /* sign, rounding, 0 terminator */

    gs_snprintf(str, sizeof(str), "%d", value);
    return cos_dict_put_c_key_string(pcd, key, (byte *)str, strlen(str));
}
int
cos_dict_put_c_key_bool(cos_dict_t *pcd, const char *key, bool value)
{
    return cos_dict_put_c_key_string(pcd, key,
                (const byte *)(value ? "true" : "false"),
                              (value ? 4 : 5));
}
int
cos_dict_put_c_key_real(cos_dict_t *pcd, const char *key, double value)
{
    byte str[50];		/****** ADHOC ******/
    stream s;

    s_init(&s, NULL);
    swrite_string(&s, str, sizeof(str));
    pprintg1(&s, "%g", value);
    return cos_dict_put_c_key_string(pcd, key, str, stell(&s));
}
int
cos_dict_put_c_key_floats(gx_device_pdf *pdev, cos_dict_t *pcd, const char *key, const float *pf,
                          uint size)
{
    cos_array_t *pca = cos_array_from_floats(pdev, pf, size,
                                             "cos_dict_put_c_key_floats");
    int code;

    if (pca == 0)
        return_error(gs_error_VMerror);
    code = cos_dict_put_c_key_object(pcd, key, COS_OBJECT(pca));
    if (code < 0)
        COS_FREE(pca, "cos_dict_put_c_key_floats");
    return code;
}
int
cos_dict_put_c_key_object(cos_dict_t *pcd, const char *key, cos_object_t *pco)
{
    cos_value_t value;

    return cos_dict_put_c_key(pcd, key, cos_object_value(&value, pco));
}
int
cos_dict_put_string(cos_dict_t *pcd, const byte *key_data, uint key_size,
                    const byte *value_data, uint value_size)
{
    cos_value_t cvalue;

    return cos_dict_put(pcd, key_data, key_size,
                        cos_string_value(&cvalue, value_data, value_size));
}
int
cos_dict_put_string_copy(cos_dict_t *pcd, const char *key, const char *value)
{
    return cos_dict_put_c_key_string(pcd, key, (byte *)value, strlen(value));
}
int
cos_dict_put_c_strings(cos_dict_t *pcd, const char *key, const char *value)
{
    cos_value_t cvalue;

    return cos_dict_put_c_key(pcd, key, cos_c_string_value(&cvalue, value));
}

/* Move all the elements from one dict to another. */
int
cos_dict_move_all(cos_dict_t *pcdto, cos_dict_t *pcdfrom)
{
    cos_dict_element_t *pcde = pcdfrom->elements;
    cos_dict_element_t *head = pcdto->elements;

    while (pcde) {
        cos_dict_element_t *next = pcde->next;

        if (cos_dict_find(pcdto, pcde->key.data, pcde->key.size)) {
            /* Free the element, which has been superseded. */
            cos_dict_element_free(pcdfrom, pcde, "cos_dict_move_all_from");
        } else {
            /* Move the element. */
            pcde->next = head;
            head = pcde;
        }
        pcde = next;
    }
    pcdto->elements = head;
    pcdfrom->elements = 0;
    pcdto->md5_valid = false;
    return 0;
}

/* Look up a key in a dictionary. */
const cos_value_t *
cos_dict_find(const cos_dict_t *pcd, const byte *key_data, uint key_size)
{
    cos_dict_element_t *pcde = pcd->elements;

    for (; pcde; pcde = pcde->next)
        if (!bytes_compare(key_data, key_size, pcde->key.data, pcde->key.size))
            return &pcde->value;
    return 0;
}
const cos_value_t *
cos_dict_find_c_key(const cos_dict_t *pcd, const char *key)
{
    if (pcd == NULL)
        return NULL;
    return cos_dict_find(pcd, (const byte *)key, strlen(key));
}

int cos_dict_hash(const cos_object_t *pco0, gs_md5_state_t *md5, gs_md5_byte_t *hash, gx_device_pdf *pdev)
{
    cos_dict_t *dict = (cos_dict_t *) pco0;
    cos_dict_element_t *pcde0 = dict->elements;

    for (; pcde0; pcde0 = pcde0->next) {
        gs_md5_append(md5, pcde0->key.data, pcde0->key.size);
        cos_value_hash(&pcde0->value, md5, hash, pdev);
    }
    return 0;
}

/* Compare two dictionaries. */
int
cos_dict_equal(const cos_object_t *pco0, const cos_object_t *pco1, gx_device_pdf *pdev)
{
    int code = 0;

    if (!pco0->md5_valid) {
        gs_md5_init((gs_md5_state_t *)&pco0->md5);
        code = cos_dict_hash(pco0, (gs_md5_state_t *)&pco0->md5, (gs_md5_byte_t *)pco0->hash, pdev);
        if (code < 0)
            return code;
        gs_md5_finish((gs_md5_state_t *)&pco0->md5, (gs_md5_byte_t *)pco0->hash);
        ((cos_object_t *)pco0)->md5_valid = true;
    }
    if (!pco1->md5_valid) {
        gs_md5_init((gs_md5_state_t *)&pco1->md5);
        code = cos_dict_hash(pco1, (gs_md5_state_t *)&pco1->md5, (gs_md5_byte_t *)pco1->hash, pdev);
        if (code < 0)
            return code;
        gs_md5_finish((gs_md5_state_t *)&pco1->md5, (gs_md5_byte_t *)pco1->hash);
        ((cos_object_t *)pco1)->md5_valid = true;
    }
    if (memcmp(&pco0->hash, &pco1->hash, 16) != 0)
        return false;

    return true;
}

/* Process all entries in a dictionary. */
int
cos_dict_forall(const cos_dict_t *pcd, void *client_data,
                int (*proc)(void *client_data, const byte *key_data, uint key_size, const cos_value_t *v))
{
    cos_dict_element_t *pcde = pcd->elements;

    for (; pcde; pcde = pcde->next) {
        int code = proc(client_data, pcde->key.data, pcde->key.size, &pcde->value);

        if (code != 0)
            return code;
    }
    return 0;
}

/* Set up a parameter list that writes into a Cos dictionary. */

/* We'll implement the other printers later if we have to. */
static param_proc_xmit_typed(cos_param_put_typed);
static const gs_param_list_procs cos_param_list_writer_procs = {
    cos_param_put_typed,
    NULL /* begin_collection */ ,
    NULL /* end_collection */ ,
    NULL /* get_next_key */ ,
    gs_param_request_default,
    gs_param_requested_default
};
static int
cos_param_put_typed(gs_param_list * plist, gs_param_name pkey,
                    gs_param_typed_value * pvalue)
{
    cos_param_list_writer_t *const pclist =
        (cos_param_list_writer_t *)plist;
    gx_device_pdf *pdev = pclist->pdev;
    gs_memory_t *mem = pclist->memory;
    cos_value_t value;
    cos_array_t *pca;
    int key_len = strlen(pkey);
    byte key_chars[100];		/****** ADHOC ******/
    int code;

    while(pdev->child)
        pdev = (gx_device_pdf *)pdev->child;

    if (key_len > sizeof(key_chars) - 1)
        return_error(gs_error_limitcheck);
    switch (pvalue->type) {
    default: {
        param_printer_params_t ppp;
        printer_param_list_t pplist;
        stream s;
        int len, skip;
        byte *str;

        s_init(&s, NULL);
        ppp = param_printer_params_default;
        ppp.prefix = ppp.suffix = ppp.item_prefix = ppp.item_suffix = 0;
        ppp.print_ok = pclist->print_ok;
        s_init_param_printer(&pplist, &ppp, &s);
        swrite_position_only(&s);
        param_write_typed((gs_param_list *)&pplist, "", pvalue);
        len = stell(&s);
        str = gs_alloc_string(mem, len, "cos_param_put(string)");
        if (str == 0)
            return_error(gs_error_VMerror);
        swrite_string(&s, str, len);
        param_write_typed((gs_param_list *)&pplist, "", pvalue);
        /*
         * The string starts with an initial / or /<space>, which
         * we need to remove.
         */
        skip = (str[1] == ' ' ? 2 : 1);
        memmove(str, str + skip, len - skip);
        str = gs_resize_string(mem, str, len, len - skip,
                               "cos_param_put(string)");
        cos_string_value(&value, str, len - skip);
    }
        break;
    case gs_param_type_int_array: {
        uint i;

        pca = cos_array_alloc(pdev, "cos_param_put(array)");
        if (pca == 0)
            return_error(gs_error_VMerror);
        for (i = 0; i < pvalue->value.ia.size; ++i)
            CHECK(cos_array_add_int(pca, pvalue->value.ia.data[i]));
    }
    av:
        cos_object_value(&value, COS_OBJECT(pca));
        break;
    case gs_param_type_float_array: {
        uint i;

        pca = cos_array_alloc(pdev, "cos_param_put(array)");
        if (pca == 0)
            return_error(gs_error_VMerror);
        for (i = 0; i < pvalue->value.ia.size; ++i)
            CHECK(cos_array_add_real(pca, pvalue->value.fa.data[i]));
    }
        goto av;
    case gs_param_type_string_array:
    case gs_param_type_name_array:
        /****** NYI ******/
        return_error(gs_error_typecheck);
    }
    memcpy(key_chars + 1, pkey, key_len);
    key_chars[0] = '/';
    return cos_dict_put_no_copy(pclist->pcd, key_chars, key_len + 1, &value);
}

int
cos_param_list_writer_init(gx_device_pdf *pdev, cos_param_list_writer_t *pclist, cos_dict_t *pcd,
                           int print_ok)
{
    gs_param_list_init((gs_param_list *)pclist, &cos_param_list_writer_procs,
                       COS_OBJECT_MEMORY(pcd));
    pclist->pcd = pcd;
    pclist->pdev = pdev;
    pclist->print_ok = print_ok;
    return 0;
}

/* ------ Streams ------ */

static cos_proc_release(cos_stream_release);
cos_proc_write(cos_stream_write);
static cos_proc_equal(cos_stream_equal);
static cos_proc_hash(cos_stream_hash);
const cos_object_procs_t cos_stream_procs = {
    cos_stream_release, cos_stream_write, cos_stream_equal, cos_stream_hash
};

cos_stream_t *
cos_stream_alloc(gx_device_pdf *pdev, client_name_t cname)
{
    gs_memory_t *mem = pdev->pdf_memory;
    cos_stream_t *pcs =
        gs_alloc_struct(mem, cos_stream_t, &st_cos_object, cname);

    cos_object_init((cos_object_t *)pcs, pdev, &cos_stream_procs);
    return pcs;
}

static void
cos_stream_release(cos_object_t *pco, client_name_t cname)
{
    cos_stream_t *const pcs = (cos_stream_t *)pco;
    cos_stream_piece_t *cur;
    cos_stream_piece_t *next;

    for (cur = pcs->pieces; cur; cur = next) {
        next = cur->next;
        gs_free_object(cos_object_memory(pco), cur, cname);
    }
    pcs->pieces = 0;
    cos_dict_release(pco, cname);
}

static int hash_cos_stream(const cos_object_t *pco0, gs_md5_state_t *md5, gs_md5_byte_t *hash, gx_device_pdf *pdev)
{
    const cos_stream_t *pcs = (const cos_stream_t *)pco0;
    cos_stream_piece_t *pcsp = pcs->pieces;
    gp_file *sfile = pdev->streams.file;
    byte *ptr;
    int64_t position_save;
    int result;

    sflush(pdev->strm);
    sflush(pdev->streams.strm);
    position_save = gp_ftell(sfile);

    if (!pcsp)
        return -1;

    gs_md5_init(md5);
    while(pcsp) {
        ptr = gs_malloc(pdev->memory, sizeof (byte), pcsp->size, "hash_cos_stream");
        if (ptr == 0L) {
            result = gs_note_error(gs_error_VMerror);
            return result;
        }
        if (gp_fseek(sfile, pcsp->position, SEEK_SET) != 0)
	  return_error(gs_error_ioerror);

        if (gp_fread(ptr, 1, pcsp->size, sfile) != pcsp->size) {
            gs_free(pdev->memory, ptr, sizeof (byte), pcsp->size, "hash_cos_stream");
            result = gs_note_error(gs_error_ioerror);
            return result;
        }
        gs_md5_append(md5, ptr, pcsp->size);
        gs_free(pdev->memory, ptr, sizeof (byte), pcsp->size, "hash_cos_stream");
        pcsp = pcsp->next;
    }
    gs_md5_finish(md5, (gs_md5_byte_t *)hash);
    if (gp_fseek(sfile, position_save, SEEK_SET) != 0)
      return_error(gs_error_ioerror);

    return 0;
}

static int cos_stream_hash(const cos_object_t *pco0, gs_md5_state_t *md5, gs_md5_byte_t *hash, gx_device_pdf *pdev)
{
    cos_stream_t *pcs0 = (cos_stream_t *)pco0;
    int code=0;
    if (!pco0->stream_md5_valid) {
        code = hash_cos_stream(pco0, (gs_md5_state_t *)&pco0->md5, (gs_md5_byte_t *)&pco0->stream_hash, pdev);
        if (code < 0)
            return code;
        pcs0->stream_md5_valid = 1;
    }
    gs_md5_append(md5, (byte *)&pco0->stream_hash, sizeof(pco0->stream_hash));
    if (!pco0->md5_valid) {
        gs_md5_init((gs_md5_state_t *)&pco0->md5);
        code = cos_dict_hash(pco0, (gs_md5_state_t *)&pco0->md5, (gs_md5_byte_t *)pco0->hash, pdev);
        if (code < 0)
            return code;
        gs_md5_finish((gs_md5_state_t *)&pco0->md5, (gs_md5_byte_t *)pco0->hash);
        pcs0->md5_valid = 1;
    }
    gs_md5_append(md5, (byte *)&pco0->hash, sizeof(pco0->hash));
    return code;
}

static int
cos_stream_equal(const cos_object_t *pco0, const cos_object_t *pco1, gx_device_pdf *pdev)
{
    cos_stream_t *pcs0 = (cos_stream_t *)pco0;
    cos_stream_t *pcs1 = (cos_stream_t *)pco1;
    int code;
    gs_md5_state_t md5;
    gs_md5_byte_t hash[16];

    gs_md5_init(&md5);

    if (!pco0->stream_md5_valid) {
        code = cos_stream_hash(pco0, &md5, (gs_md5_byte_t *)hash, pdev);
        if (code < 0)
            return false;
    }
    if (!pco1->stream_md5_valid) {
        code = cos_stream_hash(pco1, &md5, (gs_md5_byte_t *)hash, pdev);
        if (code < 0)
            return false;
    }
    if (memcmp(&pcs0->stream_hash, &pcs1->stream_hash, 16) != 0)
        return false;
    code = cos_dict_equal(pco0, pco1, pdev);
    if (code < 0)
        return false;
    if (!code)
        return false;
    return true;
}

/* Find the total length of a stream. */
int64_t
cos_stream_length(const cos_stream_t *pcs)
{
    return pcs->length;
}

/* Write the (dictionary) elements of a stream. */
/* (This procedure is exported.) */
int
cos_stream_elements_write(const cos_stream_t *pcs, gx_device_pdf *pdev)
{
    return cos_elements_write(pdev->strm, pcs->elements, pdev, true, pcs->id);
}

/* Write the contents of a stream.  (This procedure is exported.) */
int
cos_stream_contents_write(const cos_stream_t *pcs, gx_device_pdf *pdev)
{
    stream *s = pdev->strm;
    cos_stream_piece_t *pcsp;
    cos_stream_piece_t *last;
    cos_stream_piece_t *next;
    gp_file *sfile = pdev->streams.file;
    int64_t end_pos;
    bool same_file = (pdev->sbstack_depth > 0);
    int code;
    stream_arcfour_state sarc4, *ss = NULL;

    if (pdev->KeyLength) {
        code = pdf_encrypt_init(pdev, pcs->id, &sarc4);
        if (code < 0)
            return code;
        ss = &sarc4;
    }
    sflush(s);
    sflush(pdev->streams.strm);

    /* Reverse the elements temporarily. */
    for (pcsp = pcs->pieces, last = NULL; pcsp; pcsp = next)
        next = pcsp->next, pcsp->next = last, last = pcsp;
    for (pcsp = last, code = 0; pcsp && code >= 0; pcsp = pcsp->next) {
        if (same_file) {
            code = pdf_copy_data_safe(s, sfile, pcsp->position, pcsp->size);
            if (code < 0)
                return code;
        } else {
            end_pos = gp_ftell(sfile);
            if (gp_fseek(sfile, pcsp->position, SEEK_SET) != 0)
	      return_error(gs_error_ioerror);
            code = pdf_copy_data(s, sfile, pcsp->size, ss);
            if (code < 0)
                return code;
            if (gp_fseek(sfile, end_pos, SEEK_SET) != 0)
	      return_error(gs_error_ioerror);
        }
    }
    /* Reverse the elements back. */
    for (pcsp = last, last = NULL; pcsp; pcsp = next)
        next = pcsp->next, pcsp->next = last, last = pcsp;

    return code;
}

int
cos_stream_write(const cos_object_t *pco, gx_device_pdf *pdev, gs_id object_id)
{
    stream *s = pdev->strm;
    const cos_stream_t *const pcs = (const cos_stream_t *)pco;
    int code;

    if (pcs->input_strm != NULL) {
        stream *s = pco->input_strm;
        int status = s_close_filters(&s, NULL);

        if (status < 0)
            return_error(gs_error_ioerror);
        /* We have to break const here to clear the input_strm. */
        ((cos_object_t *)pco)->input_strm = 0;
    }
    stream_puts(s, "<<");
    cos_elements_write(s, pcs->elements, pdev, false, object_id);
    pprintld1(s, "/Length %ld>>stream\n", cos_stream_length(pcs));
    code = cos_stream_contents_write(pcs, pdev);
    stream_puts(s, "\nendstream\n");

    return code;
}

/* Return a stream's dictionary (just a cast). */
cos_dict_t *
cos_stream_dict(cos_stream_t *pcs)
{
    return (cos_dict_t *)pcs;
}

/* Add a contents piece to a stream object: size bytes just written on */
/* streams.strm. */
int
cos_stream_add(gx_device_pdf *pdev, cos_stream_t *pcs, uint size)
{
    stream *s;
    gs_offset_t position;
    cos_stream_piece_t *prev = pcs->pieces;

    /* Beware of subclassed devices. This is mad IMO, we should store
     * 's' in the stream state somewhere.
     */
    while (pdev->child)
        pdev = (gx_device_pdf *)pdev->child;

    s = pdev->streams.strm;
    position = stell(s);

    /* Check for consecutive writing -- just an optimization. */
    if (prev != 0 && prev->position + prev->size + size == position) {
        prev->size += size;
    } else {
        gs_memory_t *mem = pdev->pdf_memory;
        cos_stream_piece_t *pcsp =
            gs_alloc_struct(mem, cos_stream_piece_t, &st_cos_stream_piece,
                            "cos_stream_add");

        if (pcsp == 0)
            return_error(gs_error_VMerror);
        pcsp->position = position - size;
        pcsp->size = size;
        pcsp->next = pcs->pieces;
        pcs->pieces = pcsp;
    }
    pcs->length += size;
    return 0;
}

/* Add bytes to a stream object. */
int
cos_stream_add_bytes(gx_device_pdf *pdev, cos_stream_t *pcs, const byte *data, uint size)
{
    stream_write(pdev->streams.strm, data, size);
    return cos_stream_add(pdev, pcs, size);
}

/* Add the contents of a stream to a stream object. */
int
cos_stream_add_stream_contents(gx_device_pdf *pdev, cos_stream_t *pcs, stream *s)
{
    int code = 0;
    byte sbuff[200];	/* arbitrary */
    uint cnt;
    int status = sseek(s, 0);

    if (status < 0)
        return_error(gs_error_ioerror);
    do {
        status = sgets(s, sbuff, sizeof(sbuff), &cnt);

        if (cnt == 0) {
            if (status == EOFC)
                break;
            return_error(gs_error_ioerror);
        }
    } while ((code = cos_stream_add_bytes(pdev, pcs, sbuff, cnt)) >= 0);
    return code;
}

/* Release the last contents piece of a stream object. */
/* Warning : this function can't release pieces if another stream is written after them. */
int
cos_stream_release_pieces(gx_device_pdf *pdev, cos_stream_t *pcs)
{
    stream *s = pdev->streams.strm;
    gs_offset_t position = stell(s), position0 = position;

    while (pcs->pieces != NULL &&
                position == pcs->pieces->position + pcs->pieces->size) {
        cos_stream_piece_t *p = pcs->pieces;

        position -= p->size;
        pcs->pieces = p->next;
        gs_free_object(cos_object_memory((cos_object_t *)pcs), p, "cos_stream_release_pieces");
    }
    if (position0 != position)
        if (sseek(s, position) < 0)
            return_error(gs_error_ioerror);
    return 0;
}

/* Create a stream that writes into a Cos stream. */
/* Closing the stream will free it. */
/* Note that this is not a filter. */
typedef struct cos_write_stream_state_s {
    stream_state_common;
    cos_stream_t *pcs;
    gx_device_pdf *pdev;
    stream *s;			/* pointer back to stream */
    stream *target;		/* use this instead of strm */
} cos_write_stream_state_t;
gs_private_st_suffix_add4(st_cos_write_stream_state, cos_write_stream_state_t,
                          "cos_write_stream_state_t",
                          cos_ws_state_enum_ptrs, cos_ws_state_reloc_ptrs,
                          st_stream_state, pcs, pdev, s, target);

static int
cos_write_stream_process(stream_state * st, stream_cursor_read * pr,
                         stream_cursor_write * ignore_pw, bool last)
{
    uint count = pr->limit - pr->ptr;
    cos_write_stream_state_t *ss = (cos_write_stream_state_t *)st;
    stream *target = ss->target;
    gx_device_pdf *pdev = ss->pdev;
    gs_offset_t start_pos;
    int code;

    while(pdev->child != NULL)
        pdev = (gx_device_pdf *)pdev->child;

    start_pos = stell(pdev->streams.strm);
    stream_write(target, pr->ptr + 1, count);
    gs_md5_append(&ss->pcs->md5, pr->ptr + 1, count);
    pr->ptr = pr->limit;
    sflush(target);
    code = cos_stream_add(pdev, ss->pcs, (uint)(stell(pdev->streams.strm) - start_pos));
    return (code < 0 ? ERRC : 0);
}
static int
cos_write_stream_close(stream *s)
{
    cos_write_stream_state_t *ss = (cos_write_stream_state_t *)s->state;
    int status;
    gx_device_pdf *target_dev = (gx_device_pdf *)ss->pdev;

    while (target_dev->child != NULL)
        target_dev = (gx_device_pdf *)target_dev->child;

    sflush(s);
    status = s_close_filters(&ss->target, target_dev->streams.strm);
    gs_md5_finish(&ss->pcs->md5, (gs_md5_byte_t *)ss->pcs->stream_hash);
    ss->pcs->stream_md5_valid = 1;
    return (status < 0 ? status : s_std_close(s));
}

static const stream_procs cos_s_procs = {
    s_std_noavailable, s_std_noseek, s_std_write_reset,
    s_std_write_flush, cos_write_stream_close, cos_write_stream_process
};
static const stream_template cos_write_stream_template = {
    &st_cos_write_stream_state, 0, cos_write_stream_process, 1, 1
};
stream *
cos_write_stream_alloc(cos_stream_t *pcs, gx_device_pdf *pdev,
                       client_name_t cname)
{
    gs_memory_t *mem = pdev->pdf_memory;
    stream *s = s_alloc(mem, cname);
    cos_write_stream_state_t *ss = (cos_write_stream_state_t *)
        s_alloc_state(mem, &st_cos_write_stream_state, cname);
#define CWS_BUF_SIZE 512	/* arbitrary */
    byte *buf = gs_alloc_bytes(mem, CWS_BUF_SIZE, cname);

    if (s == 0 || ss == 0 || buf == 0)
        goto fail;
    ss->templat = &cos_write_stream_template;
    ss->pcs = pcs;
    ss->pcs->stream_md5_valid = 0;
    gs_md5_init(&ss->pcs->md5);
    memset(&ss->pcs->hash, 0x00, 16);
    ss->pdev = pdev;
    while(ss->pdev->parent)
        ss->pdev = (gx_device_pdf *)ss->pdev->parent;
    ss->s = s;
    ss->target = pdev->streams.strm; /* not s->strm */
    s_std_init(s, buf, CWS_BUF_SIZE, &cos_s_procs, s_mode_write);
    s->state = (stream_state *)ss;
    return s;
#undef CWS_BUF_SIZE
 fail:
    gs_free_object(mem, buf, cname);
    gs_free_object(mem, ss, cname);
    gs_free_object(mem, s, cname);
    return 0;
}

/* Get cos stream from pipeline. */
cos_stream_t *
cos_stream_from_pipeline(stream *s)
{
    cos_write_stream_state_t *ss;

    while(s->procs.process != cos_s_procs.process) {
        s = s->strm;
        if (s == 0L)
            return 0L;
    }

    ss = (cos_write_stream_state_t *)s->state;
    return ss->pcs;
}
