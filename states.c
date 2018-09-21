#include <stdio.h>
#include <stdlib.h>
#include "states.h"
#include "err.h"

#define INT_LEN 3 // długość liczby od 0 do 99 ze spacją

#define RD_DB false
void read_automaton_from_stdin() {
    int    lines_cnt, st_no,
           ust_cnt, // # stanów uniwersalnych
           acst_cnt, // # stanów akceptujących
           bytes;
    size_t line_size = 100 * sizeof(char);
    char c, *line = malloc(line_size), *line_ptr;


    getline(&line, &line_size, stdin);
    sscanf(line, "%d %d %d %d %d", &lines_cnt, &alphbt_size, &st_cnt, &ust_cnt,
          &acst_cnt);

    sts = calloc(st_cnt, sizeof(st));
    for (int i = 0; i < st_cnt; i++) {
        sts[i].acc = false;
        sts[i].idx = i;
        sts[i].type = i < ust_cnt ? UNIVERSAL : EXISTENCIAL;
        sts[i].st_cnts = malloc(st_cnt * sizeof(int));
    }

    // stan początkowy
    getline(&line, &line_size, stdin);
    sscanf(line, "%d", &st_no);
    begst = sts + st_no;

    // stany akceptujące
    if (acst_cnt > 0) {
        getline(&line, &line_size, stdin);
        line_ptr = line;
        for (int i = 0; i < acst_cnt; i++) {
            sscanf(line_ptr, "%d%n", &st_no, &bytes);
            line_ptr += bytes;
            sts[st_no].acc = true;
        }
    }

    // przekształcenie T
    int sts_nos[STMAX + 7]; // +1 bo próbujemy wczytać zawsze o jedną więcej
    st *st1;

    for (int l = 3; l < lines_cnt; l++) {
        getline(&line, &line_size, stdin);
        line_ptr = line;
        sscanf(line_ptr, "%d %c%n", &st_no, &c, &bytes);
        line_ptr += bytes;
        st1 = &sts[st_no];
        c -= 'a';

        int cnt = 0, ret = 1;
        while (ret != EOF) { // EOF == -1
            ret = sscanf(line_ptr, " %d%n", &sts_nos[cnt], &bytes);
            line_ptr += bytes;
            cnt++;
        }
        cnt--; // bo przy EOF również zwiększyliśmy

        st1->to_state[(int) c] = malloc(cnt * sizeof(st*));
        st1->st_cnts[(int) c] = cnt;

        for (int i = 0; i < cnt; i++) {
            st1->to_state[(int) c][i] = &sts[sts_nos[i]];
        }
    }

    for (int i = 0; i < st_cnt; i++) {
        st1 = &sts[i];
        PRINTLN(RD_DB, "stan %d:", i)
        for (int j = 0; j < alphbt_size; j++) {
            PRINT(RD_DB, "      %c ->", 'a' + j)
            for (int k = 0; k < st1->st_cnts[j]; k++) {
                PRINT(RD_DB, " %d", st1->to_state[j][k]->idx)
            }
            PRINT(RD_DB, "\n")
        }
        PRINT(RD_DB, "\n") FLUSH(RD_DB)
    }

    free(line);
}

void free_automaton() {
    for (int i = 0; i < st_cnt; i++) {
        for (int j = 0; j < alphbt_size; j++) {
            free(sts[i].to_state[j]);
        }
        free(sts[i].st_cnts);
    }
    free(sts);
}