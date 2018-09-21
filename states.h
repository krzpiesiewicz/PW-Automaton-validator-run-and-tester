#ifndef STATES_H
#define STATES_H

#include <stdbool.h>

#define STMAX 100
#define MAX_ALPHBT_SIZE 'z' - 'a' + 1

enum typest {UNIVERSAL, EXISTENCIAL};

struct state {
    int idx; // numer stanu (do debugowania)
    enum typest type;
    bool acc;
    struct state ** to_state[MAX_ALPHBT_SIZE];
    int * st_cnts;
};

typedef struct state st;

// dane autmatu:
st *sts, *begst;
int st_cnt, alphbt_size;

void read_automaton_from_stdin();

void free_automaton();

#endif // STATES_H