/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* MS Windows implementation of gp_getenv, and gp_serialnumber */

#include "windows_.h"

#include <stdio.h>
#include <stdlib.h>		/* for getenv */
#include <string.h>
#include "gscdefs.h"		/* for gs_productfamily and gs_revision and gs_serialnumber */

#if defined(__WIN32__) && !defined(METRO)
/*
 * Get a named registry value.
 * Key = hkeyroot\\key, named value = name.
 * name, ptr, plen and return values are the same as in gp_getenv();
 */
int
gp_getenv_registry(HKEY hkeyroot, const wchar_t *key, const char *name,
    char *ptr, int *plen)
{
    HKEY hkey;
    DWORD cbData, keytype;
    wchar_t w;
    LONG rc;
    wchar_t *wptr;
    wchar_t *wp = NULL;
    wchar_t *wname = NULL;
    int l = -1, size = 0;

    if (*plen) {
        wp = malloc((*plen)*sizeof(wchar_t));
        if (wp == NULL)
            return 1;
    }
    wptr = wp;

    size = gp_utf8_to_uint16(NULL, name);
    if (size >= 0)
        wname = malloc(size*sizeof(wchar_t));

    if (wname == NULL) {
        if (wp)
            free(wp);
        return 1;
    }
    gp_utf8_to_uint16(wname, name);

    if (RegOpenKeyExW(hkeyroot, key, 0, KEY_READ, &hkey) != ERROR_SUCCESS) {
        free(wname);
        if (wp)
            free(wp);
        return 1; /* Not found */
    }
    keytype = REG_SZ;
    cbData = (*plen) * sizeof(wchar_t);
    if (wptr == NULL)                   /* Registry API won't return */
        wptr = &w;	                /* ERROR_MORE_DATA if ptr is NULL */
    rc = RegQueryValueExW(hkey, wname, 0, &keytype, (BYTE *)wptr, &cbData);
    RegCloseKey(hkey);
    /* Process string keys, ignore keys that are not strings */
    if (keytype == REG_SZ) {
        switch (rc) {
            case ERROR_SUCCESS:
                if (wp) {
                    l = gp_uint16_to_utf8(NULL, wp);
                    if (l <= *plen) {
                        *plen = gp_uint16_to_utf8(ptr, wp);
                        free(wp);
                        free(wname);
                        return 0;       /* found environment variable and copied it */
                    } else;             /* buffer too small, so fall through */
                }                       /* caller only asked for the size, so fall through */
            case ERROR_MORE_DATA:
                /* buffer wasn't large enough or caller only asked for the size */
                if (l >= 0L) {
                    /* buffer size already computed above */
                    *plen = l;
                } else {
                    /* If we're probing for the size, then worst case, it can
                     * take 3 bytes out for every 2 bytes in. */
                    *plen = (cbData*3+1)/2;
                }
                if (wp)
                    free(wp);
                free(wname);
                return -1;              /* found environment variable, but buffer too small */
            default:
                break;
        }
    }
    if (wp)
        free(wp);
    free(wname);
    return 1;                           /* environment variable does not exist */
}
#endif    /* ifdef __WIN32__ */

/* ------ Environment variables ------ */

/* Get the value of an environment variable.  See gp.h for details. */
int
gp_getenv(const char *name, char *ptr, int *plen)
{
    wchar_t *wname = NULL;
    wchar_t *str;
    int size;

    size = gp_utf8_to_uint16(NULL, name);
    if (size >= 0)
        wname = malloc(size*sizeof(wchar_t));
    if (wname == NULL) {
        return -1;
    }
    gp_utf8_to_uint16(wname, name);

    str = _wgetenv(wname);

    free(wname);

    if (str) {
        /* gp_uint16_to_utf8 returns INCLUDING terminator */
        int len = gp_uint16_to_utf8(NULL, str);

        if (len <= *plen) {
            /* string fits */
            *plen = gp_uint16_to_utf8(ptr, str);
            return 0;
        }
        /* string doesn't fit */
        *plen = len;
        return -1;
    }
    /* environment variable was not found */

#if defined(__WIN32__) && !defined(METRO)
    {
        /* If using Win32, look in the registry for a value with
         * the given name.  The registry value will be under the key
         * HKEY_CURRENT_USER\Software\GPL Ghostscript\N.NN
         * or if that fails under the key
         * HKEY_LOCAL_MACHINE\Software\GPL Ghostscript\N.NN
         * where "GPL Ghostscript" is actually gs_productfamily
         * and N.NN is obtained from gs_revision.
         */
        DWORD version = GetVersion();

        if (!(((HIWORD(version) & 0x8000) != 0)
              && ((HIWORD(version) & 0x4000) == 0))) {
            /* not Win32s */
            int code;
            int major = (int)(gs_revision / 1000);
            int minor = (int)(gs_revision - (major * 1000)) / 10;
            int patch = gs_revision % 10;
            wchar_t key[256];
            wchar_t dotversion[16];

            wsprintfW(dotversion, L"%d.%02d.%d", major, minor, patch);
            wsprintfW(key, L"Software\\%hs\\%s", gs_productfamily, dotversion);
            code = gp_getenv_registry(HKEY_CURRENT_USER, key, name, ptr, plen);
            if ( code <= 0 )
                return code;	/* found it */

            code = gp_getenv_registry(HKEY_LOCAL_MACHINE, key, name, ptr, plen);
            if ( code <= 0 )
                return code;	/* found it */
        }
    }
#endif

    /* nothing found at all */

    if (*plen > 0)
        *ptr = 0;
    *plen = 1;
    return 1;
}

/* If we aren't on WIN32, We don't have a good way to get a serial */
/* number here, so just return what we always used to */
int
gp_serialnumber(void)
{
#if defined(__WIN32__) && !defined(METRO)
#define SERIALNUMBER_BUFSIZE 512
    byte buf[SERIALNUMBER_BUFSIZE];
    int buflen = SERIALNUMBER_BUFSIZE;
    int code, i;
    wchar_t key[256];

    wsprintfW(key, L"Software\\Microsoft\\MSLicensing\\HardwareID");
    code = gp_getenv_registry(HKEY_LOCAL_MACHINE, key, "ClientHWID", (char *)buf, &buflen);
    if ( code != 0 )
        return (int)(gs_serialnumber); 	/* error - just return the default */

    /* now turn the buffer into a 31-bit (avoid negative values) integer */
    for (i=0, code=0; i<buflen; i++)
        code += code + (buf[i] ^ i) + (code >> 31);      /* a cheap checksum */

    return code & 0x7fffffff;    /* strip the high bit */

#endif    /* __WIN32__ */

    return (int)(gs_serialnumber);
}
