/*
 * bcd.h - IBM 1401 character codes (BCD) and their external encodings.
 *
 * A 1401 storage position holds six data bits B A 8 4 2 1 and a word
 * mark.  Here a position is one unsigned char: bit 0x40 is the word mark,
 * bits 0x3F are the BCD character (B=0x20 A=0x10 8 4 2 1).  The check bit
 * is not kept; it is always correct in a simulated machine.
 *
 * External encodings map the 64 BCD characters to printable characters of
 * the C execution character set (ASCII on the PC, EBCDIC or ASCII on CMS,
 * depending on the runtime).  The tables are strings of character
 * literals, so the code never assumes ASCII.
 *
 *   A  Paul Pierce's primary encoding (the IBM "A" print chain), the
 *      default, and SimH's "new" conversions
 *   H  Paul Pierce's alternative (the "H" / Fortran chain)
 *   S  SimH "traditional" (old conversions), what ROPE uses
 *
 * The encodings are the ones Van Snyder's Autocoder knows by the same
 * letters, so decks are interchangeable with ROPE and SimH.
 */
#ifndef BCD_H
#define BCD_H

#define WM      0x40            /* word mark */
#define CHARM   0x3F            /* the six data bits */
#define ZONE    0x30            /* B and A bits */
#define DIGIT   0x0F            /* 8 4 2 1 */
#define ABIT    0x10
#define BBIT    0x20

/* BCD values of the characters the machine treats specially */
#define B_BLANK   000
#define B_ONE     001
#define B_TWO     002
#define B_THREE   003
#define B_FOUR    004
#define B_FIVE    005
#define B_SIX     006
#define B_SEVEN   007
#define B_EIGHT   010
#define B_NINE    011
#define B_ZERO    012
#define B_HASH    013           /* # (=) */
#define B_AT      014           /* @ (') */
#define B_TAPMRK  017           /* tape mark */
#define B_ALTBLK  020           /* cent / alternate blank (A bit only) */
#define B_SLASH   021
#define B_S       022
#define B_T       023
#define B_U       024
#define B_V       025
#define B_W       026
#define B_X       027
#define B_Y       030
#define B_Z       031
#define B_RECMRK  032           /* record mark */
#define B_COMMA   033
#define B_PERCNT  034           /* % ( ( ) */
#define B_WMCHR   035           /* word separator */
#define B_BSLASH  036
#define B_TAPSEG  037
#define B_MINUS   040
#define B_J       041
#define B_K       042
#define B_L       043
#define B_M       044
#define B_N       045
#define B_O       046
#define B_P       047
#define B_Q       050
#define B_R       051
#define B_MZERO   052           /* ! minus zero */
#define B_DOLLAR  053
#define B_ASTER   054
#define B_AMPER   060           /* & (+) */
#define B_A       061
#define B_B       062
#define B_C       063
#define B_D       064
#define B_E       065
#define B_F       066
#define B_G       067
#define B_H       070
#define B_I       071
#define B_PZERO   072           /* ? plus zero */
#define B_PERIOD  073
#define B_LOZENGE 074           /* square / lozenge */
#define B_GRPMRK  077

extern const char *bcd_enc_name;           /* "A", "H" or "S" */
extern char bcd2chr[64];                   /* BCD -> character */
extern unsigned char chr2bcd[256];         /* character -> BCD */

/* select an encoding by letter; returns 0, or -1 for an unknown letter */
int bcd_set_encoding(int letter);
/* the 63-character string of an encoding (BCD 1..63), for listings */
const char *bcd_encoding_string(int letter);

#define TOBCD(c)  (chr2bcd[(unsigned char)(c)])
#define TOCHR(b)  (bcd2chr[(b) & CHARM])

#endif
