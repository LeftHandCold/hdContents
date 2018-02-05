//
// Created by sjw on 2018/2/5.
//

#include "hdtd.h"
#include "pdf.h"

#include "pdf-encodings.h"
#include "pdf-glyphlist.h"

void
pdf_load_encoding(const char **estrings, const char *encoding)
{
    const char * const *bstrings = NULL;
    int i;

    if (!strcmp(encoding, "StandardEncoding"))
        bstrings = pdf_standard;
    if (!strcmp(encoding, "MacRomanEncoding"))
        bstrings = pdf_mac_roman;
    if (!strcmp(encoding, "MacExpertEncoding"))
        bstrings = pdf_mac_expert;
    if (!strcmp(encoding, "WinAnsiEncoding"))
        bstrings = pdf_win_ansi;

    if (bstrings)
        for (i = 0; i < 256; i++)
            estrings[i] = bstrings[i];
}

int
pdf_lookup_agl(const char *name)
{
    char buf[64];
    char *p;
    int l = 0;
    int r = nelem(agl_name_list) - 1;
    int code = 0;

    hd_strlcpy(buf, name, sizeof buf);

    /* kill anything after first period and underscore */
    p = strchr(buf, '.');
    if (p) p[0] = 0;
    p = strchr(buf, '_');
    if (p) p[0] = 0;

    while (l <= r)
    {
        int m = (l + r) >> 1;
        int c = strcmp(buf, agl_name_list[m]);
        if (c < 0)
            r = m - 1;
        else if (c > 0)
            l = m + 1;
        else
            return agl_code_list[m];
    }

    if (buf[0] == 'u' && buf[1] == 'n' && buf[2] == 'i')
        code = strtol(buf + 3, NULL, 16);
    else if (buf[0] == 'u')
        code = strtol(buf + 1, NULL, 16);
    else if (buf[0] == 'a' && buf[1] != 0 && buf[2] != 0)
        code = strtol(buf + 1, NULL, 10);

    return (code > 0 && code <= 0x10ffff) ? code : HD_REPLACEMENT_CHARACTER;
}