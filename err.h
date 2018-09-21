#ifndef ERR_H
#define ERR_H

#include <stdlib.h>

#ifdef DEBUG

#define DB true;

char p_name[100], deb_buff[500];
int buff_pos;

#include <zconf.h>
#include <stdbool.h>

#define SET_P_NAME(fmt, ...) {\
int tmplen = sprintf(p_name, fmt, ##__VA_ARGS__);\
sprintf(p_name + tmplen, " (%d)", getpid());}
#define FLUSH(cond) {if (cond) {fprintf(stderr, "%s", deb_buff); buff_pos = 0;}}
#define PRINT(cond, fmt, ...) {\
    if (cond) {buff_pos += sprintf(deb_buff + buff_pos, fmt, ##__VA_ARGS__);}}
#define PREF(cond) {PRINT(cond, "%s: ", p_name);}
#define PRINTP(cond, fmt, ...) {PREF(cond) PRINT(cond, fmt, ##__VA_ARGS__);}
#define PRINTLN(cond, fmt, ...) {PRINTP(cond, fmt, ##__VA_ARGS__) PRINT(cond, "\n");}
#define PRINTLNF(cond, fmt, ...) {PRINTLN(cond, fmt, ##__VA_ARGS__) FLUSH(cond);}
#define DEB(cond, instr) instr;

#else

#define DB false

#define SET_P_NAME(name) ;
#define FLUSH(cond) ;
#define PRINT(cond, fmt, ...) ;
#define PREF(cond) ;
#define PRINTP(cond, fmt, ...) ;
#define PRINTLN(cond, fmt, ...) ;
#define PRINTLNF(cond, fmt, ...) ;
#define DEB(cond, instr) ;

#endif

enum exiting {EXIT, NOEXIT};

/* wypisuje informacje o błędnym zakończeniu funkcji systemowej 
i kończy działanie */
extern void syserr(enum exiting mode, const char *fmt, ...);

/* wypisuje informacje o błędzie i kończy działanie */
extern void fatal(enum exiting mode, const char *fmt, ...);

#endif // ERR_H
