/*
 * disk.c - IBM 1401 disk storage: the 1311 disk drive or the 1405 RAMAC
 *
 * A 1401 has one or the other; both answer to %F in the A-address.
 *
 * 1311 (PACKn options, drives 0-4) behaves as SimH i1401's DP device:
 *   100 cylinders x 10 tracks x 20 sectors; a sector holds a 6-character
 *   address and 100 characters (move mode) or 90 characters with word
 *   marks (load mode).  The B-address is the disk control field: drive
 *   character, 6-digit sector address, 3-digit sector count.  %F0 seek
 *   (with a direct seek form), %F1 read/write sectors, %F3 write check,
 *   %F5 sector count overlay, %F6 track with addresses.  An address of
 *   000000 on the pack is the sector's own number.  The file is SimH's
 *   .dsk format: 106 bytes per sector, each a BCD character with the word
 *   mark in bit 0x40, 20,000 sectors.
 *
 * 1405 (DISK option, Model 2: 100,000 records of 200 characters in
 *   20,000 tracks of 5 records) follows the Reference Manual, pp. 63-68:
 *   the B-address is an 8-character record address (access arm 0 or 1,
 *   six-digit record number, constant 0), then a group mark with a word
 *   mark, then the data, then a group mark with a word mark.  %F0 seek,
 *   %F1 single record (200 characters, 176 with word marks), %F2 full
 *   track (1000, 880), %F3 write check of the last operation.  Read and
 *   write need the arm on the record's track (seek), else the unequal
 *   address indicator.  Reading a record in the other mode than it was
 *   written sets the parity indicator.  The file holds the records that
 *   were ever written: 204 bytes each, the record number in 3 bytes (high
 *   first), the mode (1 move, 2 load), 200 bytes BCD with word mark 0x40.
 *
 * Indicators: V parity / write check, W wrong length, X unequal address,
 * Y any disk error, N access inoperable.
 *
 * Part of the 1401 emulator for VM/370plus (mikebeer/1401).  C89.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i1401.h"

#define IN_DPW 025 /* V parity, write check */
#define IN_LNG 026 /* W wrong length record */
#define IN_UNA 027 /* X unequal address compare */
#define IN_DSK 030 /* Y any disk error */
#define IN_ACC 045 /* N access inoperable */

#define GMWM       (WM | B_GRPMRK)
#define ADDR_OK(x) ((x) >= 0 && (x) < m.memsize)

static int disk_model; /* 0 none, 1311, 1405 */

static void clear_ind(void)
{
    m.ind[IN_DPW] = m.ind[IN_LNG] = m.ind[IN_UNA] = 0;
    m.ind[IN_DSK] = m.ind[IN_ACC] = 0;
}

static void err(int ind)
{
    m.ind[ind] = 1;
    m.ind[IN_DSK] = 1;
}

/* a digit character as a number, -1 if not a digit (0 is 8-2) */
static int dig(int c)
{
    c &= DIGIT;
    if (c == 0 || c > B_ZERO) return -1;
    return c == B_ZERO ? 0 : c;
}

/* a field of n digits as a number, -1 if a character is not a digit */
static long num(long at, int n)
{
    long r = 0;
    int d;
    for (; n > 0; n--, at++) {
        if ((d = dig(m.M[at])) < 0) return -1;
        r = r * 10 + d;
    }
    return r;
}

/* store v as n digits ending at at+n-1; load mode clears word marks */
static void put_num(long at, int n, long v, int wm_clear)
{
    int d;
    for (; n > 0; n--) {
        d = (int)(v % 10);
        v /= 10;
        m.M[at + n - 1] =
            (unsigned char)((wm_clear ? 0 : (m.M[at + n - 1] & WM)) |
                            (d ? d : B_ZERO));
    }
}

/* ================================================================ 1311 */

#define DP_DRIVES 5
#define DP_ADDR   6
#define DP_DATA   100
#define DP_SECT   (DP_ADDR + DP_DATA)
#define DP_NSEC   20
#define DP_NTRK   10
#define DP_NCYL   100
#define DP_CYLSEC (DP_NTRK * DP_NSEC)
#define DP_TOTSEC (DP_NCYL * DP_CYLSEC)
#define DP_BYTES  ((long)DP_TOTSEC * DP_SECT)

typedef struct {
    char *name;
    unsigned char *buf;
    long hw; /* bytes to write back */
    int cyl;
    int dirty;
} Pack;

static Pack pack[DP_DRIVES];
static int dp_lastf;

int disk_attach_pack(int drive, const char *name)
{
    Pack *p = &pack[drive];
    FILE *f;
    if (disk_model == 1405) return -1;
    disk_model = 1311;
    p->buf = (unsigned char *)calloc((size_t)DP_BYTES, 1);
    if (!p->buf) return -1;
    p->name = (char *)name;
    f = fopen(name, "rb");
    if (f) {
        p->hw = (long)fread(p->buf, 1, (size_t)DP_BYTES, f);
        fclose(f);
    }
    return 0;
}

static unsigned char *sector(Pack *p, long psec)
{
    return p->buf + (psec % DP_TOTSEC) * DP_SECT;
}

static int zero_addr(const unsigned char *ap)
{
    int i;
    for (i = 0; i < DP_ADDR; i++)
        if (ap[i] & CHARM) return 0;
    return 1;
}

static int same_addr(const unsigned char *ap, long dcf)
{
    int i;
    for (i = 0; i < DP_ADDR; i++)
        if ((m.M[dcf + 1 + i] & CHARM) != (ap[i] & CHARM)) return 0;
    return 1;
}

static void touch(Pack *p, long da)
{
    if (da + 1 > p->hw) p->hw = da + 1;
    p->dirty = 1;
}

/* the sector for an address: the one under the head with that address
   (or address 000000), else any on the track with it */
static long dp_find(Pack *p, long sec, long dcf)
{
    long psec = (long)p->cyl * DP_CYLSEC + sec % DP_CYLSEC;
    int i;
    if (zero_addr(sector(p, psec)) || same_addr(sector(p, psec), dcf))
        return psec;
    psec -= psec % DP_NSEC;
    for (i = 0; i < DP_NSEC; i++, psec++)
        if (!zero_addr(sector(p, psec)) && same_addr(sector(p, psec), dcf))
            return psec;
    err(IN_UNA);
    return -1;
}

/* the next sector of a multi-sector operation: not across a cylinder */
static int dp_next(Pack *p, long psec, long dcf)
{
    if (psec % DP_CYLSEC) {
        if (zero_addr(sector(p, psec)) || same_addr(sector(p, psec), dcf))
            return 0;
    }
    err(IN_UNA);
    return -1;
}

/* move an address between disk and storage at B; -1 on an error */
static int dp_rdadr(Pack *p, long psec, int load, int check)
{
    unsigned char *ap = sector(p, psec);
    int zad = zero_addr(ap), i, ac;
    long s = psec, pw = 100000L;
    for (i = 0; i < DP_ADDR; i++, pw /= 10) {
        if (m.M[m.BS] == GMWM) {
            err(IN_LNG);
            return -1;
        }
        if (zad) {
            ac = (int)(s / pw);
            s %= pw;
            if (!ac) ac = B_ZERO;
        } else
            ac = ap[i];
        if (check) {
            if (!zad && (load ? m.M[m.BS] != (ac & (WM | CHARM))
                              : (m.M[m.BS] & CHARM) != (ac & CHARM))) {
                err(IN_DPW);
                return -1;
            }
        } else if (load)
            m.M[m.BS] = (unsigned char)(ac & CHARM);
        else
            m.M[m.BS] = (unsigned char)((m.M[m.BS] & WM) | (ac & CHARM));
        if (!ADDR_OK(m.BS + 1)) return -2;
        m.BS++;
    }
    return 0;
}

static int dp_rdsec(Pack *p, long psec, int load, int check)
{
    unsigned char *ap = sector(p, psec) + DP_ADDR;
    int i, lim = load ? DP_DATA - 10 : DP_DATA;
    for (i = 0; i < lim; i++, ap++) {
        if (m.M[m.BS] == GMWM) {
            err(IN_LNG);
            return -1;
        }
        if (check) {
            if (load ? m.M[m.BS] != (*ap & (WM | CHARM))
                     : (m.M[m.BS] & CHARM) != (*ap & CHARM)) {
                err(IN_DPW);
                return -1;
            }
        } else if (load)
            m.M[m.BS] = (unsigned char)(*ap & (WM | CHARM));
        else
            m.M[m.BS] = (unsigned char)((m.M[m.BS] & WM) | (*ap & CHARM));
        if (!ADDR_OK(m.BS + 1)) return -2;
        m.BS++;
    }
    return 0;
}

/* write n characters from B at byte da; a group mark with a word mark
   fills the rest of the sector with blanks (wrong length) */
static int dp_write(Pack *p, long da, int n, int fill, int load)
{
    int i;
    for (i = 0; i < n; i++, da++) {
        if (m.M[m.BS] == GMWM) {
            for (; i < fill; i++, da++) {
                p->buf[da] = B_BLANK;
                touch(p, da);
            }
            err(IN_LNG);
            return -1;
        }
        p->buf[da] = (unsigned char)(m.M[m.BS] & (load ? WM | CHARM : CHARM));
        touch(p, da);
        if (!ADDR_OK(m.BS + 1)) return -2;
        m.BS++;
    }
    return 0;
}

static long dp_track(int drv, long sec)
{
    return (long)drv * DP_TOTSEC + (long)pack[drv].cyl * DP_CYLSEC +
           ((sec / DP_NSEC) % DP_NTRK) * DP_NSEC;
}

static int disk_1311(int fnc, int load, int mod)
{
    long dcf = m.BS, sec, psec, cnt, diff;
    int drv, check = 0, last, r = 0;
    Pack *p;
    clear_ind();
    m.AS = dcf + 6;
    m.BS = dcf + 6;
    if (!ADDR_OK(m.BS)) return STOP_WRAP;
    if (m.M[dcf] & 040) /* drive from the cylinder digit */
        drv = m.M[dcf + 2] & 016;
    else
        drv = m.M[dcf] & DIGIT;
    if (drv == 0 || (drv & 1) || drv > B_ZERO) return STOP_DISK;
    drv = drv == B_ZERO ? 0 : drv >> 1;
    p = &pack[drv];
    if (!p->buf) {
        err(IN_ACC);
        return STOP_NONE;
    }
    if (fnc == 0 && (m.M[dcf + 5] & 013) == 013) { /* direct seek */
        diff = num(dcf + 1, 4);
        if (diff < 0) return STOP_DISK;
        diff >>= 1;
        if ((m.M[dcf + 4] & ZONE) == 040) diff = -diff;
        p->cyl += (int)diff;
        if (p->cyl < 0) p->cyl = 0;
        if (p->cyl >= DP_NCYL) {
            p->cyl = 0;
            return STOP_DISK;
        }
        return STOP_NONE;
    }
    sec = num(dcf + 1, 6);
    if (sec < 0 || sec >= (long)DP_DRIVES * DP_TOTSEC) return STOP_DISK;
    if (fnc == 0) {
        p->cyl = (int)((sec / DP_CYLSEC) % DP_NCYL);
        return STOP_NONE;
    }
    m.BS = dcf + 10;
    if (!ADDR_OK(m.BS)) return STOP_WRAP;
    cnt = num(dcf + 7, 3);
    if (cnt < 0) return STOP_DISK;
    if (cnt == 0) cnt = 1000;
    if (fnc >= 10) return STOP_DISK;
    if (mod == B_W) {
        if (fnc == 3) {
            check = 1;
            fnc = dp_lastf;
        } else {
            dp_lastf = fnc;
            fnc += 10;
        }
    } else if (mod == B_R)
        dp_lastf = fnc;
    else
        return STOP_INVM;

    switch (fnc) {
    case 5: /* read, sector count overlay */
        m.BS = dcf + 7;
        /* fall through */
    case 1: /* read sectors */
        if ((psec = dp_find(p, sec, dcf)) < 0) return STOP_NONE;
        for (;;) {
            last = --cnt == 0;
            put_num(dcf + 7, 3, cnt, 1);
            if ((r = dp_rdsec(p, psec, load, check)) != 0) break;
            if ((cnt = num(dcf + 7, 3)) < 0) return STOP_DISK;
            if (cnt == 0) cnt = 1000;
            if (last) break;
            sec++;
            psec++;
            put_num(dcf + 1, 6, sec, load);
            if ((r = dp_next(p, psec, dcf)) != 0) break;
        }
        break;
    case 6: /* read track with addresses */
        m.AS = dcf + 9;
        psec = dp_track(drv, sec);
        for (;;) {
            last = --cnt == 0;
            put_num(dcf + 7, 3, cnt, 1);
            if ((r = dp_rdadr(p, psec, load, check)) != 0) break;
            if ((r = dp_rdsec(p, psec, load, check)) != 0) break;
            if ((cnt = num(dcf + 7, 3)) < 0) return STOP_DISK;
            if (cnt == 0) cnt = 1000;
            if (last) break;
            psec = dp_track(drv, sec) + (psec + 1) % DP_NSEC;
        }
        break;
    case 15: /* write, sector count overlay */
        m.BS = dcf + 7;
        /* fall through */
    case 11: /* write sectors */
        if ((psec = dp_find(p, sec, dcf)) < 0) return STOP_NONE;
        for (;;) {
            last = --cnt == 0;
            put_num(dcf + 7, 3, cnt, 1);
            if ((r = dp_write(p, (psec % DP_TOTSEC) * DP_SECT + DP_ADDR,
                              load ? DP_DATA - 10 : DP_DATA, DP_DATA - 0,
                              load)) != 0)
                break;
            if (last) break;
            sec++;
            psec++;
            put_num(dcf + 1, 6, sec, load);
            if ((r = dp_next(p, psec, dcf)) != 0) break;
        }
        break;
    case 16: /* write track with addresses */
        m.AS = dcf + 9;
        psec = dp_track(drv, sec);
        for (;;) {
            long da = (psec % DP_TOTSEC) * DP_SECT;
            last = --cnt == 0;
            put_num(dcf + 7, 3, cnt, 1);
            if ((r = dp_write(p, da, DP_ADDR, DP_SECT, load)) != 0) break;
            if ((r = dp_write(p, da + DP_ADDR, load ? DP_DATA - 10 : DP_DATA,
                              DP_DATA, load)) != 0)
                break;
            if (last) break;
            psec = dp_track(drv, sec) + (psec + 1) % DP_NSEC;
        }
        break;
    default: return STOP_DISK;
    }
    if (r == -2) return STOP_WRAP;
    if (r == 0) {
        m.BS++;
        if (!ADDR_OK(m.BS)) return STOP_WRAP;
        if (m.M[m.BS - 1] != GMWM) err(IN_LNG);
    }
    return STOP_NONE;
}

/* ================================================================ 1405 */

#define RM_RECS  100000L
#define RM_TRKS  (RM_RECS / 5)
#define RM_DATA  200
#define RM_LDATA 176
#define RM_SLOT  (1 + RM_DATA) /* mode, data */

static char *rm_name;
static unsigned char **rm_trk; /* RM_TRKS tracks of 5 slots, on demand */
static long rm_arm[2] = {-1, -1};
static long rm_last = -1; /* last record addressed */
static int rm_lastfull, rm_dirty;

static unsigned char *rm_slot(long rec, int create)
{
    long t = rec / 5;
    if (!rm_trk[t]) {
        if (!create) return 0;
        rm_trk[t] = (unsigned char *)calloc(5 * RM_SLOT, 1);
        if (!rm_trk[t]) return 0;
    }
    return rm_trk[t] + (rec % 5) * RM_SLOT;
}

int disk_attach_ramac(const char *name)
{
    FILE *f;
    unsigned char rb[4 + RM_DATA], *s;
    long rec;
    if (disk_model == 1311) return -1;
    disk_model = 1405;
    rm_name = (char *)name;
    rm_trk = (unsigned char **)calloc((size_t)RM_TRKS, sizeof *rm_trk);
    if (!rm_trk) return -1;
    f = fopen(name, "rb");
    if (!f) return 0; /* a new disk */
    while (fread(rb, 1, sizeof rb, f) == sizeof rb) {
        rec = ((long)rb[0] << 16) | ((long)rb[1] << 8) | rb[2];
        if (rec >= RM_RECS || (rb[3] != 1 && rb[3] != 2)) continue;
        if (!(s = rm_slot(rec, 1))) {
            fclose(f);
            return -1;
        }
        s[0] = rb[3];
        memcpy(s + 1, rb + 4, RM_DATA);
    }
    fclose(f);
    return 0;
}

/* the record address at B: arm, record number; -1 if not valid */
static long rm_addr(long b, int *arm)
{
    long rec;
    int a = dig(m.M[b]);
    if (a < 0 || a > 1) return -1;
    rec = num(b + 1, 6);
    if (rec < 0 || rec >= RM_RECS || dig(m.M[b + 7]) != 0) return -1;
    *arm = a;
    return rec;
}

static int disk_1405(int fnc, int load, int mod)
{
    long b = m.BS, rec, p, recs[5];
    int arm, nrec, n, i, k, full, check = 0, write = 0, short_gm = 0;
    unsigned char *s;

    m.ind[IN_ACC] = 0;
    if (!ADDR_OK(b + 9)) return STOP_WRAP;
    m.AS = b + 1;
    if (fnc == 0) { /* seek */
        m.BS = b + 8;
        if ((rec = rm_addr(b, &arm)) < 0) {
            err(IN_ACC);
            return STOP_NONE;
        }
        rm_arm[arm] = rec / 5;
        return STOP_NONE;
    }
    clear_ind();
    if (mod != B_R && mod != B_W) return STOP_INVM;
    if (fnc == 3) {
        if (mod != B_W) return STOP_INVM;
        check = 1;
        full = rm_lastfull;
    } else if (fnc == 1 || fnc == 2) {
        full = fnc == 2;
        write = mod == B_W;
    } else
        return STOP_DISK;
    n = load ? RM_LDATA : RM_DATA;
    m.BS = b + 9 + (long)(full ? 5 : 1) * n + 1;
    if (!ADDR_OK(m.BS)) return STOP_WRAP;
    if ((rec = rm_addr(b, &arm)) < 0) {
        err(IN_ACC);
        return STOP_NONE;
    }
    if (m.M[b + 8] != GMWM) {
        err(IN_LNG);
        return STOP_NONE;
    }
    if (rm_arm[arm] != rec / 5 || (check && rec != rm_last)) {
        err(IN_UNA);
        return STOP_NONE;
    }
    if (!check) {
        rm_last = rec;
        rm_lastfull = full;
    }
    nrec = full ? 5 : 1;
    for (k = 0; k < nrec; k++) recs[k] = rec - rec % 5 + (rec % 5 + k) % 5;

    p = b + 9;
    for (k = 0; k < nrec; k++) {
        s = rm_slot(recs[k], write);
        if (write && !s) return STOP_IOERR;
        if (!write && s && s[0] && s[0] != (load ? 2 : 1)) err(IN_DPW);
        if (write) {
            s[0] = (unsigned char)(load ? 2 : 1);
            memset(s + 1, 0, RM_DATA);
            rm_dirty = 1;
        }
        for (i = 0; i < n; i++, p++) {
            unsigned char d = s && s[0] ? s[1 + i] : 0;
            if (m.M[p] == GMWM && !short_gm) {
                err(IN_LNG);
                if (!write || load) {
                    if (write) /* the rest of the record is blank */
                        for (; i < n; i++) s[1 + i] = B_BLANK;
                    return STOP_NONE;
                }
                short_gm = 1; /* move mode: core keeps filling the record */
            }
            if (write)
                s[1 + i] =
                    (unsigned char)(m.M[p] & (load ? WM | CHARM : CHARM));
            else if (check) {
                if (load ? m.M[p] != d : (m.M[p] & CHARM) != (d & CHARM)) {
                    err(IN_DPW);
                    return STOP_NONE;
                }
            } else if (load)
                m.M[p] = (unsigned char)(d & (WM | CHARM));
            else
                m.M[p] = (unsigned char)((m.M[p] & WM) | (d & CHARM));
        }
    }
    if (m.M[p] != GMWM && !short_gm) err(IN_LNG);
    return STOP_NONE;
}

/* ============================================================ common */

int io_disk(int fnc, int flag, int mod)
{
    if (disk_model == 1311) return disk_1311(fnc, flag & MD_WM, mod);
    if (disk_model == 1405) return disk_1405(fnc, flag & MD_WM, mod);
    return STOP_NXD;
}

int disk_close(void)
{
    FILE *f;
    int i, k, rc = 0;
    long t;
    unsigned char rb[4];
    for (i = 0; i < DP_DRIVES; i++) {
        Pack *p = &pack[i];
        if (!p->buf || !p->dirty) continue;
        if (!(f = fopen(p->name, "wb")) ||
            fwrite(p->buf, 1, (size_t)p->hw, f) != (size_t)p->hw) {
            fprintf(stderr, "IBM1401: cannot write disk pack %s\n", p->name);
            rc = -1;
        }
        if (f) fclose(f);
        p->dirty = 0;
    }
    if (disk_model == 1405 && rm_dirty) {
        if (!(f = fopen(rm_name, "wb"))) {
            fprintf(stderr, "IBM1401: cannot write disk file %s\n", rm_name);
            return -1;
        }
        for (t = 0; t < RM_TRKS; t++) {
            if (!rm_trk[t]) continue;
            for (k = 0; k < 5; k++) {
                unsigned char *s = rm_trk[t] + k * RM_SLOT;
                long rec = t * 5 + k;
                if (!s[0]) continue;
                rb[0] = (unsigned char)(rec >> 16);
                rb[1] = (unsigned char)(rec >> 8);
                rb[2] = (unsigned char)rec;
                rb[3] = s[0];
                fwrite(rb, 1, 4, f);
                fwrite(s + 1, 1, RM_DATA, f);
            }
        }
        if (ferror(f)) rc = -1;
        fclose(f);
        rm_dirty = 0;
    }
    return rc;
}
