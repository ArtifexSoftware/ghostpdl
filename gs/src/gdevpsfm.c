/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*$Id$ */
/* Write a CMap */
#include "gx.h"
#include "gserrors.h"
#include "gxfcmap.h"
#include "stream.h"
#include "spprint.h"
#include "spsdf.h"
#include "gdevpsf.h"

/* ---------------- Utilities ---------------- */

typedef struct cmap_operators_s {
    const char *beginchar;
    const char *endchar;
    const char *beginrange;
    const char *endrange;
} cmap_operators_t;
private const cmap_operators_t
  cmap_cid_operators = {
    "begincidchar\n", "endcidchar\n",
    "begincidrange\n", "endcidrange\n"
  },
  cmap_notdef_operators = {
    "beginnotdefchar\n", "endnotdefchar\n",
    "beginnotdefrange\n", "endnotdefrange\n"
  };

/* Write a gs_string with a prefix. */
private void
pput_string_entry(stream *s, const char *prefix, const gs_const_string *pstr)
{
    pputs(s, prefix);
    pwrite(s, pstr->data, pstr->size);
}

/* Write a CID in hex. */
private void
pput_cid(stream *s, const byte *pcid, int size)
{
    int i;
    static const char *const hex_digits = "0123456789abcdef";

    for (i = 0; i < size; ++i) {
	pputc(s, hex_digits[pcid[i] >> 4]);
	pputc(s, hex_digits[pcid[i] & 0xf]);
    }
}

/* Write one code map. */
private int
cmap_put_code_map(stream *s, const gx_code_map_t *pccmap,
		  const cmap_operators_t *pcmo)
{
    /* For simplicity, produce one entry for each lookup range. */
    /****** IGNORES FILE INDEX ******/
    const gx_code_lookup_range_t *pclr = pccmap->lookup;
    int nl = pccmap->num_lookup;

    for (; nl > 0; ++pclr, --nl) {
	const byte *pkey = pclr->keys.data;
	const byte *pvalue = pclr->values.data;
	int gi;

	for (gi = 0; gi < pclr->num_keys; gi += 100) {
	    int i = gi, ni = min(i + 100, pclr->num_keys);
	    const char *end;

	    pprintd1(s, "%d ", ni - i);
	    if (pclr->key_is_range) {
		if (pclr->value_type == CODE_VALUE_CID) {
		    pputs(s, pcmo->beginrange);
		    end = pcmo->endrange;
		} else {
		    pputs(s, "beginbfrange\n");
		    end = "endbfrange\n";
		}
	    } else {
		if (pclr->value_type == CODE_VALUE_CID) {
		    pputs(s, pcmo->beginchar);
		    end = pcmo->endchar;
		} else {
		    pputs(s, "beginbfchar\n");
		    end = "endbfchar\n";
		}
	    }
	    for (; i < ni; ++i) {
		int j;
		long value;

		for (j = 0; j <= pclr->key_is_range; ++j) {
		    pputc(s, '<');
		    pput_cid(s, pclr->key_prefix, pclr->key_prefix_size);
		    pput_cid(s, pkey, pclr->key_size);
		    pputc(s, '>');
		    pkey += pclr->key_size;
		}
		for (j = 0, value = 0; j < pclr->value_size; ++j)
		    value = (value << 8) + *pvalue++;
		switch (pclr->value_type) {
		case CODE_VALUE_CID:
		case CODE_VALUE_CHARS:
		    pprintld1(s, "%ld\n", value);
		    break;
		case CODE_VALUE_GLYPH:
		    /****** PRINT GLYPH NAME -- NYI ******/
		    break;
		default:	/* not possible */
		    return_error(gs_error_rangecheck);
		}
	    }
	    pputs(s, end);
	}
    }
    return 0;
}

/* ---------------- Main program ---------------- */

/* Write a CMap in its standard (source) format. */
/****** DOESN'T HANDLE MULTI-FILE CMapS ******/
int
psf_write_cmap(stream *s, const gs_cmap_t *pcmap,
	       const gs_const_string *cmap_name)
{
    const gs_cid_system_info_t *const pcidsi = pcmap->CIDSystemInfo;
    int CMapVersion = 1;		/****** BOGUS ******/

    /* Write the header. */

    pputs(s, "%!PS-Adobe-3.0 Resource-CMap\n");
    pputs(s, "%%DocumentNeededResources: procset CIDInit\n");
    pputs(s, "%%IncludeResource: procset CIDInit\n");
    pput_string_entry(s, "%%BeginResource: CMap (", cmap_name);
    pput_string_entry(s, ")\n%%Title: (", cmap_name);
    pput_string_entry(s, " ", &pcidsi->Registry);
    pput_string_entry(s, " ", &pcidsi->Ordering);
    pprintd1(s, " %d)\n", pcidsi->Supplement);
    pprintd1(s, "%%%%Version: %d\n", CMapVersion);
    pputs(s, "/CIDInit /ProcSet findresource begin\n");
    pputs(s, "12 dict begin begincmap\n");

    /* Write the fixed entries. */

    pputs(s, "/CIDSystemInfo 3 dict dup begin\n");
    pputs(s, "/Registry ");
    s_write_ps_string(s, pcidsi->Registry.data, pcidsi->Registry.size, 0);
    pputs(s, " def\n/Ordering ");
    s_write_ps_string(s, pcidsi->Ordering.data, pcidsi->Ordering.size, 0);
    pprintd1(s, " def\n/Supplement %d def\nend def\n", pcidsi->Supplement);
    pput_string_entry(s, "/CMapName /", cmap_name); /****** ESCAPE NAME ******/
    pprintd1(s, " def\n/CMapVersion %d def\n", CMapVersion);
    pputs(s, "/CMapType 1 def\n");
    /****** UIDOffset ******/
    if (uid_is_XUID(&pcmap->uid)) {
	uint i, n = uid_XUID_size(&pcmap->uid);
	const long *values = uid_XUID_values(&pcmap->uid);

	pputs(s, "/XUID [");
	for (i = 0; i < n; ++i)
	    pprintld1(s, " %ld", values[i]);
	pputs(s, "] def\n");
    }
    pprintd1(s, "/WMode %d def\n", pcmap->WMode);

    /* Write the code space ranges. */

    {
	const gx_code_space_range_t *pcsr = pcmap->code_space.ranges;
	int gi;

	for (gi = 0; gi < pcmap->code_space.num_ranges; gi += 100) {
	    int i = gi, ni = min(i + 100, pcmap->code_space.num_ranges);

	    pprintd1(s, "%d begincodespacerange\n", ni - i);
	    for (; i < ni; ++i, ++pcsr) {
		pputs(s, "<");
		pput_cid(s, pcsr->first, pcsr->size);
		pputs(s, "><");
		pput_cid(s, pcsr->last, pcsr->size);
		pputs(s, ">\n");
	    }
	    pputs(s, "endcodespacerange\n");
	}
    }

    /* Write the code and notdef data. */

    cmap_put_code_map(s, &pcmap->notdef, &cmap_notdef_operators);
    cmap_put_code_map(s, &pcmap->def, &cmap_cid_operators);

    /* Write the trailer. */

    pputs(s, "endcmap\n");
    pputs(s, "CMapName currentdict /CMap defineresource pop end end\n");
    pputs(s, "%%EndResource\n");
    pputs(s, "%%EOF\n");

    return 0;
}
