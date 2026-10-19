/*
 * bcd.c - IBM 1401 BCD character encodings (see bcd.h)
 */
#include <string.h>
#include <ctype.h>
#include "bcd.h"

/* 64 characters each, in BCD order, BCD 0 (blank) first */
static const char enc_a[] =
    " 1234567890#@:>{^/STUVWXYZ|,%~\\\"-JKLMNOPQR!$*];_&ABCDEFGHI?.)[<}";
static const char enc_h[] =
    " 1234567890=':>{^/STUVWXYZ|,(~\\\"-JKLMNOPQR!$*];_+ABCDEFGHI?.)[<}";
static const char enc_s[] =
    " 1234567890#@:>(^/STUVWXYZ',%=\\+-JKLMNOPQR!$*];_&ABCDEFGHI?.)[<\"";

const char *bcd_enc_name = "A";
char bcd2chr[64];
unsigned char chr2bcd[256];

const char *bcd_encoding_string(int letter)
{
    switch (toupper(letter)) {
    case 'A': return enc_a;
    case 'H': return enc_h;
    case 'S': return enc_s;
    }
    return 0;
}

int bcd_set_encoding(int letter)
{
    const char *e = bcd_encoding_string(letter);
    int i;
    if (!e || strlen(e) != 64) return -1;
    for (i = 0; i < 256; i++) chr2bcd[i] = B_BLANK;
    /* lower case letters read as upper case */
    for (i = 0; i < 64; i++) {
        unsigned char c = (unsigned char)e[i];
        bcd2chr[i] = e[i];
        chr2bcd[c] = (unsigned char)i;
        if (isupper(c)) chr2bcd[(unsigned char)tolower(c)] = (unsigned char)i;
    }
    /* tolerant extras that do not collide with the selected encoding */
    {
        static const struct { char c; unsigned char b; } extra[] = {
            { '=', B_HASH }, { '\'', B_AT }, { '(', B_PERCNT },
            { '+', B_AMPER }, { '#', B_HASH }, { '@', B_AT },
            { '%', B_PERCNT }, { '&', B_AMPER }, { '|', B_RECMRK },
            { '}', B_GRPMRK }, { '"', B_GRPMRK }, { '{', B_TAPMRK },
            { '~', B_WMCHR }
        };
        for (i = 0; i < (int)(sizeof extra / sizeof extra[0]); i++)
            if (!strchr(e, extra[i].c))
                chr2bcd[(unsigned char)extra[i].c] = extra[i].b;
    }
    bcd_enc_name = (letter == 'h' || letter == 'H') ? "H" :
                   (letter == 's' || letter == 'S') ? "S" : "A";
    return 0;
}
