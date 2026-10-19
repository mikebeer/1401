/*
 * i1401.h - the IBM 1401 machine: storage, registers, indicators, devices
 *
 * Part of the 1401 emulator for VM/370plus (mikebeer/1401).  C89.
 */
#ifndef I1401_H
#define I1401_H

#include <stdio.h>
#include "bcd.h"

#define MAXMEM 16000

/* An address register holds a binary address; BADADDR marks an address
   built from blanks or invalid characters (the hardware "blank" state),
   and the index register tag sits above the address while decoding. */
#define BADADDR   0x1000000L
#define V_INDEX   16
#define M_INDEX   0x7F
#define INDEXMASK 0xFFFFFL /* address plus index tag */
#define ADDRMASK  0x3FFFL

/* stop reasons */
enum {
    STOP_NONE = 0,
    STOP_HALT,    /* H instruction */
    STOP_NOWM,    /* no word mark under the op code */
    STOP_NXI,     /* invalid / not installed op code */
    STOP_INVA,    /* invalid A-address */
    STOP_INVB,    /* invalid B-address */
    STOP_INVL,    /* invalid length */
    STOP_INVM,    /* invalid d-modifier */
    STOP_INVBR,   /* branch to invalid address */
    STOP_WRAP,    /* address wrapped through storage */
    STOP_NOCARD,  /* card reader empty */
    STOP_CCT,     /* carriage runaway (no punch in channel) */
    STOP_NXD,     /* device not installed */
    STOP_MTU,     /* invalid tape unit / not attached */
    STOP_MTL,     /* tape write protected */
    STOP_MTZ,     /* tape write of zero length record */
    STOP_LIMIT,   /* instruction limit reached */
    STOP_IOERR,   /* host file error */
    STOP_CONSEOF, /* console input ended */
    STOP_MAX
};

/* indicator numbers are the BCD value of the d-character that tests them */
#define IN_UNC   000 /* blank: unconditional */
#define IN_CC9   011 /* 9: carriage channel 9 */
#define IN_CC12  014 /* @: carriage channel 12 */
#define IN_UNQ   021 /* /: unequal compare */
#define IN_EQU   022 /* S: equal compare */
#define IN_LOW   023 /* T: low compare */
#define IN_HGH   024 /* U: high compare */
#define IN_OVF   031 /* Z: overflow */
#define IN_LPT   032 /* rec mark: printer error */
#define IN_PRO   034 /* %: process check */
#define IN_END   042 /* K: end of reel */
#define IN_TAP   043 /* L: tape error */
#define IN_INR   050 /* Q: inquiry request */
#define IN_PCB   051 /* R: printer carriage busy */
#define IN_PNCH  052 /* !: punch error */
#define IN_INC   054 /* *: inquiry clear */
#define IN_READ  072 /* ?: reader error */
#define IN_LST   061 /* A: last card */
#define IN_SSB   062 /* B..G: sense switches */
#define IN_SSG   067
#define IN_PRBSY 047 /* P: printer busy */

/* special feature options (all on by default) */
#define F_XSA 0x01 /* indexing and store address register */
#define F_MDV 0x02 /* multiply / divide */
#define F_HLE 0x04 /* high-low-equal compare */
#define F_BBE 0x08 /* branch if bit equal */
#define F_EPE 0x10 /* expanded print edit */
#define F_MR  0x20 /* move record */
#define F_MA  0x40 /* modify address (above 4000) */

typedef struct {
    unsigned char M[MAXMEM];
    long memsize;
    long IS;     /* I-address register */
    long AS, BS; /* A- and B-address registers */
    int D;       /* last d-character / A-register */
    int ind[64]; /* indicators */
    int features;
    int hb_pend;          /* halt and branch: branch on restart */
    long saved_IS;        /* start of the current instruction */
    unsigned long icount; /* instructions executed */
    unsigned long ilimit; /* 0 = no limit */
    int trace;
    FILE *tracef;
} Machine;

extern Machine m;

/* cpu.c */
void cpu_reset(void);
int cpu_run(void); /* returns a STOP_ code */
const char *stop_text(int reason);
void store_addr(long at, long addr); /* 3-char address ending at 'at' */
long addr_of(int h, int t, int u);   /* decode a 3-char address */
void cpu_disasm(FILE *f, long is, int len);

/* io.c - devices */
typedef struct {
    /* 1402 reader: a list of input files read as one hopper */
    char **rdr_names;
    int rdr_n, rdr_i;
    FILE *rdr;
    int rdr_eof;
    int rdr_peek; /* lookahead char or EOF */
    long cards_read;
    /* 1402 punch */
    FILE *pun;
    long cards_punched;
    /* 1403 printer */
    FILE *lpt;
    int lpt_asa;
    long lines_printed;
    int cct[132], cctlnt, cctptr, lines, lflag;
    int pending_asa; /* ASA char for the next line */
    /* 1407 console */
    FILE *con_in, *con_out;
} IOState;

extern IOState io;

int io_read_card(int ilnt, int mod);
int io_punch_card(int ilnt, int mod);
int io_write_line(int ilnt, int mod);
int io_select_stacker(int mod);
int io_carriage(int mod);
int io_console(int flag, int mod);
int io_tape(int unit, int flag, int mod);
int io_tape_func(int unit, int flag, int mod);
int io_disk(int flag, int mod);
int io_load(void); /* the LOAD key: boot from cards */
void io_close(void);
int tape_attach(int unit, const char *name, int readonly);
int rdr_open_next(void);

#define MD_WM   1 /* load mode (L): word marks travel */
#define MD_BIN  2 /* binary (odd parity) tape */
#define MD_BOOT 4

#endif
