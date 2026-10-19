/*
 * autocoder.c - IBM 1401 Autocoder assembler for VM/370plus
 *
 *   AUTOCODR source [( options [)]]
 *
 * Assembles 1401 Autocoder (the language of IBM's Tape Autocoder,
 * C24-3319, as the 1401 Reference Manual A24-1403-5 uses it) into a
 * self-loading object deck and a listing.  The deck is the card format
 * IBM's Autocoder punched: three loader cards (clear storage 1 and 2,
 * bootstrap), then load cards that each carry up to 39 characters in
 * columns 1-39, a load (LCA) in 40-46, set/clear word mark instructions
 * in 47-67 and a read-and-branch (1040) in 68-71, and an end card that
 * clears the read area and branches to the start address.  Decks are
 * column-for-column the same as those of Van Snyder's Autocoder with
 * "-b I", so they run on a real 1401, SimH, ROPE and IBM1401.
 *
 * Language: labels in columns 6-11 (6 characters), operation 16-20,
 * operands from 21 to the first blank outside a literal (to 72).
 *   declaratives  DCW DC DS DSA DA (with subfield lines) EQU
 *   control       ORG LTORG EX XFR END JOB CTL SFX CHAIN LC(0 only)
 *   instructions  every mnemonic of the 1401 and its special features
 *                 (A S M D ZA ZS MCW MLC LCA MCS MCE MN MZ MCM MIZ MA
 *                 SAR SBR SW CW CS B BCE BWZ BBE BIN BSS BE BH BL BU BAV
 *                 BC9 BCV BLC BEF BER BPB BPCB BM BW C H NOP R W P WR WP
 *                 RP WRP RCB PCB SRF SPF SS SSB CC CCB WM RT RTW RTB WT
 *                 WTW WTB WTM BSP SKP RWD RWU CU MU LU RD WD SD ...),
 *                 or an actual op code in column 19 with d in column 20
 *   operands      symbols, actual addresses, *, +/- adjustment, indexing
 *                 (&X1 &X2 &X3), character literals @...@, numeric
 *                 literals +n -n, address constants +SYM, area-defining
 *                 literals SYM#n, blank constants #n, devices %Un
 *
 * Options (CMS style after "(", or on the PC each as "-option"):
 *   DECK file | NODECK   object deck (default fn DECK A / name.cd)
 *   LIST file | NOLIST   listing (default fn LISTING A / name.lst)
 *   ENCODING A|H|S       character encoding (default A)
 *   BOOT I|N             IBM loader cards in front of the deck, or none
 *   SIZE n               core size the clear-storage cards clear (16000)
 *
 * Return codes: 0 clean, 4 warnings, 8 errors, 12 cannot open a file.
 *
 * Part of the 1401 emulator for VM/370plus (mikebeer/1401).  C89.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "bcd.h"

#define VERSION   "1.0"
#define MAXLINES  30000
#define MAXSYMS   8000
#define MAXLITS   4000
#define MAXOPS    8
#define REF       (-30000L)       /* symbol value: referenced, undefined */
#define X00       100000L

/* ------------------------------------------------------------ op table */
enum { OPT = -1, PRO = -2, REQ = -3 };

typedef struct {
    const char *op;
    int mop;            /* BCD machine op; 0 pseudo; -2 "repeat as previous" */
    int a;              /* OPT PRO REQ, or a device letter (BCD) for %x */
    int b;
    int d;              /* OPT PRO REQ, or a fixed d-character (BCD) */
} OpCode;

#define P_REPEAT  (-2)

static const OpCode optab[] = {
    { "DA",    0,        REQ, PRO, PRO },
    { "DC",    P_REPEAT, REQ, PRO, PRO },
    { "DCW",   P_REPEAT, REQ, PRO, PRO },
    { "DS",    P_REPEAT, REQ, PRO, PRO },
    { "DSA",   P_REPEAT, REQ, PRO, PRO },
    { "EQU",   P_REPEAT, REQ, PRO, PRO },
    /* arithmetic */
    { "A",     B_A,      OPT, OPT, PRO },
    { "D",     B_PERCNT, OPT, OPT, PRO },
    { "M",     B_AT,     OPT, OPT, PRO },
    { "S",     B_S,      OPT, OPT, PRO },
    { "ZA",    B_PZERO,  OPT, OPT, PRO },
    { "ZS",    B_MZERO,  OPT, OPT, PRO },
    /* data control */
    { "MBC",   B_M,      OPT, OPT, B_B },
    { "MBD",   B_M,      OPT, OPT, B_D },
    { "MCE",   B_E,      OPT, OPT, PRO },
    { "MCS",   B_Z,      OPT, OPT, PRO },
    { "MIZ",   B_X,      OPT, OPT, PRO },
    { "MLC",   B_M,      OPT, OPT, OPT },
    { "MCW",   B_M,      OPT, OPT, OPT },
    { "MLCWA", B_L,      OPT, OPT, OPT },
    { "LCA",   B_L,      OPT, OPT, OPT },
    { "MLNS",  B_D,      OPT, OPT, PRO },
    { "MN",    B_D,      OPT, OPT, PRO },
    { "MLZS",  B_Y,      OPT, OPT, PRO },
    { "MZ",    B_Y,      OPT, OPT, PRO },
    { "MRCM",  B_P,      OPT, OPT, OPT },
    { "MCM",   B_P,      OPT, OPT, OPT },
    /* logic */
    { "B",     B_B,      OPT, OPT, OPT },
    { "BAV",   B_B,      REQ, PRO, B_Z },
    { "BBE",   B_W,      REQ, REQ, REQ },
    { "BC9",   B_B,      REQ, PRO, B_NINE },
    { "BCV",   B_B,      REQ, PRO, B_AT },
    { "BE",    B_B,      REQ, PRO, B_S },
    { "BEF",   B_B,      REQ, PRO, B_K },
    { "BER",   B_B,      REQ, PRO, B_L },
    { "BH",    B_B,      REQ, PRO, B_U },
    { "BIN",   B_B,      REQ, PRO, REQ },
    { "BL",    B_B,      REQ, PRO, B_T },
    { "BLC",   B_B,      REQ, PRO, B_A },
    { "BM",    B_V,      REQ, REQ, B_K },
    { "BPCB",  B_B,      REQ, PRO, B_R },
    { "BPB",   B_B,      REQ, PRO, B_P },
    { "BU",    B_B,      REQ, PRO, B_SLASH },
    { "BW",    B_V,      OPT, REQ, B_ONE },
    { "BWZ",   B_V,      OPT, REQ, REQ },
    { "BCE",   B_B,      OPT, REQ, REQ },
    { "BSS",   B_B,      REQ, PRO, REQ },
    { "C",     B_C,      OPT, OPT, PRO },
    /* input / output */
    { "BSP",   B_U,      B_U, PRO, B_B },
    { "CU",    B_U,      B_U, PRO, REQ },
    { "DCR",   B_U,      B_F, PRO, B_D },
    { "ECR",   B_U,      B_F, PRO, B_E },
    { "LU",    B_L,      REQ, REQ, REQ },
    { "MU",    B_M,      REQ, REQ, REQ },
    { "P",     B_FOUR,   OPT, PRO, PRO },
    { "PCB",   B_FOUR,   OPT, PRO, B_C },
    { "R",     B_ONE,    OPT, PRO, PRO },
    { "RCB",   B_ONE,    OPT, PRO, B_C },
    { "RD",    B_M,      B_D, REQ, B_R },
    { "RDT",   B_M,      B_D, REQ, B_R },
    { "RDTW",  B_L,      B_D, REQ, B_R },
    { "RDW",   B_L,      B_D, REQ, B_R },
    { "RF",    B_FOUR,   REQ, PRO, B_R },
    { "RP",    B_FIVE,   OPT, PRO, PRO },
    { "RT",    B_M,      B_U, REQ, B_R },
    { "RTB",   B_M,      B_B, REQ, B_R },
    { "RTW",   B_L,      B_U, REQ, B_R },
    { "RWD",   B_U,      B_U, PRO, B_R },
    { "RWU",   B_U,      B_U, PRO, B_U },
    { "SD",    B_M,      B_D, REQ, B_R },
    { "SKP",   B_U,      B_U, PRO, B_E },
    { "SPF",   B_NINE,   OPT, PRO, PRO },
    { "SRF",   B_EIGHT,  OPT, PRO, PRO },
    { "W",     B_TWO,    OPT, PRO, PRO },
    { "WD",    B_M,      B_D, REQ, B_W },
    { "WDC",   B_M,      B_D, REQ, B_W },
    { "WDCW",  B_L,      B_D, REQ, B_W },
    { "WDT",   B_M,      B_D, REQ, B_W },
    { "WDTW",  B_L,      B_D, REQ, B_W },
    { "WDW",   B_L,      B_D, REQ, B_W },
    { "WM",    B_TWO,    OPT, PRO, B_LOZENGE },
    { "WP",    B_SIX,    OPT, PRO, PRO },
    { "WR",    B_THREE,  OPT, PRO, PRO },
    { "WRF",   B_SIX,    OPT, PRO, B_R },
    { "WRP",   B_SEVEN,  OPT, PRO, PRO },
    { "WT",    B_M,      B_U, REQ, B_W },
    { "WTB",   B_M,      B_B, REQ, B_W },
    { "WTM",   B_U,      B_U, PRO, B_M },
    { "WTW",   B_L,      B_U, REQ, B_W },
    /* miscellaneous */
    { "CC",    B_F,      PRO, PRO, OPT },
    { "CCB",   B_F,      REQ, PRO, OPT },
    { "CS",    B_SLASH,  OPT, OPT, PRO },
    { "CW",    B_LOZENGE,OPT, OPT, PRO },
    { "H",     B_PERIOD, OPT, OPT, PRO },
    { "MA",    B_HASH,   OPT, OPT, PRO },
    { "NOP",   B_N,      OPT, OPT, OPT },
    { "SAR",   B_Q,      OPT, PRO, PRO },
    { "SBR",   B_H,      OPT, OPT, PRO },
    { "SS",    B_K,      PRO, PRO, OPT },
    { "SSB",   B_K,      REQ, PRO, OPT },
    { "SW",    B_COMMA,  OPT, OPT, PRO },
    /* processor control */
    { "ALTER", 0, REQ, PRO, PRO }, { "CALL",  0, REQ, OPT, OPT },
    { "CHAIN", 0, REQ, PRO, PRO }, { "CTL",   0, REQ, PRO, PRO },
    { "DELET", 0, REQ, PRO, PRO }, { "END",   0, REQ, PRO, PRO },
    { "ENT",   0, REQ, PRO, PRO }, { "EX",    0, REQ, PRO, PRO },
    { "INCLD", 0, REQ, PRO, PRO }, { "INSER", 0, REQ, PRO, PRO },
    { "JOB",   0, REQ, PRO, PRO }, { "LC",    0, REQ, PRO, PRO },
    { "LTORG", 0, REQ, PRO, PRO }, { "ORG",   0, REQ, PRO, PRO },
    { "SFX",   0, REQ, PRO, PRO }, { "XFR",   0, REQ, PRO, PRO },
    { "PRINT", 0, REQ, PRO, PRO }, { "PUNCH", 0, REQ, PRO, PRO },
    { 0, 0, 0, 0, 0 }
};

static int find_op(const char *name)
{
    int i;
    for (i = 0; optab[i].op; i++)
        if (!strcmp(optab[i].op, name)) return i;
    return -1;
}

/* ----------------------------------------------------------- symbols */
typedef struct {
    char name[7];
    long val;           /* REF when undefined */
    long prev;          /* value in the previous pass */
    int  index;         /* 0..3 */
    char dev[4];        /* %xn if defined by EQU of a device */
    int  defined;       /* defined in this pass */
    int  referenced;
    int  defline;
} Sym;

static Sym syms[MAXSYMS];
static int nsyms;

static int sym_find(const char *name)
{
    int i;
    for (i = 0; i < nsyms; i++)
        if (!strcmp(syms[i].name, name)) return i;
    return -1;
}

static int sym_get(const char *name)
{
    int i = sym_find(name);
    if (i >= 0) return i;
    if (nsyms >= MAXSYMS) { fprintf(stderr, "AUTOCODR: too many symbols\n"); exit(12); }
    i = nsyms++;
    memset(&syms[i], 0, sizeof syms[i]);
    strncpy(syms[i].name, name, 6);
    syms[i].val = syms[i].prev = REF;
    return i;
}

/* the value to use now: this pass's definition, else the last pass's */
static long sym_value(int i)
{
    if (i < 0) return REF;
    if (syms[i].defined) return syms[i].val;
    return syms[i].prev;
}

/* ---------------------------------------------------------- literals */
enum { L_CHAR = 1, L_NUM, L_ADCON, L_AREA, L_ADCONLIT };

typedef struct {
    int  kind;
    int  width;
    unsigned char text[60];     /* BCD; for L_ADCON the symbol name */
    int  tlen;
    long addr;                  /* units position */
    int  done;
    long offset;                /* L_ADCON */
    int  index;
    int  sign;                  /* L_ADCON: '-' gives 16000 - address */
    int  sym;                   /* L_ADCON / L_AREA symbol */
    int  charlit;               /* L_ADCONLIT: the character literal */
    char key[80];               /* identity for sharing */
} Lit;

static Lit lits[MAXLITS];
static int nlits;

/* ---------------------------------------------------------- operands */
enum { K_NONE, K_ACTUAL, K_ADCONLIT, K_ADDRCON, K_AREADEF, K_ASTER,
       K_BLANKCON, K_CHARLIT, K_DAOPT, K_DEVICE, K_NUMLIT, K_OTHER,
       K_SYMBOLIC };

typedef struct {
    int  kind;
    long addr;          /* value, width, literal or symbol number */
    long offset;
    int  index;         /* -1 none, else 0..3 */
    int  d;             /* BCD of the first character, -1 if none */
    int  dlen;          /* length of the operand text */
    char label[7];      /* device text, or the symbol name */
    int  sign;          /* sign character of a literal (BCD) */
} Operand;

/* -------------------------------------------------------- statements */
typedef struct {
    char line[81];      /* the card image, blank padded */
    unsigned char bcd[81];
    int  lineno;
    int  op;            /* op table index, -1 none */
    int  direct;        /* op code in column 19 */
    char opname[6];
    int  why;           /* statement class, W_ */
    long loc;           /* location (high order) */
    int  width;
    int  nops;
    Operand ops[MAXOPS];
    int  d;             /* d-character (BCD) or -1 */
    int  litfirst, litlast; /* literal pool emitted here */
    int  sym;           /* label symbol */
    int  err;           /* error flags */
    char errtxt[80];
    int  warn;
    long daorg;         /* DA: start; subfield: record base */
    int  nrecs, recsiz, darm, dagm, daclr, daidx;
    long fstart, fend;  /* DA subfield */
} Stmt;

enum { W_NONE, W_COMMENT, W_INSTR, W_DCW, W_DC, W_DS, W_DSA, W_EQU, W_DA,
       W_FIELD, W_SBFLD, W_ORG, W_LTORG, W_END, W_EX, W_XFR, W_JOB, W_CTL,
       W_SFX, W_CHAIN, W_IGNORE, W_ERROR };

static Stmt *st;
static int nst;
static char sfx = ' ';
static int errors, warnings;
static long corsize = 16000;
static int boot = 'I';

static void serr(Stmt *s, const char *msg)
{
    if (!s->err) { strncpy(s->errtxt, msg, 79); s->errtxt[79] = 0; }
    s->err = 1;
}

/* ------------------------------------------------------------- lexer */
/* The lexer works on BCD codes, so it is independent of the encoding. */

enum { T_AT, T_CHARS, T_COMMA, T_DEVICE, T_DONE, T_HASH, T_MINUS, T_NAME,
       T_NUMBER, T_OTHER, T_PLUS, T_STAR };

static const unsigned char *L;   /* current statement in BCD, 1-based */
static int Lend;                 /* last column of the operand field */

static int is_letter(int b)
{
    return (b >= B_A && b <= B_I) || (b >= B_J && b <= B_R) ||
           (b >= B_S && b <= B_Z);
}
static int is_digit(int b) { return b >= B_ONE && b <= B_ZERO; }
static int dval(int b) { return (b & DIGIT) % 10; }

/* lex one token at *pos; sets *end */
static int lex(int *pos, int *end, int offset_mode)
{
    int p = *pos, b;
    if (p > Lend || L[p] == B_BLANK) { *end = p - 1; return T_DONE; }
    b = L[p];
    *end = p;
    if (b == B_AT) {
        /* a character literal: up to the next @ that is followed by a
           blank, a comma or the end of the field */
        int q;
        for (q = p + 1; q <= Lend; q++)
            if (L[q] == B_AT && (q == Lend || L[q + 1] == B_BLANK ||
                                 L[q + 1] == B_COMMA))
                break;
        if (q > Lend) return T_AT;
        *end = q;
        return q == p + 1 ? T_OTHER : T_CHARS;
    }
    if (b == B_COMMA) return T_COMMA;
    if (b == B_PERCNT) {
        if (p + 2 <= Lend && is_letter(L[p + 1]) && is_digit(L[p + 2])) {
            *end = p + 2;
            return T_DEVICE;
        }
        return T_OTHER;
    }
    if (b == B_HASH) return T_HASH;
    if (b == B_MINUS) return T_MINUS;
    if (b == B_AMPER) return T_PLUS;
    if (b == B_ASTER) return T_STAR;
    if (is_letter(b) || b == B_PERIOD || b == B_LOZENGE) {
        while (*end < Lend) {
            int c = L[*end + 1];
            if (c == B_COMMA || c == B_HASH || c == B_AMPER || c == B_MINUS ||
                c == B_BLANK || c == B_AT)
                break;
            (*end)++;
        }
        return T_NAME;
    }
    if (is_digit(b)) {
        if (offset_mode) {
            while (*end < Lend && (L[*end + 1] & DIGIT) >= 1 &&
                   (L[*end + 1] & DIGIT) <= 10)
                (*end)++;
        } else {
            while (*end < Lend && L[*end + 1] >= B_ONE && L[*end + 1] <= B_ZERO &&
                   (L[*end + 1] & ZONE) == 0)
                (*end)++;
            /* the low-order character may carry a sign zone */
            if (*end < Lend) {
                int c = L[*end + 1], dd = c & DIGIT;
                if (dd >= 1 && dd <= 10 && c != B_BLANK && (c & ZONE) &&
                    (*end + 2 > Lend || L[*end + 2] == B_COMMA ||
                     L[*end + 2] == B_BLANK))
                    (*end)++;
            }
        }
        return T_NUMBER;
    }
    return T_OTHER;
}

static void get_name(int s, int e, char *out)
{
    int i, n = 0;
    for (i = s; i <= e && n < 6; i++) out[n++] = TOCHR(L[i]);
    out[n] = 0;
    if (n == 5 && sfx != ' ') { out[5] = sfx; out[6] = 0; }
}

static long num_of(int s, int e)
{
    long v = 0;
    int i, neg;
    for (i = s; i < e; i++) v = v * 10 + dval(L[i]);
    v = v * 10 + dval(L[e]);
    neg = (L[e] & ZONE) == BBIT;
    return neg ? -v : v;
}

/* ---------------------------------------------------------- literals */
static int pass_no;

static int lit_create(int kind, int width, const char *key, int lc_dummy)
{
    int i, share = 52;
    (void)lc_dummy;
    if (kind == L_CHAR) share = 4;
    else if (kind == L_NUM) share = 6;
    if (width <= share) {
        for (i = 0; i < nlits; i++)
            if (!lits[i].done && lits[i].kind == kind && lits[i].width == width &&
                !strcmp(lits[i].key, key))
                return i;
    }
    if (nlits >= MAXLITS) { fprintf(stderr, "AUTOCODR: too many literals\n"); exit(12); }
    i = nlits++;
    memset(&lits[i], 0, sizeof lits[i]);
    lits[i].kind = kind;
    lits[i].width = width;
    lits[i].index = 0;
    strncpy(lits[i].key, key, 79);
    return i;
}

/* --------------------------------------------------------- the parser */
/* parse one operand at *pos into op; returns 0 more follow (comma),
   1 last, 2 nothing, -1 error */

static Stmt *cur;
static int do_lits;

static int adjust(int *pos, Operand *op, int org)
{
    int s, e, t;
    for (;;) {
        int neg = L[*pos] == B_MINUS;
        s = *pos + 1;
        t = lex(&s, &e, 1);
        if (t != T_NAME && t != T_NUMBER) { serr(cur, "Offset or index must be a number or X#"); return -1; }
        if (e - s == 1 && L[s] == B_X && (L[e] == B_ZERO || (L[e] >= B_ONE && L[e] <= B_THREE))) {
            if (op->index < 0) op->index = dval(L[e]);
            if (neg) serr(cur, "Index cannot be subtracted");
        } else if (e - s == 2 && L[s] == B_X && L[s + 1] == B_ZERO && L[e] == B_ZERO) {
            if (org) op->offset = X00;
        } else {
            long v = 0;
            int i;
            for (i = s; i <= e; i++) v = v * 10 + dval(L[i]);
            op->offset += neg ? -v : v;
        }
        *pos = e + 1;
        if (*pos > Lend) return 1;
        if (L[*pos] == B_COMMA) { (*pos)++; return 0; }
        if (L[*pos] == B_BLANK) return 1;
        if (L[*pos] == B_AMPER || L[*pos] == B_MINUS) continue;
        serr(cur, "Missing punctuation");
        return -1;
    }
}

/* after an operand: comma -> 0, end -> 1, else error */
static int finish(int *pos)
{
    if (*pos > Lend || L[*pos] == B_BLANK) return 1;
    if (L[*pos] == B_COMMA) { (*pos)++; return 0; }
    serr(cur, "Operand must end here");
    return -1;
}

static int parse(int *pos, Operand *op, int org)
{
    int s = *pos, e, t, r;
    char key[80];
    memset(op, 0, sizeof *op);
    op->index = -1;
    op->d = -1;
    t = lex(&s, &e, 0);
    if (t != T_DONE) { op->d = L[s]; op->dlen = e - s + 1; }
    switch (t) {
    case T_DONE:
        return 2;
    case T_AT:
        op->kind = K_OTHER;
        *pos = e + 1;
        return finish(pos) < 0 ? -1 : 1;
    case T_CHARS: {
        int w = e - s - 1, i;
        op->kind = K_CHARLIT;
        if (do_lits) {
            int k;
            key[0] = 'C';
            /* key: the BCD text as printable pairs */
            for (i = 0; i < w && i < 39; i++) {
                key[1 + 2 * i] = (char)('A' + (L[s + 1 + i] >> 3));
                key[2 + 2 * i] = (char)('A' + (L[s + 1 + i] & 7));
            }
            key[1 + 2 * (w < 39 ? w : 39)] = 0;
            k = lit_create(L_CHAR, w, key, 0);
            if (lits[k].tlen == 0) {
                for (i = 0; i < w && i < 60; i++) lits[k].text[i] = L[s + 1 + i];
                lits[k].tlen = w;
            }
            op->addr = k;
        } else
            op->addr = w;
        *pos = e + 1;
        return finish(pos);
    }
    case T_COMMA:
        /* a comma as the d-character: must be the last thing */
        op->kind = K_OTHER;
        *pos = e + 1;
        if (*pos > Lend || L[*pos] == B_BLANK) return 1;
        serr(cur, "Operand missing");
        return -1;
    case T_DEVICE:
        op->kind = K_DEVICE;
        op->label[0] = '%';
        op->label[1] = TOCHR(L[s + 1]);
        op->label[2] = TOCHR(L[s + 2]);
        op->label[3] = 0;
        *pos = e + 1;
        return finish(pos);
    case T_OTHER:
        op->kind = K_OTHER;
        *pos = e + 1;
        return finish(pos);
    case T_HASH:
        s = e + 1;
        t = lex(&s, &e, 0);
        if (t == T_NUMBER) {
            op->kind = K_BLANKCON;
            op->addr = num_of(s, e);
        } else if (t == T_DONE) {
            op->kind = K_OTHER;
        } else {
            serr(cur, "Area definitions cannot have offset or indexing");
            return -1;
        }
        *pos = e + 1;
        return finish(pos);
    case T_MINUS: case T_PLUS: {
        int sign = L[s];
        int b2 = e + 1;
        int s2 = b2, e2, t2;
        t2 = lex(&s2, &e2, 0);
        op->sign = sign;
        if (t2 == T_CHARS) {
            int w = e2 - s2 - 1, i, k, c;
            op->kind = K_ADCONLIT;
            if (do_lits) {
                for (i = 0; i < w && i < 39; i++) {
                    key[1 + 2 * i] = (char)('A' + (L[s2 + 1 + i] >> 3));
                    key[2 + 2 * i] = (char)('A' + (L[s2 + 1 + i] & 7));
                }
                key[0] = 'C';
                key[1 + 2 * (w < 39 ? w : 39)] = 0;
                c = lit_create(L_CHAR, w, key, 0);
                if (lits[c].tlen == 0) {
                    for (i = 0; i < w && i < 60; i++) lits[c].text[i] = L[s2 + 1 + i];
                    lits[c].tlen = w;
                }
                key[0] = 'P';
                k = lit_create(L_ADCONLIT, 3, key, 0);
                lits[k].charlit = c;
                op->addr = k;
            } else
                op->addr = w;
            *pos = e2 + 1;
            return finish(pos);
        }
        if (t2 == T_DONE) { op->kind = K_OTHER; *pos = e2 + 1; return 1; }
        if (t2 == T_NAME) {
            char nm[8];
            Operand tmp;
            if (e2 - s2 > 5) serr(cur, "Name too long");
            get_name(s2, e2, nm);
            memset(&tmp, 0, sizeof tmp);
            tmp.index = -1;
            *pos = e2 + 1;
            r = 1;
            if (*pos <= Lend && (L[*pos] == B_AMPER || L[*pos] == B_MINUS))
                r = adjust(pos, &tmp, 0);
            else
                r = finish(pos);
            op->kind = K_ADDRCON;
            strcpy(op->label, nm);
            if (do_lits) {
                int k;
                sprintf(key, "A%c%s%+ld%d", sign == B_MINUS ? '-' : '+', nm,
                        tmp.offset, tmp.index);
                k = lit_create(L_ADCON, 3, key, 0);
                lits[k].sym = sym_get(nm);
                syms[lits[k].sym].referenced = 1;
                lits[k].offset = tmp.offset;
                lits[k].index = tmp.index < 0 ? 0 : tmp.index;
                lits[k].sign = sign;
                op->addr = k;
            } else
                op->addr = 3;
            op->offset = tmp.offset;
            op->index = tmp.index;
            return r;
        }
        if (t2 == T_NUMBER) {
            int i, k, w = e2 - b2 + 1;
            op->kind = K_NUMLIT;
            if (do_lits) {
                key[0] = 'N';
                key[1] = sign == B_MINUS ? '-' : '+';
                for (i = 0; i < w && i < 70; i++) key[2 + i] = (char)('0' + dval(L[b2 + i]));
                key[2 + (w < 70 ? w : 70)] = 0;
                k = lit_create(L_NUM, w, key, 0);
                if (lits[k].tlen == 0) {
                    for (i = 0; i < w && i < 60; i++) lits[k].text[i] = L[b2 + i];
                    lits[k].tlen = w;
                    lits[k].sign = sign;
                }
                op->addr = k;
            } else
                op->addr = w;
            *pos = e2 + 1;
            return finish(pos);
        }
        serr(cur, "+ or - must be followed by a name or number");
        return -1;
    }
    case T_NAME: case T_NUMBER: case T_STAR: {
        int b0 = s;
        if (t == T_NAME) {
            if (e - s > 5) serr(cur, "Name too long");
            op->kind = K_SYMBOLIC;
            get_name(s, e, op->label);
        } else if (t == T_NUMBER) {
            op->kind = K_ACTUAL;
            op->addr = do_lits ? num_of(s, e) : (e - s + 1);
        } else
            op->kind = K_ASTER;
        *pos = e + 1;
        if (*pos > Lend || L[*pos] == B_BLANK) return 1;
        if (L[*pos] == B_COMMA) { (*pos)++; return 0; }
        if (L[*pos] == B_HASH) {
            int s3 = *pos + 1, e3;
            if (L[b0] == B_ASTER) { serr(cur, "Asterisk cannot be followed by #"); return -1; }
            if (lex(&s3, &e3, 0) != T_NUMBER) { serr(cur, "# must be followed by a number"); return -1; }
            if (do_lits) {
                int k, w = (int)num_of(s3, e3);
                sprintf(key, "R%s", op->label);
                k = lit_create(L_AREA, w, key, 0);
                lits[k].sym = sym_get(op->label);
                op->addr = k;
            }
            op->kind = K_AREADEF;
            *pos = e3 + 1;
            return finish(pos);
        }
        if (L[*pos] == B_AMPER || L[*pos] == B_MINUS)
            return adjust(pos, op, org);
        serr(cur, "Missing punctuation");
        return -1;
    }
    }
    return -1;
}

/* ------------------------------------------------- address characters */
static unsigned char zoned_bcd(int digit, int zone)
{
    unsigned char b = (unsigned char)(digit == 0 ? B_ZERO : digit);
    return (unsigned char)(b | (zone << 4));
}

/* 3 BCD characters of an address; negative becomes its absolute value */
static void num_to_addr(long num, int index, unsigned char *out)
{
    long n;
    int high;
    if (num < -15999) { out[0] = out[1] = out[2] = B_HASH; return; }
    n = (num < 0 ? -num : num) % 16000;
    high = (int)(n / 1000);
    out[2] = zoned_bcd((int)(n % 10), high / 4);
    out[1] = zoned_bcd((int)((n / 10) % 10), index & 3);
    out[0] = zoned_bcd((int)((n / 100) % 10), high % 4);
}

/* ======================================================= pass 1 */

static long p, pmax;

static void define(Stmt *s, const char *name, long val, int index)
{
    int i = sym_get(name);
    if (syms[i].defined) {
        if (pass_no == 1 || syms[i].defline != s->lineno) serr(s, "Duplicate label");
        return;
    }
    syms[i].defined = 1;
    syms[i].val = val;
    syms[i].index = index;
    syms[i].defline = s->lineno;
    s->sym = i;
}

static void ltorg(Stmt *s)
{
    int i;
    s->litfirst = -1;
    for (i = 0; i < nlits; i++) {
        Lit *l = &lits[i];
        if (l->done) continue;
        if (s->litfirst < 0) s->litfirst = i;
        s->litlast = i;
        l->done = 1;
        if (l->kind == L_ADCONLIT || l->kind == L_ADCON) {
            l->addr = p + 2;
            p += 3;
        } else {
            l->addr = p + l->width - 1;
            if (l->kind == L_AREA) define(s, syms[l->sym].name, l->addr, 0);
            p += l->width;
        }
    }
    if (p > pmax) pmax = p;
}

static int prev_op;

static void pass1_stmt(Stmt *s)
{
    int pos, r, i, lt, le;
    const OpCode *oc;
    char label[8];
    int has_label;

    s->err = 0; s->errtxt[0] = 0; s->sym = -1; s->litfirst = -1;
    s->width = 0; s->nops = 0; s->d = -1;
    s->loc = p;
    cur = s;
    L = s->bcd;
    Lend = 72;
    if (s->line[5] == '*') { s->why = W_COMMENT; return; }

    /* label, columns 6-11 */
    has_label = 0;
    label[0] = 0;
    {
        int k = 6, e;
        while (k <= 11 && L[k] == B_BLANK) k++;
        if (k <= 11) {
            e = k;
            while (e < 11 && L[e + 1] != B_BLANK) e++;
            get_name(k, e, label);
            has_label = 1;
            if (k != 6) s->warn = 1;
            for (i = 0; label[i]; i++)
                if (!isalnum((unsigned char)label[i])) { serr(s, "Invalid label"); break; }
        }
    }

    /* operation, columns 16-20 */
    {
        int n = 0;
        for (i = 15; i < 20; i++) if (s->line[i] != ' ') s->opname[n++] = (char)toupper((unsigned char)s->line[i]);
        s->opname[n] = 0;
    }
    s->direct = 0;
    if (s->line[15] == ' ' && s->line[16] == ' ' && s->line[17] == ' ' && s->line[18] != ' ') {
        s->direct = 1;
        s->op = -1;
    } else if (s->opname[0] == 0) {
        s->op = -1;
    } else {
        /* the op field is columns 16-20; embedded blanks end it */
        char nm[6];
        int n = 0;
        for (i = 15; i < 20 && s->line[i] != ' '; i++) nm[n++] = (char)toupper((unsigned char)s->line[i]);
        nm[n] = 0;
        s->op = find_op(nm);
        for (; i < 20; i++) if (s->line[i] != ' ') s->op = -1;   /* "R THE" */
        if (s->op < 0) { serr(s, "Unrecognized op code"); s->why = W_ERROR; return; }
    }
    pos = 21;
    if (L[21] == B_BLANK && L[22] != B_BLANK) pos = 22;    /* "CHAIN 2" */

    /* blank op: repeat the previous declarative, or a DA subfield */
    if (!s->direct && s->op < 0) {
        if (prev_op >= 0 && optab[prev_op].mop == P_REPEAT) s->op = prev_op;
        else if (prev_op >= 0 && !strcmp(optab[prev_op].op, "DA")) {
            Operand o1, o2;
            Stmt *da;
            /* find the DA */
            for (da = s - 1; da >= st && (da->why == W_FIELD || da->why == W_SBFLD || da->why == W_COMMENT); da--) ;
            do_lits = 1;
            r = parse(&pos, &o1, 0);
            if (r < 0 || o1.kind != K_ACTUAL) { serr(s, "Subfield needs a position"); s->why = W_ERROR; return; }
            s->why = W_SBFLD;
            s->fstart = o1.addr;
            s->fend = o1.addr;
            if (r == 0) {
                r = parse(&pos, &o2, 0);
                if (r < 0 || o2.kind != K_ACTUAL) { serr(s, "Subfield needs two positions"); return; }
                s->why = W_FIELD;
                s->fend = o2.addr;
            }
            s->daorg = da->loc;
            s->nrecs = da->nrecs; s->recsiz = da->recsiz;
            s->loc = da->loc + s->fend - 1;
            if (s->fend > da->recsiz || s->fstart > da->recsiz) serr(s, "Subfield outside the record");
            if (has_label) define(s, label, s->loc, da->daidx);
            return;
        } else { serr(s, "No op code"); s->why = W_ERROR; return; }
    }

    oc = s->op >= 0 ? &optab[s->op] : 0;
    do_lits = 1;

    if (oc && (oc->mop == 0 || oc->mop == P_REPEAT)) {
        const char *o = oc->op;
        prev_op = s->op;
        if (!strcmp(o, "DCW") || !strcmp(o, "DC")) {
            Operand *op = &s->ops[0];
            s->why = o[2] == 'W' ? W_DCW : W_DC;
            do_lits = 0;
            r = parse(&pos, op, 0);
            s->nops = 1;
            if (r != 1) { serr(s, "Bad DC/DCW operand"); s->width = 0; }
            switch (op->kind) {
            case K_ACTUAL: case K_NUMLIT: case K_CHARLIT: case K_BLANKCON:
                s->width = (int)op->addr;
                if (op->kind == K_BLANKCON) s->width = (int)op->addr;
                break;
            case K_SYMBOLIC: case K_ADDRCON:
                op->kind = K_ADDRCON;
                s->width = 3;
                break;
            case K_ADCONLIT: {
                /* +@...@: the character literal is created now */
                int w = (int)op->addr, k, i2, st0;
                char key[80];
                st0 = pos - 1;
                while (st0 > 21 && L[st0] != B_AT) st0--;
                /* st0 is the closing @; the text is w characters before */
                for (i2 = 0; i2 < w && i2 < 39; i2++) {
                    key[1 + 2 * i2] = (char)('A' + (L[st0 - w + i2] >> 3));
                    key[2 + 2 * i2] = (char)('A' + (L[st0 - w + i2] & 7));
                }
                key[0] = 'C';
                key[1 + 2 * (w < 39 ? w : 39)] = 0;
                k = lit_create(L_CHAR, w, key, 0);
                if (lits[k].tlen == 0) {
                    for (i2 = 0; i2 < w && i2 < 60; i2++) lits[k].text[i2] = L[st0 - w + i2];
                    lits[k].tlen = w;
                }
                op->addr = k;
                s->width = 3;
                break;
            }
            case K_ASTER:
                s->width = 3;
                break;
            case K_DEVICE:
                s->width = 3;
                break;
            default:
                serr(s, "Improper operand for DC/DCW");
            }
            if (s->width > 52) { serr(s, "DCW of more than 52 characters"); s->width = 52; }
            if (has_label) {
                if (isdigit((unsigned char)label[0])) {
                    s->loc = atol(label) - s->width + 1;
                    p -= s->width;
                } else
                    define(s, label, s->loc + s->width - 1, 0);
            }
            p += s->width;
        } else if (!strcmp(o, "DS")) {
            s->why = W_DS;
            if (L[pos] == B_HASH) pos++;
            {
                int s0 = pos, e0;
                if (lex(&s0, &e0, 0) != T_NUMBER) serr(s, "Operand of DS must be a number");
                else s->width = (int)num_of(s0, e0);
            }
            if (has_label) define(s, label, s->loc + s->width - 1, 0);
            p += s->width;
        } else if (!strcmp(o, "DSA")) {
            Operand *op = &s->ops[0];
            s->why = W_DSA;
            s->width = 3;
            r = parse(&pos, op, 0);
            s->nops = 1;
            if (op->kind == K_ADDRCON) {      /* +name / -name */
                if (op->sign == B_MINUS) serr(s, "DSA of a negative name");
                op->kind = K_SYMBOLIC;
            } else if (op->kind == K_NUMLIT) {
                op->kind = K_ACTUAL;
                op->addr = num_of(pos - (int)op->addr, pos - 1);
                if (op->sign == B_MINUS) op->addr = 16000 - op->addr;
            }
            if (op->kind != K_SYMBOLIC && op->kind != K_ACTUAL && op->kind != K_ASTER)
                serr(s, "Improper operand for DSA");
            if (op->kind == K_SYMBOLIC) syms[sym_get(op->label)].referenced = 1;
            if (has_label) {
                if (isdigit((unsigned char)label[0])) {
                    s->loc = atol(label) - 2;
                    p -= 3;
                } else
                    define(s, label, s->loc + 2, 0);
            }
            p += 3;
        } else if (!strcmp(o, "EQU")) {
            Operand *op = &s->ops[0];
            s->why = W_EQU;
            r = parse(&pos, op, 0);
            s->nops = 1;
            if (r != 1) serr(s, "Bad EQU operand");
            if (has_label) {
                int idx = op->index < 0 ? 0 : op->index;
                int si = sym_get(label);
                switch (op->kind) {
                case K_ACTUAL:
                    define(s, label, op->addr + op->offset, idx);
                    break;
                case K_ASTER:
                    define(s, label, p - 1 + op->offset, idx);
                    break;
                case K_DEVICE:
                    define(s, label, 0, 0);
                    strcpy(syms[si].dev, op->label);
                    break;
                case K_SYMBOLIC: {
                    int ti = sym_get(op->label);
                    long v = sym_value(ti);
                    syms[ti].referenced = 1;
                    if (op->index < 0) idx = syms[ti].index;
                    if (syms[ti].dev[0]) {
                        define(s, label, 0, 0);
                        strcpy(syms[si].dev, syms[ti].dev);
                    } else if (v > REF)
                        define(s, label, v + op->offset, idx);
                    else {
                        syms[si].defined = 1;   /* resolved next pass */
                        syms[si].val = REF;
                        syms[si].defline = s->lineno;
                        s->sym = si;
                    }
                    break;
                }
                default:
                    serr(s, "Improper EQU operand");
                }
            }
        } else if (!strcmp(o, "DA")) {
            int s0 = pos, e0, ok = 1;
            s->why = W_DA;
            s->daclr = s->dagm = s->darm = 0; s->daidx = 0;
            s->nrecs = s->recsiz = 1;
            if (lex(&s0, &e0, 1) != T_NUMBER) ok = 0;
            else {
                /* nXm: the X ends the first number */
                int k = s0;
                long n1 = 0;
                while (k <= Lend && L[k] >= B_ONE && L[k] <= B_ZERO && (L[k] & ZONE) == 0) n1 = n1 * 10 + dval(L[k++]);
                if (L[k] != B_X) ok = 0;
                else {
                    long n2 = 0;
                    k++;
                    if (!(L[k] >= B_ONE && L[k] <= B_ZERO)) ok = 0;
                    while (k <= Lend && L[k] >= B_ONE && L[k] <= B_ZERO && (L[k] & ZONE) == 0) n2 = n2 * 10 + dval(L[k++]);
                    s->nrecs = (int)n1; s->recsiz = (int)n2;
                    while (ok && L[k] == B_COMMA) {
                        k++;
                        if (L[k] == B_RECMRK) { s->darm = 1; k++; }
                        else if (L[k] == B_C && (L[k + 1] == B_COMMA || L[k + 1] == B_BLANK)) { s->daclr = 1; k++; }
                        else if (L[k] == B_G && (L[k + 1] == B_COMMA || L[k + 1] == B_BLANK)) { s->dagm = 1; k++; }
                        else if (L[k] == B_X && L[k + 1] >= B_ONE && L[k + 1] <= B_THREE) { s->daidx = L[k + 1]; k += 2; }
                        else if (L[k] == B_X && L[k + 1] == B_ZERO) { s->daidx = 0; k += 2; }
                        else ok = 0;
                    }
                    if (L[k] != B_BLANK && k <= Lend) ok = 0;
                }
            }
            if (!ok) { serr(s, "DA needs nXm"); s->nrecs = s->recsiz = 1; }
            s->width = s->nrecs * s->recsiz + (s->darm ? s->nrecs : 0) + (s->dagm ? 1 : 0);
            if (s->darm) s->recsiz++;
            if (has_label && isdigit((unsigned char)label[0])) s->loc = atol(label);
            else if (has_label) define(s, label, s->loc, s->daidx);
            s->daorg = s->loc;
            if (!(has_label && isdigit((unsigned char)label[0]))) p += s->width;
        } else if (!strcmp(o, "ORG") || !strcmp(o, "LTORG")) {
            Operand *op = &s->ops[0];
            s->why = o[0] == 'L' ? W_LTORG : W_ORG;
            if (has_label) define(s, label, p, 0);
            r = parse(&pos, op, 1);
            s->nops = 1;
            if (op->index >= 0) serr(s, "Indexing not permitted");
            if (s->line[20] == ' ' && s->line[21] == ' ') { op->kind = K_ACTUAL; op->addr = pmax; }
            switch (op->kind) {
            case K_ACTUAL: p = op->addr; break;
            case K_ASTER: break;
            case K_SYMBOLIC: {
                long v = sym_value(sym_get(op->label));
                syms[sym_get(op->label)].referenced = 1;
                if (v <= REF) { if (pass_no > 2) serr(s, "Undefined ORG"); p = 0; }
                else p = v;
                break;
            }
            default: serr(s, "Improper operand form");
            }
            if (op->offset >= X00) { p += 99; p = p - p % 100 + op->offset - X00; }
            else p += op->offset;
            s->loc = p;
            if (s->why == W_LTORG) ltorg(s);
        } else if (!strcmp(o, "END") || !strcmp(o, "EX") || !strcmp(o, "XFR")) {
            Operand *op = &s->ops[0];
            s->why = o[0] == 'E' ? (o[1] == 'N' ? W_END : W_EX) : W_XFR;
            if (s->why != W_XFR) { ltorg(s); }
            s->loc = p;
            r = parse(&pos, op, 0);
            s->nops = r == 2 ? 0 : 1;
            if (op->index >= 0) serr(s, "Improper operand");
            if (op->kind == K_SYMBOLIC) syms[sym_get(op->label)].referenced = 1;
        } else if (!strcmp(o, "JOB")) {
            s->why = W_JOB;
        } else if (!strcmp(o, "CTL")) {
            s->why = W_CTL;
        } else if (!strcmp(o, "SFX")) {
            s->why = W_SFX;
            sfx = s->line[20];
        } else if (!strcmp(o, "CHAIN")) {
            int s0 = pos, e0;
            s->why = W_CHAIN;
            if (lex(&s0, &e0, 0) != T_NUMBER) serr(s, "Operand of CHAIN is not a number");
            else s->nrecs = (int)num_of(s0, e0);
            if (s->nrecs < 1) s->nrecs = 1;
            /* the previous machine op code n more times, chained */
            s->width = s->nrecs;
            {
                Stmt *pv;
                for (pv = s - 1; pv >= st && pv->why != W_INSTR; pv--) ;
                s->d = pv >= st ? (pv->direct ? pv->bcd[19] : optab[pv->op].mop) : B_N;
            }
            p += s->width;
        } else if (!strcmp(o, "LC")) {
            s->why = W_IGNORE;
        } else {
            s->why = W_IGNORE;      /* ALTER CALL DELET INSER INCLD PRINT PUNCH ENT */
        }
        if (p > pmax) pmax = p;
        return;
    }

    /* ---------------- a machine instruction ---------------- */
    s->why = W_INSTR;
    prev_op = s->op;
    if (has_label) define(s, label, p, 0);
    s->width = 1;
    for (i = 0; i < MAXOPS; i++) {
        r = parse(&pos, &s->ops[i], 0);
        if (r == 2) {
            /* a trailing comma: the d-character is a blank */
            if (i > 0) {
                memset(&s->ops[i], 0, sizeof s->ops[i]);
                s->ops[i].kind = K_OTHER;
                s->ops[i].d = B_BLANK;
                s->ops[i].dlen = 1;
                s->ops[i].index = -1;
                s->nops = i + 1;
            }
            break;
        }
        if (r < 0 || s->err) break;     /* the bad operand is dropped */
        s->nops = i + 1;
        if (r != 0) break;
    }
    /* a trailing comma made an empty operand: drop "done" ones */
    {
        int got_d = 0, mop, d;
        int A, B, D;
        if (s->direct) {
            mop = s->bcd[19];
            d = s->line[19] != ' ' ? s->bcd[20] : -1;
            got_d = d >= 0;
            A = OPT; B = OPT; D = got_d ? PRO : OPT;
        } else {
            mop = oc->mop;
            A = oc->a; B = oc->b; D = oc->d;
            d = (D >= 0) ? D : -1;
            got_d = D >= 0;
        }
        s->d = d;
        lt = le = 0;
        (void)lt; (void)le;
        if (s->nops == 0) {
            if (A == REQ) serr(s, "A field required");
            if (A != OPT && D == REQ && !s->direct) serr(s, "D field required");
        } else {
            Operand *o1 = &s->ops[0];
            if (A == PRO) {
                if (s->nops > 1) serr(s, "A field not allowed");
                else if (D == OPT || D == REQ) {
                    if (o1->dlen != 1) serr(s, "D modifier must be one character");
                    else { s->d = o1->d; got_d = 1; }
                } else if (o1->kind != K_OTHER) serr(s, "A field not allowed");
                s->nops = 0;
                goto done_ops;
            } else if (o1->kind == K_OTHER) {
                s->nops = 0;
                goto done_ops;
            } else {
                s->width += 3;
                if (o1->kind == K_SYMBOLIC) syms[sym_get(o1->label)].referenced = 1;
            }
            if (s->nops == 1) {
                if (B == REQ) serr(s, "B field required");
                if (D == REQ && !got_d) serr(s, "D field required");
            } else {
                Operand *o2 = &s->ops[1];
                if (B == PRO) {
                    if (D == OPT || D == REQ) {
                        if (o2->dlen != 1) serr(s, "D modifier must be one character");
                        else { s->d = o2->d; got_d = 1; }
                    } else serr(s, "B field not allowed");
                    s->nops = 1;
                } else if (mop == B_B && !got_d && !s->direct && s->nops == 2) {
                    if (o2->dlen != 1) serr(s, "D modifier must be one character");
                    else { s->d = o2->d; got_d = 1; }
                    s->nops = 1;
                } else if (o2->kind == K_OTHER) {
                    serr(s, "Bad B field");
                } else {
                    s->width += 3;
                    if (o2->kind == K_SYMBOLIC) syms[sym_get(o2->label)].referenced = 1;
                }
                if (s->nops == 2) {
                    if (D == REQ && !got_d) serr(s, "D field required");
                } else if (s->nops >= 3) {
                    if (D == PRO || got_d) serr(s, "D field not allowed");
                    else if (s->ops[2].dlen != 1) serr(s, "D modifier must be one character");
                    else { s->d = s->ops[2].d; got_d = 1; }
                    if (s->nops > 3) serr(s, "Too many operands");
                    s->nops = 2;
                }
            }
        }
done_ops:
        if (got_d && s->d >= 0) s->width++;
        else s->d = -1;
    }
    p += s->width;
    if (p > pmax) pmax = p;
}

/* ============================================================ pass 2 */

/* ---- the object deck: IBM 1401 Autocoder load card format ---- */
static FILE *deckf, *listf;
static unsigned char card[81];          /* BCD, columns 1-80 */
static int card_no = 1, obj_pos, num_load_ops;
static long last_obj_p = -1;
static char deck_id[6];

/* IBM's loader cards (1401-AU-037), written here in encoding A */
static const char cs1_16[] =
    ",008015,022026,030037,044,049,053053N000000N00001026                   ";
static const char cs2_16[] =
    "L068116,105106,110117B101/I9I#071029C029056B026/B001/0991,001/001117I0?";
static const char cs1_4[] =
    ",008015,019026,030,034041,045,053,0570571026                           ";
static const char cs2_4[] =
    "L068112,102106,113/101099/I99,027A070028)027B0010270B0261,001/001113I0 ";
static const char bootstrap[] =
    ",008015,022029,036040,047054,061068,072/061039              ,0010011040";
static const char ex1[] =
    ",015022)024056,029036,040047,0540611001,001008B001     ,001008B001     ";
static const char ex2[] =
    ",068072)063067/061039                                       ,0010011040";

static unsigned char a2b(int c)
{
    /* the loader card strings are in encoding A */
    static const char enc_a[] =
        " 1234567890#@:>{^/STUVWXYZ|,%~\\\"-JKLMNOPQR!$*];_&ABCDEFGHI?.)[<}";
    const char *q = strchr(enc_a, c);
    return (unsigned char)(q ? q - enc_a : 0);
}

static void set_card_text(int col, const char *txt)
{
    int i;
    for (i = 0; txt[i] && col + i <= 80; i++) card[col + i] = a2b(txt[i]);
}

static void card_reset(void)
{
    int i;
    for (i = 1; i <= 80; i++) card[i] = B_BLANK;
    set_card_text(40, "L001001,040040,040040,040040");
}

static void put_addr(int col, long a)
{
    num_to_addr(a, 0, &card[col]);
}

static void write_card(void)
{
    int i;
    if (deckf) {
        for (i = 1; i <= 71; i++) fputc(TOCHR(card[i]), deckf);
        fprintf(deckf, "%04d%s\n", card_no % 10000, deck_id);
    }
    card_no++;
}

static void finish_obj(const char *touch)
{
    if (obj_pos > 0) {
        char t[16];
        card[40] = B_L;
        sprintf(t, "%03d", obj_pos);
        set_card_text(41, t);
        put_addr(44, last_obj_p);
    }
    if (touch) set_card_text(68, touch);
    write_card();
    card_reset();
    obj_pos = 0;
    num_load_ops = 0;
}

static void sw(long at)
{
    int j = 3 * num_load_ops + num_load_ops / 2 + 48;
    put_addr(j, at);
    num_load_ops++;
}

/* load 'what' (len n, BCD) ending at 'where'; wm: set a word mark at the
   high-order end; clear>0: load 'clear' blanks instead */
static void dc(const unsigned char *what, int n, int wm, long where, int clear)
{
    int maxcard = clear ? 38 : 39, remain = clear ? clear : n, i = 0, l, k;
    long myp = where - remain + 1;
    int mywm = wm;
    if (!deckf) return;
    if ((obj_pos > 0 && last_obj_p + 1 != myp) ||
        (obj_pos + remain > maxcard && !clear) ||
        (obj_pos == 0 && num_load_ops > 0 && !mywm))
        finish_obj("1040");
    if (num_load_ops >= 6 && mywm) finish_obj("1040");
    while (remain > 0) {
        l = remain < maxcard - obj_pos ? remain : maxcard - obj_pos;
        for (k = obj_pos + 1; k <= maxcard; k++) card[k] = B_BLANK;
        if (!clear) for (k = 0; k < l; k++) card[obj_pos + 1 + k] = what[i + k];
        if (obj_pos == 0) {
            if (!mywm) {
                card[47] = B_LOZENGE;          /* ) clear word mark */
                put_addr(48, myp);
                put_addr(51, myp);
                num_load_ops = 2;
            }
        } else if (mywm) {
            if (num_load_ops >= 6) finish_obj("1040");
            sw(myp);
        }
        i += l;
        myp += l;
        last_obj_p = myp - 1;
        mywm = 0;
        obj_pos += l;
        if (obj_pos == maxcard) finish_obj("1040");
        remain -= l;
    }
}

/* ---- the listing ---- */
static int list_lines;

static void list_text(Stmt *s, const char *ct, long loc, const unsigned char *obj, int on, int show_card)
{
    char out[160], objtxt[40];
    int i, k = 0;
    if (!listf) return;
    for (i = 0; i < on && k < 30; i++) {
        objtxt[k++] = TOCHR(obj[i]);
        if (on >= 4 && (i == 0 || (i == 3 && on >= 7) || (i == 6 && on == 8))) objtxt[k++] = ' ';
    }
    objtxt[k] = 0;
    sprintf(out, "%05d  %-66.66s %3s", s ? s->lineno : 0, s ? s->line + 5 : "", ct);
    if (loc >= 0) sprintf(out + strlen(out), " %05ld", loc);
    else strcat(out, "      ");
    sprintf(out + strlen(out), "  %-14s", objtxt);
    if (show_card) sprintf(out + strlen(out), " %4d", obj_pos == 0 ? card_no - 1 : card_no);
    for (i = (int)strlen(out); i > 0 && out[i - 1] == ' '; i--) out[i - 1] = 0;
    fprintf(listf, "%s\n", out);
    list_lines++;
}

static void list_err(Stmt *s)
{
    if (s->err) {
        errors++;
        if (listf) fprintf(listf, "*** ERROR line %d: %s\n", s->lineno, s->errtxt);
        fprintf(stderr, "AUTOCODR: line %d: %s\n", s->lineno, s->errtxt);
    }
}

/* the address of an operand */
static long op_addr(Stmt *s, Operand *o, int *index, int width, char *dev)
{
    long a = 0;
    *index = 0;
    if (dev) dev[0] = 0;
    switch (o->kind) {
    case K_ACTUAL: a = o->addr; break;
    case K_ASTER:  a = s->loc + width - 1; break;
    case K_ADCONLIT: case K_ADDRCON: case K_AREADEF: case K_CHARLIT: case K_NUMLIT:
        a = lits[o->addr].addr; break;
    case K_SYMBOLIC: {
        int i = sym_find(o->label);
        if (i < 0 || sym_value(i) <= REF) {
            /* X1..X3 name the index registers when not defined */
            if (!strcmp(o->label, "X1")) a = 89;
            else if (!strcmp(o->label, "X2")) a = 94;
            else if (!strcmp(o->label, "X3")) a = 99;
            else { serr(s, "Undefined symbol"); return -99999L; }
        } else {
            a = sym_value(i);
            *index = syms[i].index;
            if (syms[i].dev[0] && dev) strcpy(dev, syms[i].dev);
        }
        break;
    }
    case K_DEVICE:
        if (dev) strcpy(dev, o->label);
        break;
    }
    if (o->index >= 0) *index = o->index;
    return a + o->offset;
}

static void emit_lits(Stmt *s)
{
    int i;
    unsigned char buf[64];
    if (s->litfirst < 0) return;
    for (i = s->litfirst; i <= s->litlast; i++) {
        Lit *l = &lits[i];
        int w = l->width, k;
        if (l->kind == L_CHAR) {
            memcpy(buf, l->text, (size_t)w);
        } else if (l->kind == L_NUM) {
            for (k = 0; k < w; k++) buf[k] = l->text[k];
            buf[w - 1] = zoned_bcd(dval(buf[w - 1]), l->sign == B_MINUS ? 2 : 3);
        } else if (l->kind == L_AREA) {
            for (k = 0; k < w; k++) buf[k] = B_BLANK;
        } else if (l->kind == L_ADCON) {
            long v = sym_value(l->sym);
            if (v <= REF) { serr(s, "Undefined symbol in address constant"); v = 0; }
            v += l->offset;
            if (l->sign == B_MINUS) v = 16000 - v;
            num_to_addr(v, l->index, buf);
            w = 3;
        } else {                        /* L_ADCONLIT */
            num_to_addr(lits[l->charlit].addr, 0, buf);
            w = 3;
        }
        dc(buf, w, 1, l->addr, 0);
        if (listf) {
            char ct[16];
            sprintf(ct, "%d", w);
            fprintf(listf, "%05d  %-66.66s %3s %05ld  ", 0, "  (literal)", ct, l->addr - w + 1);
            for (k = 0; k < w; k++) fputc(TOCHR(buf[k]), listf);
            fprintf(listf, "\n");
        }
    }
}

static void pass2(void)
{
    int i, k;
    int need_cs = boot != 'N', need_ex = 0;
    unsigned char obj[64];

    card_reset();
    for (i = 0; i < nst; i++) {
        Stmt *s = &st[i];
        cur = s;
        L = s->bcd;
        if (s->why == W_JOB && s->line[5] != '*') {
            if (obj_pos > 0) finish_obj("1040");
            for (k = 0; k < 5; k++) deck_id[k] = s->line[75 + k];
            deck_id[5] = 0;
            for (k = 4; k >= 0 && deck_id[k] == ' '; k--) deck_id[k] = 0;
        }
        if (need_cs) {
            if (corsize <= 4000) {
                memcpy(card + 1, "", 0);
                set_card_text(1, cs1_4); write_card(); card_reset();
                set_card_text(1, cs2_4); put_addr(27, corsize - 1); write_card(); card_reset();
            } else {
                set_card_text(1, cs1_16); write_card(); card_reset();
                set_card_text(1, cs2_16); put_addr(27, corsize - 1); write_card(); card_reset();
            }
            set_card_text(1, bootstrap); write_card(); card_reset();
            need_cs = 0;
        }
        if (need_ex && boot != 'N') {
            set_card_text(1, ex1); write_card(); card_reset();
            set_card_text(1, ex2); write_card(); card_reset();
            need_ex = 0;
        }
        switch (s->why) {
        case W_COMMENT: case W_JOB: case W_CTL: case W_SFX: case W_IGNORE:
        case W_EQU: case W_SBFLD:
            list_text(s, "", s->why == W_EQU && s->sym >= 0 ? sym_value(s->sym) : -1, 0, 0, 0);
            break;
        case W_ERROR:
            list_text(s, "", -1, 0, 0, 0);
            break;
        case W_DS:
            list_text(s, "", s->loc, 0, 0, 0);
            break;
        case W_ORG:
            list_text(s, "", s->loc, 0, 0, 0);
            break;
        case W_LTORG:
            list_text(s, "", s->loc, 0, 0, 0);
            emit_lits(s);
            break;
        case W_DCW: case W_DC: {
            Operand *o = &s->ops[0];
            int w = s->width, idx;
            long a;
            char ct[16];
            if (w <= 0) { list_text(s, "", -1, 0, 0, 0); break; }
            switch (o->kind) {
            case K_ACTUAL: case K_NUMLIT: {
                /* the characters as written; a sign zones the units digit */
                int st0 = 21;
                if (L[21] == B_AMPER || L[21] == B_MINUS) st0 = 22;
                for (k = 0; k < w; k++) obj[k] = L[st0 + k];
                if (st0 == 22) obj[w - 1] = zoned_bcd(dval(obj[w - 1]), L[21] == B_MINUS ? 2 : 3);
                break;
            }
            case K_CHARLIT: {
                int q = 21;
                while (q < 72 && L[q] != B_AT) q++;
                for (k = 0; k < w; k++) obj[k] = L[q + 1 + k];
                break;
            }
            case K_BLANKCON:
                for (k = 0; k < w; k++) obj[k] = B_BLANK;
                break;
            case K_ADDRCON: {
                int si = sym_find(o->label);
                long v;
                if (si < 0 || sym_value(si) <= REF) { serr(s, "Undefined symbol"); v = 0; idx = 0; }
                else { v = sym_value(si) + o->offset; idx = syms[si].index; }
                if (o->index >= 0) idx = o->index;
                if (o->sign == B_MINUS) v = 16000 - v;
                num_to_addr(v, idx, obj);
                break;
            }
            case K_ADCONLIT:
                num_to_addr(lits[o->addr].addr, 0, obj);
                break;
            case K_ASTER:
                a = s->loc + 2 + o->offset;
                num_to_addr(a, o->index < 0 ? 0 : o->index, obj);
                break;
            case K_DEVICE:
                for (k = 0; k < 3; k++) obj[k] = TOBCD(o->label[k]);
                break;
            default:
                for (k = 0; k < w; k++) obj[k] = B_BLANK;
            }
            dc(obj, w, s->why == W_DCW, s->loc + w - 1, 0);
            sprintf(ct, "%d", w);
            list_text(s, ct, s->loc + w - 1, obj, w > 14 ? 14 : w, 1);
            break;
        }
        case W_DSA: {
            Operand *o = &s->ops[0];
            int idx;
            long a = op_addr(s, o, &idx, 3, 0);
            if (o->kind == K_ASTER) a = s->loc + 2 + o->offset;
            num_to_addr(a, idx, obj);
            dc(obj, 3, 1, s->loc + 2, 0);
            list_text(s, "3", s->loc + 2, obj, 3, 1);
            break;
        }
        case W_DA: {
            long pda = s->loc, pp;
            int r, rs = s->recsiz;
            list_text(s, "", pda, 0, 0, 0);
            if (num_load_ops >= 6) finish_obj("1040");
            if (s->daclr) dc(0, 0, 0, pda + (long)rs * s->nrecs - 1, rs * s->nrecs);
            pp = pda;
            for (r = 0; r < s->nrecs; r++) {
                if (num_load_ops >= 6) finish_obj("1040");
                sw(pp);
                pp += rs;
            }
            pp = pda;
            for (r = 0; r < s->nrecs; r++) {
                pp += rs;
                if (s->darm) {
                    unsigned char rm = B_RECMRK;
                    dc(&rm, 1, 0, pp - 1, 0);
                }
            }
            if (s->dagm) {
                unsigned char gm = B_GRPMRK;
                dc(&gm, 1, 1, pp, 0);
            }
            break;
        }
        case W_FIELD: {
            long org = s->daorg + s->fstart - 1;
            int r;
            for (r = 0; r < s->nrecs; r++) {
                if (num_load_ops >= 6) finish_obj("1040");
                sw(org);
                org += s->recsiz;
            }
            list_text(s, "", s->loc, 0, 0, 0);
            break;
        }
        case W_CHAIN: {
            int n;
            unsigned char op1 = (unsigned char)s->d;
            for (n = 0; n < s->width; n++) dc(&op1, 1, 1, s->loc + n, 0);
            list_text(s, "", s->loc, 0, 0, 0);
            break;
        }
        case W_INSTR: {
            int w = s->width, idx, err0 = s->err;
            char dev[4];
            long a;
            obj[0] = (unsigned char)(s->direct ? s->bcd[19] : optab[s->op].mop);
            if (w >= 4) {
                a = op_addr(s, &s->ops[0], &idx, w, dev);
                if (dev[0]) for (k = 0; k < 3; k++) obj[1 + k] = TOBCD(dev[k]);
                else num_to_addr(a, idx, obj + 1);
                if (!s->direct && optab[s->op].a >= 0) {
                    obj[1] = B_PERCNT;
                    obj[2] = (unsigned char)optab[s->op].a;
                }
            }
            if (w >= 7) {
                a = op_addr(s, &s->ops[1], &idx, w, dev);
                if (dev[0]) for (k = 0; k < 3; k++) obj[4 + k] = TOBCD(dev[k]);
                else num_to_addr(a, idx, obj + 4);
            }
            if (w == 2 || w == 5 || w == 8) obj[w - 1] = (unsigned char)s->d;
            if (err0) for (k = 0; k < w; k++) obj[k] = B_HASH;
            dc(obj, w, 1, s->loc + w - 1, 0);
            {
                char ct[16];
                sprintf(ct, "%d", w);
                list_text(s, ct, s->loc, obj, w, 1);
            }
            break;
        }
        case W_EX: case W_XFR: {
            int idx;
            long a = 0;
            if (s->why == W_EX) emit_lits(s);
            if (s->nops) a = op_addr(s, &s->ops[0], &idx, 0, 0);
            finish_obj("1040");
            set_card_text(40, "N000000");
            card[68] = B_B;
            put_addr(69, a);
            write_card();
            card_reset();
            obj_pos = 0; num_load_ops = 0;
            need_ex = 1;
            list_text(s, "", a, 0, 0, 0);
            break;
        }
        case W_END: {
            int idx;
            long a = 0;
            emit_lits(s);
            if (s->nops) a = op_addr(s, &s->ops[0], &idx, 0, 0);
            if (obj_pos > 0 || num_load_ops > 0) finish_obj("1040");
            for (k = 40; k <= 71; k++) card[k] = B_BLANK;
            card[40] = B_SLASH;
            put_addr(41, a);
            set_card_text(44, "080");
            write_card();
            card_reset();
            list_text(s, "", a, 0, 0, 0);
            list_err(s);
            return;
        }
        }
        list_err(s);
    }
    /* no END card */
    if (obj_pos > 0) finish_obj("1040");
}

/* ============================================================ driver */

static int is_fm(const char *s)
{
    return isalpha((unsigned char)s[0]) && (!s[1] || (isdigit((unsigned char)s[1]) && !s[2]));
}

static int ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

static char *take_file(int argc, char **argv, int *i)
{
    static char bufs[8][64];
    static int nb;
    char *b;
    if (*i >= argc) return 0;
#ifdef __CMS__
    b = bufs[nb++ & 7];
    if (strchr(argv[*i], '.') || *i + 1 >= argc || argv[*i + 1][0] == '(') {
        strncpy(b, argv[(*i)++], 63); b[63] = 0;
        return b;
    }
    sprintf(b, "%.8s %.8s", argv[*i], argv[*i + 1]);
    *i += 2;
    if (*i < argc && is_fm(argv[*i])) { strcat(b, " "); strcat(b, argv[(*i)++]); }
    return b;
#else
    (void)bufs; (void)nb; (void)b; (void)is_fm;
    return argv[(*i)++];
#endif
}

static void symtab_list(void)
{
    int i, j, n = 0, *ix;
    if (!listf) return;
    ix = (int *)malloc(sizeof(int) * (nsyms + 1));
    if (!ix) return;
    for (i = 0; i < nsyms; i++) if (syms[i].defined || syms[i].referenced) ix[n++] = i;
    for (i = 1; i < n; i++) {
        int t = ix[i];
        for (j = i; j > 0 && strcmp(syms[ix[j - 1]].name, syms[t].name) > 0; j--) ix[j] = ix[j - 1];
        ix[j] = t;
    }
    fprintf(listf, "\nSYMBOL TABLE\n\n");
    for (i = 0; i < n; i++) {
        Sym *y = &syms[ix[i]];
        if (y->dev[0]) fprintf(listf, "%-6s   %3s ", y->name, y->dev);
        else if (y->val <= REF) fprintf(listf, "%-6s UNDEF ", y->name);
        else fprintf(listf, "%-6s %05ld%s", y->name, y->val, y->index ? (y->index == 1 ? "+X1" : y->index == 2 ? "+X2" : "+X3") : "   ");
        fprintf(listf, (i % 6 == 5) ? "\n" : "    ");
    }
    fprintf(listf, "\n");
    free(ix);
}

int main(int argc, char **argv)
{
    char *src = 0, *deckname = 0, *listname = 0;
    int nodeck = 0, nolist = 0, i, opts = 0;
    FILE *in;
    char base[64], line[512];
    int changed, iter;

    bcd_set_encoding('A');
    for (i = 1; i < argc; ) {
        char *a = argv[i];
        char kw[16];
        int k;
        if (!strcmp(a, "(")) { opts = 1; i++; continue; }
        if (!strcmp(a, ")")) { i++; continue; }
        if (a[0] == '(' && a[1]) { opts = 1; a++; }
        else if (a[0] == '-' && a[1]) a++;
        else if (!opts) { src = take_file(argc, argv, &i); continue; }
        strncpy(kw, a, 15); kw[15] = 0;
        for (k = 0; kw[k]; k++) kw[k] = (char)toupper((unsigned char)kw[k]);
        i++;
        if (!strcmp(kw, "DECK") || !strcmp(kw, "O")) deckname = take_file(argc, argv, &i);
        else if (!strcmp(kw, "NODECK")) nodeck = 1;
        else if (!strcmp(kw, "LIST") || !strcmp(kw, "L")) listname = take_file(argc, argv, &i);
        else if (!strcmp(kw, "NOLIST")) nolist = 1;
        else if ((!strcmp(kw, "ENCODING") || !strcmp(kw, "ENC") || !strcmp(kw, "E")) && i < argc) {
            if (bcd_set_encoding(argv[i++][0])) { fprintf(stderr, "AUTOCODR: encoding must be A, H or S\n"); return 12; }
        } else if (!strcmp(kw, "BOOT") && i < argc) {
            boot = toupper((unsigned char)argv[i++][0]);
            if (boot != 'I' && boot != 'N') { fprintf(stderr, "AUTOCODR: BOOT I or N\n"); return 12; }
        } else if (!strcmp(kw, "SIZE") && i < argc) {
            char *s = argv[i++];
            corsize = atol(s);
            if (toupper((unsigned char)s[strlen(s) - 1]) == 'K') corsize *= 1000;
        } else if (!strcmp(kw, "HELP") || !strcmp(kw, "H") || !strcmp(kw, "?")) { src = 0; break; }
        else { fprintf(stderr, "AUTOCODR: unknown option %s\n", kw); return 12; }
        (void)ieq;
    }
    if (!src) {
        fprintf(stderr,
            "AUTOCODR " VERSION " - IBM 1401 Autocoder for VM/370plus\n"
            "usage: AUTOCODR source ( options\n"
            "  DECK file | NODECK   LIST file | NOLIST\n"
            "  ENCODING A|H|S   BOOT I|N   SIZE n\n"
            "on the PC each option may be written -option\n");
        return 12;
    }
    /* default output names */
#ifdef __CMS__
    {
        int k = 0;
        while (src[k] && src[k] != ' ' && src[k] != '.' && k < 8) { base[k] = src[k]; k++; }
        base[k] = 0;
    }
#else
    strncpy(base, src, 50); base[50] = 0;
    {
        char *dot = strrchr(base, '.');
        char *sl = strrchr(base, '/');
        if (dot && (!sl || dot > sl)) *dot = 0;
    }
#endif
    if (!(in = fopen(src, "r"))) { fprintf(stderr, "AUTOCODR: cannot open %s\n", src); return 12; }

    st = (Stmt *)calloc(MAXLINES, sizeof(Stmt));
    if (!st) { fprintf(stderr, "AUTOCODR: no storage\n"); return 12; }
    while (fgets(line, sizeof line, in)) {
        Stmt *s;
        int n, k;
        if (nst >= MAXLINES) { fprintf(stderr, "AUTOCODR: too many lines\n"); return 12; }
        s = &st[nst++];
        n = (int)strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        memset(s->line, ' ', 80);
        s->line[80] = 0;
        for (k = 0; k < n && k < 80; k++) s->line[k] = line[k] == '\t' ? ' ' : line[k];
        /* + and & are the same character (12 punch) in Autocoder */
        for (k = 0; k < 80; k++)
            s->bcd[k + 1] = s->line[k] == '+' ? B_AMPER : TOBCD(s->line[k]);
        s->bcd[0] = 0;
        s->lineno = nst;
    }
    fclose(in);

    /* pass 1, repeated until the symbol values settle */
    for (iter = 1; iter <= 10; iter++) {
        pass_no = iter;
        nlits = 0;
        sfx = ' ';
        prev_op = -1;
        for (i = 0; i < nsyms; i++) { syms[i].prev = syms[i].defined ? syms[i].val : syms[i].prev; syms[i].defined = 0; syms[i].val = REF; }
        p = 333; pmax = 0;
        for (i = 0; i < nst; i++) {
            pass1_stmt(&st[i]);
            if (st[i].why == W_END) { nst = i + 1; break; }
        }
        changed = 0;
        for (i = 0; i < nsyms; i++) if (syms[i].val != syms[i].prev) changed = 1;
        if (!changed && iter > 1) break;
    }

    if (!nodeck) {
        char nm[80];
        if (!deckname) {
#ifdef __CMS__
            sprintf(nm, "%s DECK A", base);
#else
            sprintf(nm, "%s.cd", base);
#endif
            deckname = nm;
        }
        if (!(deckf = fopen(deckname, "w"))) { fprintf(stderr, "AUTOCODR: cannot open %s\n", deckname); return 12; }
    }
    if (!nolist) {
        char nm[80];
        if (!listname) {
#ifdef __CMS__
            sprintf(nm, "%s LISTING A", base);
#else
            sprintf(nm, "%s.lst", base);
#endif
            listname = nm;
        }
        if (!strcmp(listname, "*")) listf = stdout;
        else if (!(listf = fopen(listname, "w"))) { fprintf(stderr, "AUTOCODR: cannot open %s\n", listname); return 12; }
        fprintf(listf, "AUTOCODR " VERSION "  IBM 1401 AUTOCODER  ENCODING %s\n\n", bcd_enc_name);
        fprintf(listf, " LINE  LABEL     OP   OPERANDS%57s CT  LOCN  INSTRUCTION   CARD\n\n", "");
    }
    pass2();
    if (listf) {
        symtab_list();
        fprintf(listf, "\n%d ERRORS, %d CARDS\n", errors, card_no - 1);
        if (listf != stdout) fclose(listf);
    }
    if (deckf) fclose(deckf);
    fprintf(stderr, "AUTOCODR: %d ERRORS, %d CARDS%s%s\n", errors, card_no - 1,
            deckf ? " IN " : "", deckf ? deckname : "");
    return errors ? 8 : warnings ? 4 : 0;
}
