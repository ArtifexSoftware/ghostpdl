/* Copyright (C) 2022-2024 Artifex Software, Inc.
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

#ifndef PARAM
#define PARAM(A,B) A
#endif
PARAM(E_PDF_NOERROR,                   "no error"),
PARAM(E_PDF_NOHEADER,                  "no header detected"),
PARAM(E_PDF_NOHEADERVERSION,           "header lacks a version number"),
PARAM(E_PDF_NOSTARTXREF,               "no startxref token found"),
PARAM(E_PDF_BADSTARTXREF,              "startxref offset invalid"),
PARAM(E_PDF_BADXREFSTREAM,             "couldn't read hybrid file's XrefStm"),
PARAM(E_PDF_BADXREF,                   "error in xref table"),
PARAM(E_PDF_SHORTXREF,                 "too few entries in xref table"),
PARAM(E_PDF_PREV_NOT_XREF_STREAM,      "The /Prev entry in an XrefStm dictionary did not point to an XrefStm"),
PARAM(E_PDF_MISSINGENDSTREAM,          "content stream lacks endstream"),
PARAM(E_PDF_UNKNOWNFILTER,             "request for unknown filter"),
PARAM(E_PDF_MISSINGWHITESPACE,         "missing white space after number"),
PARAM(E_PDF_MALFORMEDNUMBER,           "malformed number"),
PARAM(E_PDF_NUMBEROVERFLOW,            "integer overflowed"),
PARAM(E_PDF_UNESCAPEDSTRING,           "unbalanced or unescaped character '(' in string"),
PARAM(E_PDF_BADOBJNUMBER,              "invalid object number"),
PARAM(E_PDF_MISSINGENDOBJ,             "object lacks an endobj"),
PARAM(E_PDF_TOKENERROR,                "error executing PDF token"),
PARAM(E_PDF_KEYWORDTOOLONG,            "potential token is too long"),
PARAM(E_PDF_BADPAGETYPE,               "Page object does not have /Page type"),
PARAM(E_PDF_CIRCULARREF,               "circular reference to indirect object"),
PARAM(E_PDF_CIRCULARNAME,              "Definition of a name is a reference to itself"),
PARAM(E_PDF_UNREPAIRABLE,              "Can't repair xref, repair already performed"),
PARAM(E_PDF_REPAIRED,                  "xref table was repaired"),
PARAM(E_PDF_BADSTREAM,                 "error reading a stream"),
PARAM(E_PDF_MISSINGOBJ,                "obj token missing"),
PARAM(E_PDF_BADPAGEDICT,               "error in Page dictionary"),
PARAM(E_PDF_OUTOFMEMORY,               "out of memory"),
PARAM(E_PDF_PAGEDICTERROR,             "error reading page dictionary"),
PARAM(E_PDF_STACKUNDERFLOWERROR,       "stack underflow"),
PARAM(E_PDF_BADSTREAMDICT,             "error in stream dictionary"),
PARAM(E_PDF_INHERITED_STREAM_RESOURCE, "stream inherited a resource"),
PARAM(E_PDF_DEREF_FREE_OBJ,            "Attempt to dereference a free object"),
PARAM(E_PDF_INVALID_TRANS_XOBJECT,     "error in transparency XObject"),
PARAM(E_PDF_NO_SUBTYPE,                "object lacks a required Subtype"),
PARAM(E_PDF_BAD_SUBTYPE,               "Object has an unrecognised Subtype"),
PARAM(E_PDF_IMAGECOLOR_ERROR,          "error in image colour"),
PARAM(E_DICT_SELF_REFERENCE,           "dictionary contains a key which (indirectly) references the dictionary."),
PARAM(E_IMAGE_MASKWITHCOLOR,           "Image has both ImageMask and ColorSpace keys."),
PARAM(E_PDF_INVALID_DECRYPT_LEN,       "Invalid /Length in Encryption dictionary (not in range 40-128 or not a multiple of 8)."),
PARAM(E_PDF_GROUP_NO_CS,               "Group attributes dictionary is missing /CS"),
PARAM(E_BAD_GROUP_DICT,                "Error retrieving Group dictionary for a page or XObject"),
PARAM(E_BAD_HALFTONE,                  "Error setting a halftone"),
PARAM(E_PDF_BADENCRYPT,                "Encrypt diciotnary not a dictionary"),
PARAM(E_PDF_MISSINGTYPE,               "A dictionary is missing a required /Type key."),
PARAM(E_PDF_NESTEDTOODEEP,             "Dictionaries/arrays nested too deeply"),
PARAM(E_PDF_UNMATCHEDMARK,             "A closing mark (] or >>) had no matching mark, ignoring the closing mark"),
PARAM(E_PDF_BADPAGECOUNT,              "page tree root node /Count did not match the actual number of pages in the tree."),
PARAM(E_PDF_NO_ROOT,                   "Can't find the document Catalog"),
PARAM(E_PDF_BAD_ROOT_TYPE,             "Document Catalog has incorrect /Type"),
PARAM(E_PDF_BAD_NAMED_DEST,            "An annotation has an invalid named destination"),
PARAM(E_PDF_BAD_LENGTH,                "Incorrect /Length for stream object"),
PARAM(E_PDF_DICT_IS_STREAM,            "Expected a dictionary but encountered a stream"),
PARAM(E_PDF_BAD_TYPE,                  "An object is of the wrong type"),
PARAM(E_PDF_BAD_VALUE,                 "An object has an unexpected value"),
PARAM(E_PDF_BAD_ANNOTATION,            "There was an error in an annotation"),
PARAM(E_PDF_BAD_XREFSTMOFFSET,         "An XRefStm value did not point to a cross reference stream"),
PARAM(E_PDF_SMASK_MISSING_G,           "An SMask is missing the required Group (/G) key, SMask was ignored"),
PARAM(E_PDF_PS_XOBJECT_IGNORED,        "An XObject had the Subtype /PS which is deprecated, the XObject was ignored"),
PARAM(E_PDF_TRANS_CHK_BADTYPE,         "An object (eg XObject, Pattern) being checked for transparency had the wrong type and was not checked"),
PARAM(E_PDF_SPOT_CHK_BADTYPE,          "An object (eg Shading) being checked for spot colours had the wrong type and was not checked"),
PARAM(E_PDF_BG_ISNAME,                 "An ExtGState dictionary has a /BG key with a value which is a name, not a function"),
PARAM(E_PDF_UCR_ISNAME,                "An ExtGState dictionary has a /UCR key with a value which is a name, not a function"),
PARAM(E_PDF_TR_NAME_NOT_IDENTITY,      "An ExtGState dictionary has a /TR key with a value which is a name other than /Identity"),
PARAM(E_PDF_ICC_BAD_N,                 "ICCbased space /N value does not match the number of components in the ICC profile"),
PARAM(E_PDF_MISSING_BBOX,              "A form is missing the required /BBox key"),
PARAM(E_PDF_GROUP_BAD_BC_TOO_BIG,      "An SMask has a /BC array with too many components. Ignoring specified BC"),
PARAM(E_PDF_GROUP_BAD_BC_NO_CS,        "An Smask has a /BC key, but the Group attributes dictionary has no /CS. Ignoring specified BC"),
#undef PARAM
