/*
 * cpu.c - IBM 1401 processing unit
 *
 * Behaviour follows the IBM 1401 Reference Manual A24-1403-5 (April 1962):
 * instruction loading (I-phase, Figure 17), chaining, the address
 * register contents after each operation (Figure 18), word marks, the
 * arithmetic sign rules, editing (Figures 57/58) and the special features
 * (multiply/divide, indexing and store address register, high-low-equal
 * compare, bit test, expanded print edit, move record, modify address).
 * Edge cases (invalid lengths, address wrap) are made to agree with Bob
 * Supnik's SimH i1401, which the test suite uses as a second opinion.
 *
 * Part of the 1401 emulator for VM/370plus (mikebeer/1401).  C89.
 */
#include <stdio.h>
#include <string.h>
#include "i1401.h"

Machine m;

/* op codes (BCD) */
#define OP_R    001
#define OP_W    002
#define OP_WR   003
#define OP_P    004
#define OP_RP   005
#define OP_WP   006
#define OP_WRP  007
#define OP_SRF  010
#define OP_SPF  011
#define OP_MA   013
#define OP_MUL  014
#define OP_CS   021
#define OP_S    022
#define OP_MTF  024
#define OP_BWZ  025
#define OP_BBE  026
#define OP_MZ   030
#define OP_MCS  031
#define OP_SWM  033
#define OP_DIV  034
#define OP_SS   042
#define OP_LCA  043
#define OP_MCW  044
#define OP_NOP  045
#define OP_MCM  047
#define OP_SAR  050
#define OP_ZS   052
#define OP_A    061
#define OP_B    062
#define OP_C    063
#define OP_MN   064
#define OP_MCE  065
#define OP_CC   066
#define OP_SBR  070
#define OP_ZA   072
#define OP_H    073
#define OP_CWM  074

/* op table flags */
#define L1   0x0001
#define L2   0x0002
#define L4   0x0008
#define L5   0x0010
#define L7   0x0040
#define L8   0x0080
#define AREQ 0x0100     /* A-address must be valid */
#define BREQ 0x0200     /* B-address must be valid */
#define MLS  0x0400     /* move/load/store: B register not loaded from A */
#define NOWM 0x0800     /* 7 characters at most (SW, CS) */
#define IOOP 0x1000
#define NEED(f) ((long)(f) << 16)       /* required feature */

static const long op_table[64] = {
    0,
    L1|L2|L4|L5,                        /* 1 R */
    L1|L2|L4|L5,                        /* 2 W */
    L1|L2|L4|L5,                        /* 3 WR */
    L1|L2|L4|L5,                        /* 4 P */
    L1|L4,                              /* 5 RP */
    L1|L2|L4|L5,                        /* 6 WP */
    L1|L2|L4|L5,                        /* 7 WRP */
    L1,                                 /* 8 SRF */
    L1,                                 /* 9 SPF */
    0,
    L1|L4|L7|AREQ|BREQ|NEED(F_MA),      /* # MA */
    L1|L4|L7|AREQ|BREQ|NEED(F_MDV),     /* @ M (multiply) */
    0, 0, 0,
    0,
    L1|L4|L7|BREQ|NOWM,                 /* / CS */
    L1|L4|L7|AREQ|BREQ,                 /* S */
    0,
    L5|IOOP,                            /* U tape control */
    L1|L8|BREQ,                         /* V BWZ */
    L1|L8|BREQ|NEED(F_BBE),             /* W BBE */
    0,
    L1|L4|L7|AREQ|BREQ,                 /* Y MZ */
    L1|L4|L7|AREQ|BREQ,                 /* Z MCS */
    0,
    L1|L4|L7|AREQ|BREQ|NOWM,            /* , SW */
    L1|L4|L7|AREQ|BREQ|NEED(F_MDV),     /* % D (divide) */
    0, 0, 0,
    0, 0,
    L2|L5,                              /* K SS */
    L1|L4|L7|L8|BREQ|MLS|IOOP,          /* L LCA */
    L1|L4|L7|L8|BREQ|MLS|IOOP,          /* M MCW */
    L1|L2|L4|L5|L7|L8,                  /* N NOP */
    0,
    L1|L4|L7|AREQ|BREQ|NEED(F_MR),      /* P MCM */
    L1|L4|AREQ|MLS|NEED(F_XSA),         /* Q SAR */
    0,
    L1|L4|L7|AREQ|BREQ,                 /* ! ZS */
    0, 0, 0, 0, 0,
    0,
    L1|L4|L7|AREQ|BREQ,                 /* A */
    L1|L4|L5|L8,                        /* B */
    L1|L4|L7|AREQ|BREQ,                 /* C */
    L1|L4|L7|AREQ|BREQ,                 /* D MN */
    L1|L4|L7|AREQ|BREQ,                 /* E MCE */
    L2|L5,                              /* F CC */
    0,
    L1|L4|L7|AREQ|MLS,                  /* H SBR */
    0,
    L1|L4|L7|AREQ|BREQ,                 /* ? ZA */
    L1|L2|L4|L5|L7|L8,                  /* . H */
    L1|L4|L7|AREQ|BREQ,                 /* lozenge CW */
    0, 0, 0
};

/* address character tables, built at reset */
static long hun_t[64], ten_t[64], one_t[64];
static const int bin_of[16] = { 0,1,2,3,4,5,6,7,8,9,0,3,4,5,6,7 };
static const int bcd_of[10] = { B_ZERO,1,2,3,4,5,6,7,8,9 };

/* indicators that reset when tested */
static int ind_resets(int d)
{
    return d == IN_OVF || d == IN_LPT || d == IN_PRO ||
           d == IN_PNCH || d == IN_INC || d == IN_END || d == IN_TAP ||
           d == IN_READ;
}

/* collating sequence for high-low-equal compare (Figure 267) */
static int col_t[64];
static void build_collate(void)
{
    /* characters in ascending collating order, by BCD value */
    static const unsigned char seq[64] = {
        000, 073, 074, 075, 076, 077, 060, 053, 054, 055, 056, 057, 040,
        021, 033, 034, 035, 036, 037, 020, 013, 014, 015, 016, 017, 072,
        061, 062, 063, 064, 065, 066, 067, 070, 071, 052, 041, 042, 043,
        044, 045, 046, 047, 050, 051, 032, 022, 023, 024, 025, 026, 027,
        030, 031, 012, 001, 002, 003, 004, 005, 006, 007, 010, 011
    };
    int i;
    for (i = 0; i < 64; i++) col_t[seq[i]] = i;
}

static void build_tables(void)
{
    int c;
    static const long xtag[4] = { 0, 87L << V_INDEX, 92L << V_INDEX,
                                  97L << V_INDEX };
    for (c = 0; c < 64; c++) {
        int d = c & DIGIT, z = (c >> 4) & 3;
        int bad = (d == 0 || d > 10);
        hun_t[c] = bin_of[d] * 100L + z * 1000L + (bad ? BADADDR : 0);
        ten_t[c] = bin_of[d] * 10L + xtag[z] + ((bad && z == 0) ? BADADDR : 0);
        one_t[c] = bin_of[d] + z * 4000L + (bad ? BADADDR : 0);
    }
    build_collate();
}

long addr_of(int h, int t, int u)
{
    return hun_t[h & CHARM] + ten_t[t & CHARM] + one_t[u & CHARM];
}

#define ADDR_ERR(x) (((unsigned long)(x)) >= (unsigned long)m.memsize)

static int addr_h(long a) { return bcd_of[(a % 1000) / 100] | (int)(((a / 1000) & 3) << 4); }
static int addr_t(long a) { return bcd_of[(a % 100) / 10]; }
static int addr_u(long a) { return bcd_of[a % 10] | (int)(((a / 1000) & 014) << 2); }

void store_addr(long at, long addr)
{
    m.M[at]     = (unsigned char)((m.M[at] & WM)     | addr_u(addr));
    m.M[at - 1] = (unsigned char)((m.M[at - 1] & WM) | addr_t(addr));
    m.M[at - 2] = (unsigned char)((m.M[at - 2] & WM) | addr_h(addr));
}

static const char *stop_msgs[STOP_MAX] = {
    "running", "halt", "no word mark under op code",
    "invalid operation code", "invalid A-address", "invalid B-address",
    "invalid instruction length", "invalid d-modifier",
    "branch to an invalid address", "address wrapped through storage",
    "card reader empty", "carriage runaway (no punch in channel)",
    "device not installed", "tape unit not ready",
    "tape file protected", "tape write of a zero length record",
    "instruction limit reached", "file error", "console input ended"
};

const char *stop_text(int r)
{
    return (r >= 0 && r < STOP_MAX) ? stop_msgs[r] : "?";
}

void cpu_reset(void)
{
    int i;
    build_tables();
    for (i = 0; i < 64; i++)
        if (i < IN_SSB || i > IN_SSG) m.ind[i] = 0;
    m.ind[IN_UNC] = 1;
    m.AS = m.BS = BADADDR;
    m.D = 0;
    m.hb_pend = 0;
    if (!m.memsize) m.memsize = MAXMEM;
    if (!m.features) m.features = F_XSA|F_MDV|F_HLE|F_BBE|F_EPE|F_MR|F_MA;
    if (m.memsize <= 4000) m.features &= ~F_MA;
}

/* digit arithmetic helpers */
static const unsigned char sum_t[20] = {
    B_ZERO,1,2,3,4,5,6,7,8,9, B_ZERO,1,2,3,4,5,6,7,8,9
};
#define SUM(t)  ((t) >= 0 && (t) < 20 ? sum_t[t] : sum_t[(t) % 10])
#define BIN(c)  (bin_of[(c) & DIGIT])

static int div_add(long ap, long bp)
{
    int a, b, c = 0, r;
    do {
        a = m.M[ap]; b = m.M[bp];
        r = BIN(b) + BIN(a) + c;
        c = r >= 10;
        m.M[bp] = (unsigned char)((b & WM) | sum_t[r % 10 + (r >= 10 ? 10 : 0)]);
        ap--; bp--;
    } while (!(a & WM));
    return c;
}

static int div_sub(long ap, long bp)
{
    int a, b, c = 0, r;
    do {
        a = m.M[ap]; b = m.M[bp];
        r = BIN(b) - BIN(a) - c;
        c = r < 0;
        m.M[bp] = (unsigned char)((b & WM) | sum_t[r + 10]);
        ap--; bp--;
    } while (!(a & WM));
    b = m.M[bp];
    if (BIN(b) != 0) {
        r = BIN(b) - c;
        m.M[bp] = (unsigned char)((b & WM) | sum_t[r]);
        return 0;
    }
    return c;
}

static void div_sign(int dvr, int dvd, long qp, long rp)
{
    int neg = (dvr & ZONE) == BBIT;
    m.M[rp] |= ZONE;
    if (neg) m.M[rp] &= (unsigned char)~ABIT;
    m.M[qp] |= ZONE;
    if (((dvd & ZONE) == BBIT) ^ neg) m.M[qp] &= (unsigned char)~ABIT;
}

static int iomod_ok(int ilnt, int mod, const int *tab)
{
    if (ilnt != 2 && ilnt != 5 && ilnt < 8) return 1;
    if (!tab) return 0;
    for (; *tab >= 0; tab++) if (*tab == mod) return 1;
    return 0;
}

static const int r_mod[]  = { B_C, -1 };
static const int w_mod[]  = { B_S, B_LOZENGE, -1 };
static const int ss_mod[] = { B_ONE, B_TWO, B_FOUR, B_EIGHT, B_DOLLAR,
                              B_PERIOD, B_LOZENGE, -1 };
static const int mtf_mod[] = { B_A, B_B, B_E, B_M, B_R, B_U, -1 };

#define IO_INQ  023     /* %T0 */
#define IO_MT   024     /* %Ux */
#define IO_MTB  062     /* %Bx */
#define IO_DP   066     /* %Fx */

static int iodisp(int dev, int unit, int flag, int mod)
{
    switch (dev) {
    case IO_INQ: return io_console(flag, mod);
    case IO_MT:  return io_tape(unit, flag, mod);
    case IO_MTB: return io_tape(unit, flag | MD_BIN, mod);
    case IO_DP:  return io_disk(flag, mod);
    }
    return STOP_NXD;
}

/* trace one instruction */
void cpu_disasm(FILE *f, long is, int len)
{
    int i;
    fprintf(f, "%05ld ", is);
    for (i = 0; i < len && is + i < m.memsize; i++)
        fputc(TOCHR(m.M[is + i]), f);
    fprintf(f, "%*s", 9 - len > 0 ? 9 - len : 1, "");
}

#define MM(x) do { if (--(x) < 0) { (x) = BADADDR + MAXMEM - 1; \
                   reason = STOP_WRAP; goto done; } } while (0)
#define PP(x) do { if (ADDR_ERR(++(x))) { (x) = BADADDR + ((x) % MAXMEM); \
                   reason = STOP_WRAP; goto done; } } while (0)
#define BRANCH do { if (ADDR_ERR(m.AS)) { reason = STOP_INVBR; goto done; } \
                    m.BS = (m.features & F_XSA) ? IS : BADADDR; \
                    IS = m.AS; } while (0)

int cpu_run(void)
{
    long IS = m.IS, flags, xa;
    int op, t, wm, ioind = 0, dev = 0, unit = 0, ilnt;
    int a, b, i;
    long asave, bsave, lowprd, ps, qs, k;
    int carry, sign, quo, qsign, qawm, qzero, qbody, qdollar, qaster, qdecimal;
    int reason = STOP_NONE;
    unsigned char *M = m.M;

    for (;;) {
        if (m.hb_pend) {
            m.hb_pend = 0;
            BRANCH;
        }
        m.saved_IS = IS;
        if (m.ilimit && m.icount >= m.ilimit) { reason = STOP_LIMIT; break; }
        m.icount++;

        /* ---------------- I-phase ---------------- */
        if (ADDR_ERR(IS)) { reason = STOP_WRAP; break; }
        if (!(M[IS] & WM)) { reason = STOP_NOWM; break; }
        op = M[IS] & CHARM;
        flags = op_table[op];
        if (!flags || ((flags >> 16) & ~m.features)) { reason = STOP_NXI; break; }
        if (op == OP_SAR) m.BS = m.AS;
        PP(IS);

        if ((t = M[IS]) & WM) goto check_length;            /* 1 char */
        m.D = ioind = t;
        m.AS = hun_t[t];
        PP(IS);

        if ((t = M[IS]) & WM) {                             /* 2 chars */
            m.AS |= BADADDR;
            if (!(flags & MLS)) m.BS = m.AS;
            goto check_length;
        }
        m.D = dev = t;
        m.AS += ten_t[t];
        PP(IS);

        if ((t = M[IS]) & WM) {                             /* 3 chars */
            m.AS |= BADADDR;
            if (!(flags & MLS)) m.BS = m.AS;
            goto check_length;
        }
        m.D = unit = t;
        if (unit == B_ZERO) unit = 0;
        m.AS += one_t[t];
        xa = (m.AS >> V_INDEX) & M_INDEX;
        if (xa && ioind != B_PERCNT && (m.features & F_XSA)) {
            m.AS = m.AS + hun_t[M[xa] & CHARM] + ten_t[M[xa + 1] & CHARM]
                        + one_t[M[xa + 2] & CHARM];
            m.AS = (m.AS & 0x7FFFL) % MAXMEM;
        }
        if (!(flags & MLS)) m.BS = m.AS;
        PP(IS);

        if ((t = M[IS]) & WM) goto check_length;            /* 4 chars */
        if (op == OP_B && t == B_BLANK) goto check_length;
        m.D = t;
        m.BS = hun_t[t];
        PP(IS);

        if ((t = M[IS]) & WM) {                             /* 5 chars */
            m.BS |= BADADDR;
            goto check_length;
        }
        m.D = t;
        m.BS += ten_t[t];
        PP(IS);

        if ((t = M[IS]) & WM) {                             /* 6 chars */
            m.BS |= BADADDR;
            goto check_length;
        }
        m.D = t;
        m.BS += one_t[t];
        xa = (m.BS >> V_INDEX) & M_INDEX;
        if (xa && (m.features & F_XSA)) {
            m.BS = m.BS + hun_t[M[xa] & CHARM] + ten_t[M[xa + 1] & CHARM]
                        + one_t[M[xa + 2] & CHARM];
            m.BS = (m.BS & 0x7FFFL) % MAXMEM;
        }
        PP(IS);

        if (flags & NOWM) goto check_length;                /* SW, CS: 7 */
        if ((t = M[IS]) & WM) goto check_length;            /* 7 chars */
        while (!((t = M[IS]) & WM)) {                       /* 8: d */
            m.D = t;
            PP(IS);
        }

check_length:
        if ((flags & BREQ) && ADDR_ERR(m.BS)) { reason = STOP_INVB; break; }
        if ((flags & AREQ) && ADDR_ERR(m.AS)) { reason = STOP_INVA; break; }
        ilnt = (int)(IS - m.saved_IS);
        if (m.trace && m.tracef) {
            cpu_disasm(m.tracef, m.saved_IS, ilnt);
            fprintf(m.tracef, " A=%05ld B=%05ld\n",
                    m.AS & ADDRMASK, m.BS & ADDRMASK);
        }

        /* ---------------- E-phase ---------------- */
        switch (op) {

        case OP_MCW:
            if (ilnt >= 4 && ioind == B_PERCNT) {
                reason = iodisp(dev, unit, 0, m.D);
                break;
            }
            if (ADDR_ERR(m.AS)) { reason = STOP_INVA; break; }
            do {
                wm = M[m.AS] | M[m.BS];
                M[m.BS] = (unsigned char)((M[m.BS] & WM) | (M[m.AS] & CHARM));
                MM(m.AS); MM(m.BS);
            } while (!(wm & WM));
            break;

        case OP_LCA:
            if (ilnt >= 4 && ioind == B_PERCNT) {
                reason = iodisp(dev, unit, MD_WM, m.D);
                break;
            }
            if (ADDR_ERR(m.AS)) { reason = STOP_INVA; break; }
            do {
                wm = M[m.BS] = M[m.AS];
                MM(m.AS); MM(m.BS);
            } while (!(wm & WM));
            break;

        case OP_MCM:                    /* move to record or group mark */
            do {
                t = M[m.AS];
                M[m.BS] = (unsigned char)((M[m.BS] & WM) | (t & CHARM));
                PP(m.AS); PP(m.BS);
            } while ((t & CHARM) != B_RECMRK && t != (B_GRPMRK | WM));
            break;

        case OP_MCS:                    /* move and suppress zeros */
            bsave = m.BS;
            qzero = 1;
            do {
                wm = M[m.AS];
                M[m.BS] = (unsigned char)(M[m.AS] & (m.BS != bsave ? CHARM : DIGIT));
                MM(m.AS); MM(m.BS);
            } while (!(wm & WM));
            do {
                PP(m.BS);
                t = M[m.BS];
                if (t == B_ZERO || t == B_COMMA) {
                    if (qzero) M[m.BS] = 0;
                } else if (t == B_BLANK || t == B_MINUS)
                    ;
                else if ((t == B_PERIOD && (m.features & F_EPE)) || t <= B_NINE)
                    qzero = 0;
                else
                    qzero = 1;
            } while (m.BS < bsave);
            PP(m.BS);
            break;

        case OP_MN:
            M[m.BS] = (unsigned char)((M[m.BS] & ~DIGIT) | (M[m.AS] & DIGIT));
            MM(m.AS); MM(m.BS);
            break;

        case OP_MZ:
            M[m.BS] = (unsigned char)((M[m.BS] & ~ZONE) | (M[m.AS] & ZONE));
            MM(m.AS); MM(m.BS);
            break;

        case OP_B:
            if (ilnt == 4) {
                BRANCH;
            } else if (ilnt == 5) {
                if (m.ind[m.D]) BRANCH;
                if (ind_resets(m.D)) m.ind[m.D] = 0;
            } else {
                if (ADDR_ERR(m.BS)) { reason = STOP_INVB; break; }
                if ((M[m.BS] & CHARM) == m.D) BRANCH;
                else MM(m.BS);
            }
            break;

        case OP_BWZ:
            if (((m.D & 1) && (M[m.BS] & WM)) ||
                ((m.D & 2) && ((M[m.BS] & ZONE) == (m.D & ZONE))))
                BRANCH;
            else
                MM(m.BS);
            break;

        case OP_BBE:
            if (M[m.BS] & m.D & CHARM) BRANCH;
            else MM(m.BS);
            break;

        case OP_ZA: case OP_ZS:
            a = 0; i = 0;
            do {
                if (a & WM)
                    wm = M[m.BS] = (unsigned char)((M[m.BS] & WM) | B_ZERO);
                else {
                    a = M[m.AS];
                    wm = M[m.BS] = (unsigned char)((M[m.BS] & WM) | (a & DIGIT));
                    MM(m.AS);
                }
                if (i == 0)
                    i = M[m.BS] = (unsigned char)(M[m.BS] |
                        ((((a & ZONE) == BBIT) ^ (op == OP_ZS)) ? BBIT : ZONE));
                MM(m.BS);
            } while (!(wm & WM));
            break;

        case OP_A: case OP_S:
            bsave = m.BS;
            a = M[m.AS];
            b = M[m.BS];
            MM(m.AS);
            qsign = ((a & ZONE) == BBIT) ^ ((b & ZONE) == BBIT) ^ (op == OP_S);
            t = BIN(a);
            t = BIN(b) + (qsign ? 10 - t : t);
            carry = t >= 10;
            b = (b & ~DIGIT) | sum_t[t];
            if (qsign && !(b & BBIT)) b |= ZONE;
            M[m.BS] = (unsigned char)b;
            MM(m.BS);
            if (b & WM) {
                if (qsign && !carry)
                    M[bsave] = (unsigned char)(WM + ((b & ZONE) ^ ABIT) + sum_t[10 - t]);
                if (!qsign && carry) m.ind[IN_OVF] = 1;
                break;
            }
            do {
                if (a & WM) a = WM;
                else { a = M[m.AS]; MM(m.AS); }
                b = M[m.BS];
                t = BIN(a);
                t = BIN(b) + (qsign ? 9 - t : t) + carry;
                carry = t >= 10;
                if ((b & WM) && !qsign) {
                    M[m.BS] = (unsigned char)(WM + sum_t[t] +
                        (((a & ZONE) + b + (carry ? ABIT : 0)) & ZONE));
                    if (carry) m.ind[IN_OVF] = 1;
                } else
                    M[m.BS] = (unsigned char)((b & WM) + sum_t[t]);
                MM(m.BS);
            } while (!(b & WM));
            if (qsign && !carry) {      /* recomplement */
                M[bsave] ^= ABIT;
                for (carry = 1; bsave != m.BS; --bsave) {
                    t = 9 - BIN(M[bsave]) + carry;
                    carry = t >= 10;
                    M[bsave] = (unsigned char)((M[bsave] & ~DIGIT) | sum_t[t]);
                }
            }
            break;

        case OP_C:
            if (ilnt != 1) {
                m.ind[IN_EQU] = 1;
                m.ind[IN_UNQ] = m.ind[IN_HGH] = m.ind[IN_LOW] = 0;
            }
            do {
                a = M[m.AS]; b = M[m.BS];
                wm = a | b;
                if ((a & CHARM) != (b & CHARM)) {
                    m.ind[IN_EQU] = 0;
                    m.ind[IN_UNQ] = 1;
                    m.ind[IN_HGH] = col_t[b & CHARM] > col_t[a & CHARM];
                    m.ind[IN_LOW] = m.ind[IN_HGH] ^ 1;
                }
                MM(m.AS); MM(m.BS);
            } while (!(wm & WM));
            if ((a & WM) && !(b & WM)) {
                m.ind[IN_EQU] = m.ind[IN_LOW] = 0;
                m.ind[IN_UNQ] = m.ind[IN_HGH] = 1;
            }
            if (!(m.features & F_HLE))
                m.ind[IN_EQU] = m.ind[IN_LOW] = m.ind[IN_HGH] = 0;
            break;

        /* ---------- 1402 / 1403 ---------- */
        case OP_R:
            if (!iomod_ok(ilnt, m.D, r_mod)) { reason = STOP_INVM; break; }
            reason = io_read_card(ilnt, m.D);
            m.BS = 81;
            if (!reason && (ilnt == 4 || ilnt == 5)) BRANCH;
            break;
        case OP_W:
            if (!iomod_ok(ilnt, m.D, w_mod)) { reason = STOP_INVM; break; }
            reason = io_write_line(ilnt, m.D);
            m.BS = 333;
            if (!reason && (ilnt == 4 || ilnt == 5)) BRANCH;
            break;
        case OP_P:
            if (!iomod_ok(ilnt, m.D, r_mod)) { reason = STOP_INVM; break; }
            reason = io_punch_card(ilnt, m.D);
            m.BS = 181;
            if (!reason && (ilnt == 4 || ilnt == 5)) BRANCH;
            break;
        case OP_WR:
            if (!iomod_ok(ilnt, m.D, w_mod)) { reason = STOP_INVM; break; }
            reason = io_write_line(ilnt, m.D);
            if (!reason) reason = io_read_card(ilnt, m.D);
            m.BS = 81;
            if (!reason && (ilnt == 4 || ilnt == 5)) BRANCH;
            break;
        case OP_WP:
            if (!iomod_ok(ilnt, m.D, w_mod)) { reason = STOP_INVM; break; }
            reason = io_write_line(ilnt, m.D);
            if (!reason) reason = io_punch_card(ilnt, m.D);
            m.BS = 181;
            if (!reason && (ilnt == 4 || ilnt == 5)) BRANCH;
            break;
        case OP_RP:
            if (!iomod_ok(ilnt, m.D, 0)) { reason = STOP_INVM; break; }
            reason = io_read_card(ilnt, m.D);
            if (!reason) reason = io_punch_card(ilnt, m.D);
            m.BS = 181;
            if (!reason && (ilnt == 4 || ilnt == 5)) BRANCH;
            break;
        case OP_WRP:
            if (!iomod_ok(ilnt, m.D, w_mod)) { reason = STOP_INVM; break; }
            reason = io_write_line(ilnt, m.D);
            if (!reason) reason = io_read_card(ilnt, m.D);
            if (!reason) reason = io_punch_card(ilnt, m.D);
            m.BS = 181;
            if (!reason && (ilnt == 4 || ilnt == 5)) BRANCH;
            break;
        case OP_SS:
            if (!iomod_ok(ilnt, m.D, ss_mod)) { reason = STOP_INVM; break; }
            reason = io_select_stacker(m.D);
            if (!reason && (ilnt == 4 || ilnt == 5)) BRANCH;
            break;
        case OP_CC:
            reason = io_carriage(m.D);
            if (!reason && (ilnt == 4 || ilnt == 5)) BRANCH;
            break;
        case OP_SRF: case OP_SPF:
            break;

        /* ---------- 729 control ---------- */
        case OP_MTF:
            if (ilnt < 4) { reason = STOP_INVL; break; }
            if (ioind != B_PERCNT) { reason = STOP_INVA; break; }
            if (!iomod_ok(ilnt, m.D, mtf_mod)) { reason = STOP_INVM; break; }
            if (dev == IO_MT) reason = io_tape_func(unit, 0, m.D);
            else if (dev == IO_MTB) reason = io_tape_func(unit, MD_BIN, m.D);
            else reason = STOP_INVA;
            break;

        /* ---------- edit ---------- */
        case OP_MCE:
            a = M[m.AS];
            b = M[m.BS];
            t = a & DIGIT;
            MM(m.AS);
            qsign = (a & ZONE) == BBIT;
            qawm = qzero = qbody = 0;
            qdollar = qaster = qdecimal = 0;
            /* pass 1: right to left under control of the B field */
            do {
                b = M[m.BS];
                M[m.BS] &= (unsigned char)~WM;
                switch (b & CHARM) {
                case B_ASTER:
                    if (!qbody || qdollar || !(m.features & F_EPE)) break;
                    qaster = 1;
                    goto a_cycle;
                case B_DOLLAR:
                    if (!qbody || qaster || !(m.features & F_EPE)) break;
                    qdollar = 1;
                    goto a_cycle;
                case B_ZERO:
                    if (qawm) {
                        if (!qzero) M[m.BS] |= WM;
                        qzero = 1;
                        break;
                    }
                    if (!qzero) t |= WM;
                    qzero = 1;
                    goto a_cycle;
                case B_BLANK:
                    if (qawm) break;
                a_cycle:
                    M[m.BS] = (unsigned char)t;
                    if (a & WM) {
                        qbody = 0;
                        qawm = 1;
                    } else {
                        qbody = 1;
                        a = M[m.AS];
                        MM(m.AS);
                        t = a & CHARM;
                    }
                    break;
                case B_C: case B_R: case B_MINUS:
                    if (!qsign && !qbody) M[m.BS] = B_BLANK;
                    break;
                case B_COMMA:
                    if (!qbody) M[m.BS] = B_BLANK;
                    break;
                case B_AMPER:
                    M[m.BS] = B_BLANK;
                    break;
                }
                MM(m.BS);
            } while (!(b & WM));
            if (!qzero) break;
            /* pass 2: left to right, zero suppression */
            do {
                PP(m.BS);
                b = M[m.BS];
                switch (b & CHARM) {
                case B_ONE: case B_TWO: case B_THREE: case B_FOUR:
                case B_FIVE: case B_SIX: case B_SEVEN: case B_EIGHT:
                case B_NINE:
                    qzero = 0;
                    break;
                case B_ZERO: case B_COMMA:
                    if (qzero && !qdecimal)
                        M[m.BS] = (unsigned char)(qaster ? B_ASTER : B_BLANK);
                    break;
                case B_BLANK:
                    if (qaster) M[m.BS] = B_ASTER;
                    break;
                case B_PERIOD:
                    if (qzero && (m.features & F_EPE)) qdecimal = 1;
                    break;
                case B_PERCNT: case B_WMCHR: case B_BSLASH:
                case B_TAPSEG: case B_MINUS:
                    break;
                default:
                    qzero = 1;
                    break;
                }
            } while (!(b & WM));
            M[m.BS] &= (unsigned char)~WM;
            if (!qdollar && !(qdecimal && qzero)) {
                m.BS++;
                break;
            }
            if (qdecimal && qzero) qdollar = 0;
            /* pass 3 (expanded print edit): right to left */
            for (;;) {
                b = M[m.BS];
                if (b == B_BLANK && qdollar) { M[m.BS] = B_DOLLAR; break; }
                if (b == B_PERIOD) {
                    M[m.BS] = (unsigned char)(qaster ? B_ASTER : B_BLANK);
                    break;
                }
                if (b == B_ZERO && !qdollar)
                    M[m.BS] = (unsigned char)(qaster ? B_ASTER : B_BLANK);
                MM(m.BS);
            }
            break;

        /* ---------- multiply / divide ---------- */
        case OP_MUL:
            asave = m.AS;
            bsave = lowprd = m.BS;
            do {
                a = M[m.AS];
                M[m.BS] = B_ZERO;
                MM(m.AS); MM(m.BS);
            } while (!(a & WM));
            M[m.BS] = B_ZERO;
            MM(m.BS);
            sign = ((M[asave] & ZONE) == BBIT) ^ ((M[m.BS] & ZONE) == BBIT);
            do {
                ps = bsave;
                m.AS = asave;
                carry = 0;
                b = M[m.BS];
                do {
                    a = M[m.AS];
                    t = BIN(a) * BIN(b) + carry + BIN(M[ps]);
                    carry = t / 10;
                    M[ps] = (unsigned char)((M[ps] & WM) | bcd_of[t % 10]);
                    MM(m.AS);
                    ps--;
                } while (!(a & WM));
                M[m.BS] = (unsigned char)((M[m.BS] & WM) | B_ZERO);
                t = BIN(M[ps]) + carry;
                M[ps] = (unsigned char)((M[ps] & WM) | bcd_of[t % 10]);
                bsave--;
                MM(m.BS);
            } while (!(b & WM));
            M[lowprd] |= ZONE;
            if (sign) M[lowprd] &= (unsigned char)~ABIT;
            break;

        case OP_DIV:
            asave = m.AS;
            t = 0;
            do {
                a = M[m.AS];
                if (BIN(a)) t = 1;
                MM(m.AS);
            } while (!(a & WM));
            if (t == 0) {               /* divide by zero */
                m.ind[IN_OVF] = 1;
                qs = bsave = m.BS;
                do {
                    b = M[bsave];
                    PP(bsave);
                } while (!(b & ZONE));
                if (ADDR_ERR(qs)) { reason = STOP_WRAP; break; }
                div_sign(M[asave], b, qs - 1, bsave - 1);
                m.BS = (m.BS - 2) - (asave - (m.AS + 1));
                break;
            }
            bsave = m.BS;
            qs = m.BS - (asave - m.AS) - 1;
            do {
                quo = 0;
                if (ADDR_ERR(qs) || ADDR_ERR(bsave)) { reason = STOP_WRAP; goto done; }
                b = M[bsave];
                do {
                    t = div_sub(asave, bsave);
                    quo++;
                    /* a quotient field over the divisor can make the
                       machine subtract zero for ever: the instruction
                       limit counts these cycles too */
                    if (m.ilimit && ++m.icount >= m.ilimit) { reason = STOP_LIMIT; goto done; }
                } while (t == 0);
                div_add(asave, bsave);
                quo--;
                if (quo > 9) m.ind[IN_OVF] = 1;
                M[qs] = (unsigned char)((M[qs] & WM) | bcd_of[quo % 10]);
                bsave++;
                qs++;
            } while (!(b & ZONE));
            div_sign(M[asave], b, qs - 1, bsave - 1);
            m.BS = qs - 2;
            break;

        /* ---------- word marks, clear storage ---------- */
        case OP_SWM:
            M[m.BS] |= WM;
            M[m.AS] |= WM;
            MM(m.AS); MM(m.BS);
            break;

        case OP_CWM:
            M[m.BS] &= (unsigned char)~WM;
            M[m.AS] &= (unsigned char)~WM;
            MM(m.AS); MM(m.BS);
            break;

        case OP_CS:
            k = (m.BS / 100) * 100;
            while (m.BS >= k) M[m.BS--] = 0;
            if (m.BS < 0) m.BS += m.memsize;
            if (ilnt == 7) {            /* clear storage and branch */
                if (ADDR_ERR(m.AS)) { reason = STOP_INVBR; break; }
                IS = m.AS;
            }
            break;

        /* ---------- address arithmetic ---------- */
        case OP_MA:
            a = 0;
            k = one_t[M[m.AS] & CHARM]; MM(m.AS);
            k += ten_t[M[m.AS] & CHARM]; MM(m.AS);
            k += hun_t[M[m.AS] & CHARM]; MM(m.AS);
            asave = k;
            k = one_t[M[m.BS] & CHARM]; MM(m.BS);
            k += ten_t[M[m.BS] & CHARM]; MM(m.BS);
            k += hun_t[M[m.BS] & CHARM]; MM(m.BS);
            bsave = k;
            k = ((asave + bsave) & 0x7FFFL) % MAXMEM;
            M[m.BS + 3] = (unsigned char)((M[m.BS + 3] & WM) | addr_u(k));
            M[m.BS + 2] = (unsigned char)((M[m.BS + 2] & (WM | ZONE)) | addr_t(k));
            M[m.BS + 1] = (unsigned char)((M[m.BS + 1] & WM) | addr_h(k));
            if (((asave & 0x7FFFL) % 4000) + ((bsave & 0x7FFFL) % 4000) >= 4000)
                m.BS += 2;
            break;

        case OP_SAR: case OP_SBR:
            M[m.AS] = (unsigned char)((M[m.AS] & WM) | addr_u(m.BS)); MM(m.AS);
            M[m.AS] = (unsigned char)((M[m.AS] & WM) | addr_t(m.BS)); MM(m.AS);
            M[m.AS] = (unsigned char)((M[m.AS] & WM) | addr_h(m.BS)); MM(m.AS);
            break;

        case OP_NOP:
            break;

        case OP_H:
            if (ilnt == 4) m.hb_pend = 1;
            reason = STOP_HALT;
            break;

        default:
            reason = STOP_NXI;
            break;
        }
done:
        if (reason) break;
    }
    if (reason == STOP_HALT) m.saved_IS = IS;   /* START resumes here */
    m.IS = IS;
    return reason;
}
