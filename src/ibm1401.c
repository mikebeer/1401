/*
 * ibm1401.c - IBM 1401 emulator for VM/370plus: the command
 *
 *   IBM1401 deck [deck ...] [( options [)]]
 *
 * The decks are read as one hopper by the 1402 reader.  The machine is
 * started with the LOAD key: the first card is read into 001-080 with a
 * word mark in 001 and the program begins at 001 -- so a self-loading
 * object deck (from AUTOCODR or any 1401 assembler) runs as it would
 * have on the real machine, followed by its data cards.
 *
 * Options (CMS style after "(", or on the PC each as "-option"):
 *   DATA file        more cards behind the deck (may be repeated)
 *   PRINT file|*     1403 output (default * = the terminal)
 *   ASA              print file with ASA carriage control in column 1
 *   PUNCH file       1402 punch output (default: cards are dropped)
 *   TAPEn file       mount a tape on 729 unit n (1-6); RTAPEn read-only
 *   PACKn file       mount a 1311 disk pack on drive n (0-4), SimH .dsk
 *   DISK file        the 1405 disk storage (Model 2) in this file
 *   CONSOLE file     1407 console input from a file (default terminal)
 *   CONOUT file      1407 console output to a file (default terminal)
 *   SIZE n           core storage: 1400 2000 4000 8000 12000 16000 (4K..)
 *   ENCODING A|H|S   external character encoding (default A)
 *   SWITCH letters   sense switches B-G on (e.g. SWITCH BD)
 *   HALT STOP|GO|ASK what a halt instruction does (default STOP)
 *   LIMIT n          stop after n instructions
 *   TRACE [file]     trace every instruction
 *   DUMP from to     print storage from..to at the end
 *   QUIET            no end-of-run message
 *
 * Return codes: 0 halt, 4 card reader empty, 8 instruction limit,
 * 12 machine check (invalid instruction or address), 20 file error.
 *
 * Part of the 1401 emulator for VM/370plus (mikebeer/1401).  C89.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "i1401.h"
#include "version.h"

static int ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

/* a CMS file mode: a letter and at most a digit */
static int is_fm(const char *s)
{
    return isalpha((unsigned char)s[0]) &&
           (!s[1] || (isdigit((unsigned char)s[1]) && !s[2]));
}

/* take one file name from argv; on CMS "fn ft [fm]" may be three tokens */
static char *take_file(int argc, char **argv, int *i)
{
    static char bufs[16][64];
    static int nb;
    char *b;
    if (*i >= argc) return 0;
#ifdef __CMS__
    b = bufs[nb++ & 15];
    if (!strcmp(argv[*i], "*") || strchr(argv[*i], '.') || *i + 1 >= argc ||
        argv[*i + 1][0] == '(' || argv[*i + 1][0] == ')') {
        strncpy(b, argv[(*i)++], 63);
        b[63] = 0;
        return b;
    }
    sprintf(b, "%.8s %.8s", argv[*i], argv[*i + 1]);
    *i += 2;
    if (*i < argc && is_fm(argv[*i])) {
        strcat(b, " ");
        strcat(b, argv[(*i)++]);
    }
    return b;
#else
    (void)bufs;
    (void)nb;
    (void)b;
    (void)is_fm;
    return argv[(*i)++];
#endif
}

/* a storage print: 50 positions a line, word marks as 1 below */
static void storage_dump(long from, long to)
{
    long a, k;
    if (from < 0) from = 0;
    if (to >= m.memsize) to = m.memsize - 1;
    from -= from % 50;
    for (a = from; a <= to; a += 50) {
        printf("%05ld ", a);
        for (k = a; k < a + 50 && k < m.memsize; k++) putchar(TOCHR(m.M[k]));
        printf("\n      ");
        for (k = a; k < a + 50 && k < m.memsize; k++)
            putchar((m.M[k] & WM) ? '1' : ' ');
        printf("\n");
    }
}

static void usage(void)
{
    fprintf(
        stderr,
        "IBM1401 " VERSION " BUILD " BUILD
        " - IBM 1401 emulator for VM/370plus\n"
        "usage: IBM1401 deck [deck ...] ( options\n"
        "  DATA file  PRINT file|*  ASA  PUNCH file  TAPEn file  RTAPEn file\n"
        "  PACKn file  DISK file  CONSOLE file  CONOUT file  SIZE n\n"
        "  ENCODING A|H|S  SWITCH BCDEFG  HALT STOP|GO|ASK  LIMIT n\n"
        "  TRACE [file]  DUMP from to  QUIET\n"
        "on the PC each option may be written -option\n");
}

int main(int argc, char **argv)
{
    char *decks[64];
    int ndeck = 0, i, opts = 0, quiet = 0, rc;
    int halt_mode = 0; /* 0 stop, 1 go, 2 ask */
    char *print_name = 0, *punch_name = 0, *trace_name = 0, *con_name = 0;
    char *conout_name = 0;
    long dump_from = -1, dump_to = -1;
    char *tape_name[7];
    int tape_ro[7];
    char *pack_name[5], *ramac_name = 0;
    int npack = 0;
    int reason;

    memset(tape_name, 0, sizeof tape_name);
    memset(tape_ro, 0, sizeof tape_ro);
    memset(pack_name, 0, sizeof pack_name);
    memset(&m, 0, sizeof m);
    memset(&io, 0, sizeof io);
    bcd_set_encoding('A');

    for (i = 1; i < argc;) {
        char *a = argv[i];
        char kw[16];
        if (!strcmp(a, "(")) {
            opts = 1;
            i++;
            continue;
        }
        if (!strcmp(a, ")")) {
            i++;
            continue;
        }
        if (a[0] == '(' && a[1]) {
            opts = 1;
            a++;
        } else if (a[0] == '-' && a[1]) {
            a++;
        } else if (!opts) {
            char *f;
#ifdef __CMS__
            /* CMS: the one deck is "fn [ft [fm]]"; more files by DATA */
            if (ndeck) {
                fprintf(stderr, "IBM1401: more cards with the DATA option\n");
                return 24;
            }
            {
                static char spec[40];
                int k = 0;
                spec[0] = 0;
                while (i < argc && argv[i][0] != '(' && k < 3) {
                    if (k) strcat(spec, " ");
                    strncat(spec, argv[i++], 8);
                    k++;
                }
                f = spec;
            }
#else
            f = take_file(argc, argv, &i);
#endif
            if (ndeck < 64) decks[ndeck++] = f;
            continue;
        }
        strncpy(kw, a, 15);
        kw[15] = 0;
        {
            char *p;
            for (p = kw; *p; p++) *p = (char)toupper((unsigned char)*p);
        }
        i++;
        if (ieq(kw, "PRINT") || ieq(kw, "PR"))
            print_name = take_file(argc, argv, &i);
        else if (ieq(kw, "PUNCH") || ieq(kw, "PU"))
            punch_name = take_file(argc, argv, &i);
        else if (ieq(kw, "ASA") || ieq(kw, "CC"))
            io.lpt_asa = 1;
        else if (ieq(kw, "DATA") || ieq(kw, "CARDS")) {
            char *f = take_file(argc, argv, &i);
            if (f && ndeck < 64) decks[ndeck++] = f;
        } else if (ieq(kw, "CONSOLE"))
            con_name = take_file(argc, argv, &i);
        else if (ieq(kw, "CONOUT"))
            conout_name = take_file(argc, argv, &i);
        else if ((!strncmp(kw, "TAPE", 4) && strlen(kw) == 5) ||
                 (!strncmp(kw, "RTAPE", 5) && strlen(kw) == 6)) {
            int ro = kw[0] == 'R';
            int u = kw[ro ? 5 : 4] - '0';
            if (u < 1 || u > 6) {
                fprintf(stderr, "IBM1401: bad tape unit %s\n", kw);
                return 24;
            }
            tape_name[u] = take_file(argc, argv, &i);
            tape_ro[u] = ro;
        } else if (!strncmp(kw, "PACK", 4) && strlen(kw) == 5) {
            int u = kw[4] - '0';
            if (u < 0 || u > 4 || ramac_name) {
                fprintf(stderr, "IBM1401: bad disk pack %s\n", kw);
                return 24;
            }
            pack_name[u] = take_file(argc, argv, &i);
            npack++;
        } else if (ieq(kw, "DISK")) {
            if (npack) {
                fprintf(stderr, "IBM1401: a 1401 has a 1311 or a 1405\n");
                return 24;
            }
            ramac_name = take_file(argc, argv, &i);
        } else if (ieq(kw, "SIZE") && i < argc) {
            char *s = argv[i++];
            long n = atol(s);
            if (toupper((unsigned char)s[strlen(s) - 1]) == 'K') n *= 1000;
            if (n != 1400 && n != 2000 && n != 4000 && n != 8000 &&
                n != 12000 && n != 16000) {
                fprintf(stderr, "IBM1401: invalid SIZE %s\n", s);
                return 24;
            }
            m.memsize = n;
        } else if ((ieq(kw, "ENCODING") || ieq(kw, "ENC")) && i < argc) {
            if (bcd_set_encoding(argv[i++][0])) {
                fprintf(stderr, "IBM1401: encoding must be A, H or S\n");
                return 24;
            }
        } else if ((ieq(kw, "SWITCH") || ieq(kw, "SW")) && i < argc) {
            char *s = argv[i++];
            for (; *s; s++) {
                int c = toupper((unsigned char)*s);
                if (c >= 'B' && c <= 'G') m.ind[IN_SSB + (c - 'B')] = 1;
            }
        } else if (ieq(kw, "HALT") && i < argc) {
            char *s = argv[i++];
            halt_mode = ieq(s, "GO") ? 1 : ieq(s, "ASK") ? 2 : 0;
        } else if (ieq(kw, "LIMIT") && i < argc)
            m.ilimit = (unsigned long)atol(argv[i++]);
        else if (ieq(kw, "TRACE")) {
            m.trace = 1;
            if (i < argc && argv[i][0] != '-' && argv[i][0] != ')')
                trace_name = take_file(argc, argv, &i);
        } else if (ieq(kw, "QUIET"))
            quiet = 1;
        else if (ieq(kw, "DUMP") && i + 1 < argc) {
            dump_from = atol(argv[i++]);
            dump_to = atol(argv[i++]);
        } else if (ieq(kw, "HELP") || ieq(kw, "?")) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "IBM1401: unknown option %s\n", kw);
            usage();
            return 24;
        }
    }
    if (!ndeck) {
        usage();
        return 24;
    }
#ifdef __CMS__
    /* "fn" alone is the object deck fn DECK (any disk) that AUTOCODR writes */
    for (i = 0; i < ndeck; i++)
        if (!strchr(decks[i], ' ') && !strchr(decks[i], '.') &&
            strcmp(decks[i], "-")) {
            char *d = (char *)malloc(32);
            if (d) {
                sprintf(d, "%.8s DECK", decks[i]);
                decks[i] = d;
            }
        }
#endif

    /* devices */
    io.rdr_names = decks;
    io.rdr_n = ndeck;
    io.rdr_i = 0;
    io.rdr_peek = EOF;
    if (!print_name || !strcmp(print_name, "*"))
        io.lpt = stdout;
    else if (!(io.lpt = fopen(print_name, "w"))) {
        fprintf(stderr, "IBM1401: cannot open print file %s\n", print_name);
        return 20;
    }
    if (punch_name && !(io.pun = fopen(punch_name, "w"))) {
        fprintf(stderr, "IBM1401: cannot open punch file %s\n", punch_name);
        return 20;
    }
    io.con_in = stdin;
    io.con_out = stdout;
    if (con_name && !(io.con_in = fopen(con_name, "r"))) {
        fprintf(stderr, "IBM1401: cannot open console file %s\n", con_name);
        return 20;
    }
    if (conout_name && !(io.con_out = fopen(conout_name, "w"))) {
        fprintf(stderr, "IBM1401: cannot open console file %s\n", conout_name);
        return 20;
    }
    m.tracef = stderr;
    if (trace_name && !(m.tracef = fopen(trace_name, "w"))) {
        fprintf(stderr, "IBM1401: cannot open trace file %s\n", trace_name);
        return 20;
    }
    io.cctlnt = 66;
    io.cct[0] = 03; /* channel 1 on line 1 */
    io.pending_asa = 1;
    for (i = 1; i <= 6; i++)
        if (tape_name[i] && tape_attach(i, tape_name[i], tape_ro[i])) {
            fprintf(stderr, "IBM1401: cannot mount tape %s\n", tape_name[i]);
            return 20;
        }

    for (i = 0; i <= 4; i++)
        if (pack_name[i] && disk_attach_pack(i, pack_name[i])) {
            fprintf(stderr, "IBM1401: cannot mount disk pack %s\n",
                    pack_name[i]);
            return 20;
        }
    if (ramac_name && disk_attach_ramac(ramac_name)) {
        fprintf(stderr, "IBM1401: cannot use disk file %s\n", ramac_name);
        return 20;
    }

    cpu_reset();
    if (rdr_open_next()) return 20;
    reason = io_load();
    while (!reason) {
        reason = cpu_run();
        if (reason == STOP_HALT && halt_mode) {
            if (halt_mode == 2) {
                char line[80];
                fflush(io.lpt);
                fprintf(stderr,
                        "IBM1401: HALT AT %ld - ENTER TO START, STOP TO END\n",
                        m.saved_IS);
                if (!fgets(line, sizeof line, stdin) ||
                    toupper((unsigned char)line[0]) == 'S')
                    break;
            }
            m.IS = m.saved_IS;
            reason = STOP_NONE;
        }
    }
    io_close();
    if (dump_from >= 0) storage_dump(dump_from, dump_to);

    switch (reason) {
    case STOP_HALT: rc = 0; break;
    case STOP_NOCARD: rc = 4; break;
    case STOP_LIMIT: rc = 8; break;
    case STOP_IOERR: rc = 20; break;
    default: rc = 12; break;
    }
    if (!quiet) {
        fflush(io.lpt);
        fprintf(stderr,
                "IBM1401: %s AT %ld, %lu INSTRUCTIONS, %ld CARDS READ, "
                "%ld LINES PRINTED, %ld CARDS PUNCHED\n",
                reason == STOP_HALT ? "HALT" : stop_text(reason),
                reason == STOP_HALT ? m.saved_IS : m.saved_IS, m.icount,
                io.cards_read, io.lines_printed, io.cards_punched);
    }
    if (io.lpt && io.lpt != stdout) fclose(io.lpt);
    if (io.pun) fclose(io.pun);
    if (io.con_out && io.con_out != stdout) fclose(io.con_out);
    return rc;
}
