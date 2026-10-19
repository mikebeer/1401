/*
 * io.c - IBM 1401 input/output units
 *
 *   1402 card read-punch   reader: a list of files read as one hopper,
 *                          one card per line; punch: one card per line
 *   1403 printer           132 positions, carriage control tape (66
 *                          lines, channel 1 on line 1 by default);
 *                          plain text, or ASA carriage control in column
 *                          1 for CMS PRINT ( CC and the VM spool
 *   729 tapes 1-6          held in storage while the program runs; a
 *                          file is either SimH .tap (name ends in .tap)
 *                          or text with one record per line
 *   1407 console inquiry   the terminal (stdin/stdout)
 *
 * Characters cross the boundary in the selected external encoding
 * (bcd.c).  Part of the 1401 emulator for VM/370plus.  C89.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i1401.h"

IOState io;

/* ======================================================= 1402 reader */

int rdr_open_next(void)
{
    if (io.rdr) {
        if (io.rdr != stdin) fclose(io.rdr);
        io.rdr = 0;
    }
    while (io.rdr_i < io.rdr_n) {
        const char *n = io.rdr_names[io.rdr_i++];
        if (!strcmp(n, "-"))
            io.rdr = stdin;
        else
            io.rdr = fopen(n, "r");
        if (io.rdr) return 0;
        fprintf(stderr, "IBM1401: cannot open card file %s\n", n);
        return -1;
    }
    return 1;
}

/* the next character of the hopper, crossing file boundaries */
static int rdr_getc(void)
{
    int c;
    for (;;) {
        if (!io.rdr) {
            if (rdr_open_next()) return EOF;
        }
        c = getc(io.rdr);
        if (c != EOF) return c;
        if (rdr_open_next()) return EOF;
    }
}

static int hopper_empty(void)
{
    int c;
    if (io.rdr_peek != EOF) return 0;
    c = rdr_getc();
    io.rdr_peek = c;
    return c == EOF;
}

/* read one card image into buf (80 characters, blank filled) */
static int rdr_card(char *buf, int width)
{
    int c, n = 0;
    memset(buf, ' ', (size_t)width);
    c = io.rdr_peek != EOF ? io.rdr_peek : rdr_getc();
    io.rdr_peek = EOF;
    if (c == EOF) return -1;
    while (c != EOF && c != '\n') {
        if (c != '\r' && n < width) buf[n++] = (char)c;
        c = rdr_getc();
    }
    return 0;
}

/* ===================================================== column binary */

/* A column-binary card is a line of 160 characters, two per column: the
   first holds rows 12,11,0,1,2,3 and the second rows 4-9, each as the
   bits B A 8 4 2 1 of a BCD character (SimH's format; with encoding S
   the 80 first halves come first, then the 80 second halves).  The card
   code of each BCD character, as 12 bits 12,11,0,1,...,9 (bit 11 = 12). */
static int hol[64]; /* BCD -> punches, -1 if none */
static int hol_done;

static void hol_init(void)
{
    int c, z, d, p;
    static const int zone[4] = {0, 1 << 9, 1 << 10, 1 << 11}; /* -,0,11,12 */
    for (c = 0; c < 64; c++) {
        z = (c >> 4) & 3;
        d = c & 15;
        p = zone[z];
        if (d == 0) {
            if (z == 1) p = -1; /* 020 has no card code */
        } else if (d <= 9)
            p |= 1 << (9 - d);
        else if (d == 10)
            p = z == 0   ? 1 << 9
                : z == 1 ? (1 << 9) | (1 << 7) | (1 << 1)
                         : zone[z] | (1 << 9);
        else
            p |= (1 << (9 - (d - 8))) | (1 << 1); /* 3-8 ... 7-8 */
        hol[c] = p;
    }
    hol_done = 1;
}

/* the BCD character a column's punches read as: its character if the
   punches are one, else the rows ORed together as SimH does (hash) */
static int colbin_to_bcd(int cb)
{
    static const int row_val[12] = {011, 010, 007, 006, 005, 004,
                                    003, 002, 001, 020, 040, 060};
    int i, bcd = 0;
    if (!hol_done) hol_init();
    for (i = 0; i < 64; i++)
        if (hol[i] == cb) return i;
    for (i = 0; i < 12; i++)
        if (cb & (1 << i)) bcd |= row_val[i];
    return bcd;
}

static int cb_old(void) { return bcd_enc_name[0] == 'S'; }

static int read_colbin(void)
{
    char buf[160];
    int i, c1, c2;
    if (rdr_card(buf, 160)) return STOP_NOCARD;
    for (i = 0; i < 80; i++) {
        c1 = TOBCD(buf[cb_old() ? i : 2 * i]) & CHARM;
        c2 = TOBCD(buf[cb_old() ? 80 + i : 2 * i + 1]) & CHARM;
        m.M[401 + i] = (unsigned char)((m.M[401 + i] & WM) | c1);
        m.M[501 + i] = (unsigned char)((m.M[501 + i] & WM) | c2);
        m.M[1 + i] =
            (unsigned char)((m.M[1 + i] & WM) | colbin_to_bcd((c1 << 6) | c2));
    }
    return STOP_NONE;
}

int io_read_card(int ilnt, int mod)
{
    char buf[80];
    int i, r;
    m.ind[IN_READ] = m.ind[IN_LST] = 0;
    if ((ilnt == 2 || ilnt == 5) && mod == B_C) {
        r = read_colbin();
        if (r) return r;
    } else {
        if (rdr_card(buf, 80)) return STOP_NOCARD;
        for (i = 0; i < 80; i++)
            m.M[1 + i] = (unsigned char)((m.M[1 + i] & WM) | TOBCD(buf[i]));
    }
    m.M[0] = 060; /* C A B bits in 000 */
    io.cards_read++;
    if (hopper_empty()) m.ind[IN_LST] = 1;
    return STOP_NONE;
}

/* the LOAD key: clear 001-080, read a card, word mark in 001, start 001 */
int io_load(void)
{
    int i, r;
    for (i = 1; i <= 80; i++) m.M[i] = 0;
    r = io_read_card(1, 0);
    if (r) return r;
    m.M[1] |= WM;
    m.IS = 1;
    return STOP_NONE;
}

/* ======================================================== 1402 punch */

int io_punch_card(int ilnt, int mod)
{
    char buf[161];
    int i, n;
    m.ind[IN_PNCH] = 0;
    if ((ilnt == 2 || ilnt == 5) && mod == B_C) { /* column binary */
        for (i = 0; i < 80; i++) {
            buf[cb_old() ? i : 2 * i] = TOCHR(m.M[401 + i]);
            buf[cb_old() ? 80 + i : 2 * i + 1] = TOCHR(m.M[501 + i]);
        }
        n = 160;
    } else {
        for (i = 0; i < 80; i++) buf[i] = TOCHR(m.M[101 + i]);
        n = 80;
    }
    for (; n > 0 && buf[n - 1] == ' '; n--);
    buf[n] = 0;
    m.M[100] = B_ZERO; /* C 8 2 bits in 100 */
    io.cards_punched++;
    if (!io.pun) return STOP_NONE; /* no punch file: cards are discarded */
    fputs(buf, io.pun);
    fputc('\n', io.pun);
    return ferror(io.pun) ? STOP_IOERR : STOP_NONE;
}

int io_select_stacker(int mod)
{
    (void)mod; /* all cards go to the one output */
    return STOP_NONE;
}

/* ====================================================== 1403 printer */

#define CHP(ch, v) ((v) & (1 << (ch)))

/* plain output: the line, then the paper motion as newlines (and a form
   feed for a skip to channel 1), as SimH's printer writes it */
static void lpt_motion(int count, int skip)
{
    int i;
    io.cctptr = (io.cctptr + count) % io.cctlnt;
    if (io.lpt_asa) {
        /* remember the motion; it is written before the next line */
        if (skip && CHP(0, io.cct[io.cctptr]))
            io.pending_asa = -1;
        else if (io.pending_asa >= 0)
            io.pending_asa += count;
    } else if (io.lpt) {
        if (skip && CHP(0, io.cct[io.cctptr]))
            fputs("\n\f", io.lpt);
        else
            for (i = 0; i < count; i++) fputc('\n', io.lpt);
    }
    m.ind[IN_CC9] = CHP(9, io.cct[io.cctptr]) != 0;
    m.ind[IN_CC12] = CHP(12, io.cct[io.cctptr]) != 0;
}

static void lpt_put(const char *line)
{
    if (!io.lpt) return;
    if (io.lpt_asa) {
        int p = io.pending_asa;
        char cc;
        if (p < 0)
            cc = '1';
        else {
            while (p > 3) {
                fputs(" \n", io.lpt);
                p--;
            }
            cc = p == 0 ? '+' : p == 1 ? ' ' : p == 2 ? '0' : '-';
        }
        fputc(cc, io.lpt);
        fputs(line, io.lpt);
        fputc('\n', io.lpt);
        io.pending_asa = 0;
    } else
        fputs(line, io.lpt);
}

int io_write_line(int ilnt, int mod)
{
    char buf[133];
    int i, n, wm, sup;
    wm = (ilnt == 2 || ilnt == 5) && mod == B_LOZENGE;
    sup = (ilnt == 2 || ilnt == 5) && mod == B_S;
    m.ind[IN_LPT] = 0;
    for (i = 0; i < 132; i++) {
        int t = m.M[201 + i];
        buf[i] = wm ? ((t & WM) ? '1' : ' ') : TOCHR(t);
    }
    for (n = 132; n > 0 && buf[n - 1] == ' '; n--);
    buf[n] = 0;
    lpt_put(buf);
    io.lines_printed++;
    if (io.lines)
        lpt_motion(io.lines, io.lflag);
    else if (!sup)
        lpt_motion(1, 0);
    else if (io.lpt && !io.lpt_asa)
        fputc('\r', io.lpt);
    io.lines = io.lflag = 0;
    if (io.lpt && ferror(io.lpt)) return STOP_IOERR;
    return STOP_NONE;
}

int io_carriage(int mod)
{
    int i, action = (mod & ZONE) >> 4;
    mod &= DIGIT;
    switch (action) {
    case 0: /* skip to channel now */
        if (mod == 0 || mod > 12 || CHP(mod, io.cct[io.cctptr])) return 0;
        for (i = 1; i < io.cctlnt + 1; i++)
            if (CHP(mod, io.cct[(io.cctptr + i) % io.cctlnt])) {
                lpt_motion(i, 1);
                return 0;
            }
        return STOP_CCT;
    case 1: /* space after the next print */
        if (mod <= 3) {
            io.lines = mod;
            io.lflag = 0;
            m.ind[IN_CC9] = m.ind[IN_CC12] = 0;
        }
        return 0;
    case 2: /* space now */
        if (mod <= 3) lpt_motion(mod, 0);
        return 0;
    case 3: /* skip to channel after the next print */
        if (mod == 0 || mod > 12) return 0;
        m.ind[IN_CC9] = m.ind[IN_CC12] = 0;
        for (i = 1; i < io.cctlnt + 1; i++)
            if (CHP(mod, io.cct[(io.cctptr + i) % io.cctlnt])) {
                io.lines = i;
                io.lflag = 1;
                return 0;
            }
        return STOP_CCT;
    }
    return 0;
}

/* ===================================================== 1407 console */

int io_console(int flag, int mod)
{
    long bs = m.BS;
    int c, wm_seen = 0;
    m.ind[IN_INC] = 0;
    if (mod == B_R) {
        m.ind[IN_INR] = 0;
        if (io.con_out) {
            fputs("1407 ENTER:\n", io.con_out);
            fflush(io.con_out);
        }
        for (;;) {
            if (bs >= m.memsize) return STOP_WRAP;
            if (m.M[bs] == (B_GRPMRK | WM)) break;
            c = io.con_in ? getc(io.con_in) : EOF;
            if (c == EOF) {
                if (bs == m.BS) return STOP_CONSEOF;
                break;
            }
            if (c == '\n') break;
            if (c == '\r') continue;
            if (flag & MD_WM) {
                if (c == '~' && !wm_seen) {
                    wm_seen = WM;
                    continue;
                }
                m.M[bs] = (unsigned char)(wm_seen | TOBCD(c));
                wm_seen = 0;
            } else
                m.M[bs] = (unsigned char)((m.M[bs] & WM) | TOBCD(c));
            bs++;
        }
        if (bs >= m.memsize) return STOP_WRAP;
        m.M[bs++] = B_GRPMRK | WM;
        m.BS = bs;
        return STOP_NONE;
    }
    if (mod == B_W) {
        while (bs < m.memsize && m.M[bs] != (B_GRPMRK | WM)) {
            int t = m.M[bs++];
            if (io.con_out) {
                if ((flag & MD_WM) && (t & WM)) fputc('~', io.con_out);
                fputc(TOCHR(t), io.con_out);
            }
        }
        if (bs >= m.memsize) return STOP_WRAP;
        bs++;
        m.BS = bs;
        if (io.con_out) {
            fputc('\n', io.con_out);
            fflush(io.con_out);
        }
        return STOP_NONE;
    }
    return STOP_INVM;
}

/* ========================================================= 729 tapes */

typedef struct {
    long len;         /* -1 = tape mark */
    unsigned char *d; /* BCD characters (6 bits) */
} TRec;

typedef struct {
    char *name;
    int attached, readonly, dirty, tapfmt;
    TRec *rec;
    long nrec, cap, pos;
} Tape;

static Tape tapes[7];

static int tape_add(Tape *t, long len, const unsigned char *d)
{
    TRec *r;
    if (t->nrec >= t->cap) {
        long n = t->cap ? t->cap * 2 : 64;
        TRec *nr = (TRec *)realloc(t->rec, n * sizeof(TRec));
        if (!nr) return -1;
        t->rec = nr;
        t->cap = n;
    }
    r = &t->rec[t->nrec++];
    r->len = len;
    r->d = 0;
    if (len > 0) {
        r->d = (unsigned char *)malloc((size_t)len);
        if (!r->d) return -1;
        memcpy(r->d, d, (size_t)len);
    }
    return 0;
}

static void tape_truncate(Tape *t, long at)
{
    while (t->nrec > at) {
        t->nrec--;
        free(t->rec[t->nrec].d);
    }
}

static int ends_with_tap(const char *n)
{
    size_t l = strlen(n);
    return l > 4 && (n[l - 4] == '.') && (n[l - 3] == 't' || n[l - 3] == 'T') &&
           (n[l - 2] == 'a' || n[l - 2] == 'A') &&
           (n[l - 1] == 'p' || n[l - 1] == 'P');
}

static unsigned long rd32(FILE *f, int *eof)
{
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4) {
        *eof = 1;
        return 0;
    }
    return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) |
           ((unsigned long)b[3] << 24);
}

int tape_attach(int unit, const char *name, int readonly)
{
    Tape *t;
    FILE *f;
    static unsigned char buf[32768];
    if (unit < 1 || unit > 6) return -1;
    t = &tapes[unit];
    t->name = (char *)malloc(strlen(name) + 1);
    if (!t->name) return -1;
    strcpy(t->name, name);
    t->tapfmt = ends_with_tap(name);
    t->readonly = readonly;
    t->attached = 1;
    t->pos = 0;
    f = fopen(name, t->tapfmt ? "rb" : "r");
    if (!f) {
        t->dirty = 1;
        return 0;
    } /* a new tape: written at the end */
    if (t->tapfmt) {
        int eof = 0;
        for (;;) {
            unsigned long l = rd32(f, &eof);
            if (eof || l == 0xFFFFFFFFUL) break;
            if (l == 0) {
                tape_add(t, -1, 0);
                continue;
            }
            l &= 0xFFFFFFL;
            if (l > sizeof buf) {
                fclose(f);
                return -1;
            }
            if (fread(buf, 1, (size_t)l, f) != l) break;
            if (l & 1) getc(f); /* pad byte */
            rd32(f, &eof);
            {
                unsigned long i;
                for (i = 0; i < l; i++) buf[i] &= CHARM;
            }
            tape_add(t, (long)l, buf);
        }
    } else {
        int c;
        long n = 0;
        for (;;) {
            c = getc(f);
            if (c == EOF || c == '\n') {
                if (c == EOF && n == 0) break;
                if (n == 1 && buf[0] == B_TAPMRK)
                    tape_add(t, -1, 0);
                else
                    tape_add(t, n, buf);
                n = 0;
                if (c == EOF) break;
                continue;
            }
            if (c == '\r') continue;
            if (n < (long)sizeof buf) buf[n++] = TOBCD(c);
        }
    }
    fclose(f);
    return 0;
}

static void wr32(FILE *f, unsigned long v)
{
    putc((int)(v & 255), f);
    putc((int)((v >> 8) & 255), f);
    putc((int)((v >> 16) & 255), f);
    putc((int)((v >> 24) & 255), f);
}

static void tape_save(Tape *t)
{
    FILE *f;
    long i, j;
    if (!t->attached || !t->dirty || t->readonly) return;
    f = fopen(t->name, t->tapfmt ? "wb" : "w");
    if (!f) {
        fprintf(stderr, "IBM1401: cannot write tape %s\n", t->name);
        return;
    }
    for (i = 0; i < t->nrec; i++) {
        TRec *r = &t->rec[i];
        if (t->tapfmt) {
            if (r->len < 0) {
                wr32(f, 0);
                continue;
            }
            wr32(f, (unsigned long)r->len);
            fwrite(r->d, 1, (size_t)r->len, f);
            if (r->len & 1) putc(0, f);
            wr32(f, (unsigned long)r->len);
        } else {
            if (r->len < 0)
                putc(TOCHR(B_TAPMRK), f);
            else
                for (j = 0; j < r->len; j++) putc(TOCHR(r->d[j]), f);
            putc('\n', f);
        }
    }
    fclose(f);
    t->dirty = 0;
}

static Tape *tape_unit(int unit)
{
    if (unit < 1 || unit > 6 || !tapes[unit].attached) return 0;
    return &tapes[unit];
}

int io_tape(int unit, int flag, int mod)
{
    Tape *t = tape_unit(unit);
    long bs = m.BS, i;
    int wm_seen = 0, c;
    static unsigned char buf[32768];
    if (!t) return STOP_MTU;
    m.ind[IN_TAP] = 0;
    m.ind[IN_END] = 0;
    if (mod == B_R) {
        TRec *r;
        long len;
        const unsigned char *d;
        static const unsigned char tm = B_TAPMRK;
        if (t->pos >= t->nrec) { /* past the end: read a tape mark */
            m.ind[IN_END] = 1;
            len = 1;
            d = &tm;
        } else {
            r = &t->rec[t->pos++];
            if (r->len < 0) {
                m.ind[IN_END] = 1;
                len = 1;
                d = &tm;
            } else {
                len = r->len;
                d = r->d;
                if (!(flag & MD_BIN) && len > 0 && (d[0] & CHARM) == B_TAPMRK)
                    m.ind[IN_END] = 1;
            }
        }
        for (i = 0; i < len; i++) {
            if (!(flag & MD_BOOT) && m.M[bs] == (B_GRPMRK | WM)) {
                m.BS = bs + 1;
                return STOP_NONE;
            }
            c = d[i] & CHARM;
            if (!(flag & MD_BIN) && c == B_ALTBLK) c = B_BLANK;
            if (flag & MD_WM) {
                if (c == B_WMCHR && !wm_seen)
                    wm_seen = WM;
                else {
                    m.M[bs] = (unsigned char)(wm_seen | c);
                    wm_seen = 0;
                }
            } else
                m.M[bs] = (unsigned char)((m.M[bs] & WM) | c);
            if (!wm_seen) bs++;
            if (bs >= m.memsize) {
                m.BS = BADADDR | (bs % MAXMEM);
                return STOP_WRAP;
            }
        }
        if (m.M[bs] != (B_GRPMRK | WM)) {
            if (flag & MD_WM)
                m.M[bs] = B_GRPMRK;
            else
                m.M[bs] = (unsigned char)((m.M[bs] & WM) | B_GRPMRK);
        }
        bs++;
        m.BS = bs;
        if (bs >= m.memsize) {
            m.BS = BADADDR | (bs % MAXMEM);
            return STOP_WRAP;
        }
        return STOP_NONE;
    }
    if (mod == B_W) {
        long n = 0;
        if (t->readonly) return STOP_MTL;
        if (m.M[bs] == (B_GRPMRK | WM)) return STOP_MTZ;
        while ((c = m.M[bs++]) != (B_GRPMRK | WM)) {
            if ((c & WM) && (flag & MD_WM) && n < (long)sizeof buf)
                buf[n++] = B_WMCHR;
            if (n < (long)sizeof buf)
                buf[n++] = (unsigned char)((((c & CHARM) == B_BLANK) &&
                                            !(flag & MD_BIN))
                                               ? B_ALTBLK
                                               : (c & CHARM));
            if (bs >= m.memsize) {
                m.BS = BADADDR | (bs % MAXMEM);
                return STOP_WRAP;
            }
        }
        tape_truncate(t, t->pos);
        if (tape_add(t, n, buf)) return STOP_IOERR;
        t->pos++;
        t->dirty = 1;
        m.BS = bs;
        return STOP_NONE;
    }
    return STOP_INVM;
}

int io_tape_func(int unit, int flag, int mod)
{
    Tape *t = tape_unit(unit);
    if (!t) return STOP_MTU;
    m.ind[IN_TAP] = 0;
    m.ind[IN_END] = 0;
    switch (mod) {
    case B_A: /* diagnostic read: skip a record */
        if (t->pos < t->nrec) {
            TRec *r = &t->rec[t->pos++];
            if (r->len < 0 ||
                (!(flag & MD_BIN) && r->len > 0 && r->d[0] == B_TAPMRK)) {
                if (r->len >= 0) m.ind[IN_END] = 1;
            }
        }
        return STOP_NONE;
    case B_B: /* backspace */
        if (t->pos > 0) t->pos--;
        return STOP_NONE;
    case B_E: /* skip and blank tape */
        if (t->readonly) return STOP_MTL;
        return STOP_NONE;
    case B_M: /* write tape mark */
        if (t->readonly) return STOP_MTL;
        tape_truncate(t, t->pos);
        if (tape_add(t, -1, 0)) return STOP_IOERR;
        t->pos++;
        t->dirty = 1;
        return STOP_NONE;
    case B_R: /* rewind */ t->pos = 0; return STOP_NONE;
    case B_U: /* rewind and unload */
        t->pos = 0;
        tape_save(t);
        t->attached = 0;
        return STOP_NONE;
    }
    return STOP_INVM;
}

void io_close(void)
{
    int i;
    for (i = 1; i <= 6; i++) tape_save(&tapes[i]);
    disk_close();
    if (io.lpt && io.lpt_asa && io.pending_asa < 0) {
        /* a final skip to channel 1 */
    }
    if (io.lpt) fflush(io.lpt);
    if (io.pun) fflush(io.pun);
}
