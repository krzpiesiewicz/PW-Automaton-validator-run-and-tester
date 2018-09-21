#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "err.h"
#include "msg.h"
#include "states.h"

#define W_DB false

enum msg_type {END, RES, ORD};

struct worker_msg {
    int from;
    enum msg_type type;
    int pos, val;
};

typedef struct worker_msg msg_t;

struct msg_to_send {
    int to_num;
    msg_t msg;
};

typedef struct msg_to_send to_send_t;

struct msg_stack {
    to_send_t * buff;
    int cnt, size;
};

typedef struct msg_stack msg_stack_t;

char w[MAXLEN];
int tester_idx, task_num, res, my_num, wlen, subscr_cnt[MAXLEN],
        res_subscr[MAXLEN][STMAX], res_for_pos[MAXLEN],
        sum_for_pos[MAXLEN], ord_dsc[STMAX][2], res_dsc[STMAX][2],
        oth_dsc[STMAX][2];
bool ord_rec_for_pos[MAXLEN];
pid_t validator_pid, parent_pid;

msg_stack_t orders, results, others;

void init_stack(msg_stack_t * stack) {
    stack->size = 1;
    stack->buff = malloc(stack->size * sizeof(to_send_t));
}

void init_stacks() {
    init_stack(&orders);
    init_stack(&results);
    init_stack(&others);
}

void release_stack(msg_stack_t * stack) {
    free(stack->buff);
}

void close_pipes_of_one_kind(int dscs[][2]) {
    for (int i = 0; i <= st_cnt; i++) {
        for (int s = 0; s < 2; s++) {
            close(dscs[i][s]);
        }
    }
}

void close_unused_pipes_of_one_kind(int dscs[][2]) {
    for (int i = 0; i <= st_cnt; i++) {
        if (i != my_num) {
            close(dscs[i][0]);
        }
    }
}

void close_unused_pipes() {
    close_unused_pipes_of_one_kind(ord_dsc);
    close_unused_pipes_of_one_kind(res_dsc);
    close_unused_pipes_of_one_kind(oth_dsc);
}

void close_pipes() {
    close_pipes_of_one_kind(ord_dsc);
    close_pipes_of_one_kind(res_dsc);
    close_pipes_of_one_kind(oth_dsc);
}

void release_worker_resources() {
    close_pipes();
    release_stack(&orders);
    release_stack(&results);
    release_stack(&others);
}

void release_resources() {
    free_automaton();
}

void check_stack_size(msg_stack_t * stack) {
    if (stack->cnt == stack->size) {
        stack->size *= 2;
        stack->buff = realloc(stack->buff, stack->size * sizeof(to_send_t));
    }
}

#define MSG_DB false

bool send_msg(msg_t * msg, int to_num) {
    int desc;
    switch(msg->type) {
        case RES:
            desc = res_dsc[to_num][1];
            break;
        case ORD:
            desc = ord_dsc[to_num][1];
            break;
        default:
            desc = oth_dsc[to_num][1];
            break;
    }
    int wrote = write(desc, msg, sizeof(struct worker_msg));
    if (wrote > 0) {
        PRINTLNF(MSG_DB, "sent msg: from %d, type %d, pos %d, val %d, to %d",
                 msg->from, msg->type, msg->pos, msg->val, to_num);
    }
    return wrote > 0;
}

bool send_one_from_stack(msg_stack_t * stack) {
    bool sent = false;
    if (stack->cnt > 0) {
        sent = send_msg(&stack->buff[stack->cnt - 1].msg,
                        stack->buff[stack->cnt - 1].to_num);
        if (sent) {
            stack->cnt--;
        }
    }
    return sent;
}

/**
 * zwraca wskaźnik na nowostworzoną wiadomość na stosie
 * @param msg
 * @param to_num
 */
msg_t * new_msg_for_stack(msg_stack_t * stack, int to_num) {
    check_stack_size(stack);
    stack->buff[stack->cnt].to_num = to_num;
    stack->cnt++;
    return &stack->buff[stack->cnt - 1].msg;
}

void send_res(int pos, int to_num) {
    msg_t *newmsg = new_msg_for_stack(&results, to_num);
    newmsg->val = res_for_pos[pos];
    newmsg->pos = pos;
    newmsg->type = RES;
    newmsg->from = my_num;
}

void get_res(msg_t * msg) {
    int pos = msg->pos - 1;
    if (res_for_pos[pos] == -1) {

        sum_for_pos[pos]++;

        switch (sts[my_num].type) {
            case UNIVERSAL:
                switch (msg->val) {
                    case 0:
                        PRINTLNF(W_DB, "jestem uni, dostałem 0, pos %d", pos)
                        res_for_pos[pos] = 0;
                        break;
                    case 1:
                        if (sum_for_pos[pos] ==
                                sts[my_num].st_cnts[(int) w[pos] - 'a']) {
                            PRINTLNF(W_DB, "jestem uni, mam komplet 1, pos %d", pos)
                            res_for_pos[pos] = 1;
                        }
                        break;
                    default:
                        break;
                }
                break;
            case EXISTENCIAL:
                switch (msg->val) {
                    case 0:
                        if (sum_for_pos[pos] ==
                                sts[my_num].st_cnts[(int) w[pos] - 'a']) {
                            PRINTLNF(W_DB, "jestem exi, dostałem komplet 0, pos %d", pos)
                            res_for_pos[pos] = 0;
                        }
                        break;
                    case 1:
                        res_for_pos[pos] = 1;
                        PRINTLNF(W_DB, "jestem exi, dostałem 1, pos %d", pos)
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
        if (res_for_pos[pos] != -1) {
            PRINTLNF(W_DB, "wysyłam subcrip, res %d, pos %d",
                     res_for_pos[pos], pos)
            for (int i = 0; i < subscr_cnt[pos]; i++) {
                send_res(pos, res_subscr[pos][i]);
            }
        }
    }
}

void get_order(msg_t * msg) {
    int pos = msg->pos;
    PRINTLNF(W_DB, "pos %d, res %d", pos, res_for_pos[pos])
    if (res_for_pos[pos] != -1) {
        PRINTLNF(W_DB, "wysyłam bo już to mam, res %d, pos %d",
                 res_for_pos[pos], pos)
        send_res(pos, msg->from);
    }
    else {
        PRINTLNF(W_DB, "Zapisuję go (%d) na subscri pos %d", msg->from, pos)
        res_subscr[pos][subscr_cnt[pos]] = msg->from;
        subscr_cnt[pos]++;
        if (!ord_rec_for_pos[pos]) {
            PRINTLNF(W_DB, "Pierwszy (%d), pos %d", msg->from, pos)
            ord_rec_for_pos[pos] = true;
            int l = w[pos] - 'a';
            for (int i = 0; i < sts[my_num].st_cnts[l]; i++) {
                msg_t *newmsg = new_msg_for_stack(&orders, sts[my_num].to_state[l][i]->idx);
                newmsg->pos = pos + 1;
                newmsg->type = ORD;
                newmsg->from = my_num;
            }
            if (sts[my_num].st_cnts[l] == 0) {
                PRINTLNF(W_DB, "ŹLE!!!, from %d, pos %d", msg->from, pos)
            }
        }
    }
}

bool rec_msg(int dscs[][2], msg_t * msg) {
    ssize_t r = read(dscs[my_num][0], msg, sizeof(msg_t));
    if (r > 0) {
        PRINTLNF(MSG_DB, "rec msg: from %d, type %d, pos %d, val %d",
                 msg->from, msg->type, msg->pos, msg->val);
    }
    return r > 0;
}

void worker() {
    PRINTLNF(W_DB, "Jestem nowym workerem, my_num %d", my_num)

    close_unused_pipes();
    init_stacks();

    res_for_pos[wlen] = sts[my_num].acc;

    for (int i = 0; i < wlen; i++) {
        int l = w[i] - 'a';
        PRINTLNF(W_DB, "cnt %d, i %d", sts[my_num].st_cnts[l], i)
        if (sts[my_num].st_cnts[l] == 0) {
            PRINTLNF(W_DB, "================, i %d", i)
            if (sts[my_num].type == UNIVERSAL) {
                res_for_pos[i] = 1;
            }
            else {
                res_for_pos[i] = 0;
            }
        }
    }

    bool end = false;
    msg_t msg;
    int cnt;

    while(!end) {
        while(!end && rec_msg(oth_dsc, &msg)) {
            if (msg.type == END) {
                PRINTLNF(W_DB, "kończę")
                end = true;
            }
        }
        if (!end) {
            cnt = 0;
            while (cnt < 100 && rec_msg(res_dsc, &msg)) {
                get_res(&msg);
                cnt++;
            }
            cnt = 0;
            while (cnt < 100 && rec_msg(ord_dsc, &msg)) {
                get_order(&msg);
                cnt++;
            }
            cnt = 0;
            while (cnt < 100 && send_one_from_stack(&results)) {
                cnt++;
            }
            cnt = 0;
            while (cnt < 5 && send_one_from_stack(&orders)) {
                cnt++;
            }
        }
        cnt = 0;
        while (cnt < 100 && send_one_from_stack(&others)) {
            cnt++;
        }
    }
    release_worker_resources();
}

#define R_DB false

int create_workers() {
    for (int p = 0; p < st_cnt; p++) {
        pid_t pid = fork();
        switch(pid) {
            case -1:
                syserr(NOEXIT, "Error in fork");
                return -1;
            case 0:
//                SET_P_NAME("run (num=%d)", my_num)
                worker();
                exit(0);
            default:
                my_num++;
                break;
        }
    }
    return 0;
}

void send_end_to_workers() {
    for (int num = 0; num < st_cnt; num++) {
        msg_t msg = {.type = END, .from = my_num};
        send_msg(&msg, num);
    }
}

void send_res_to_validator() {
    char msg[100];
    sprintf(msg, "r %d %d %d%c", tester_idx, task_num, res, '\0');
    PRINTLNF(R_DB, "msg: %s", msg)

    sleep(3);

    mqd_t resq_d = open_to_write(VALIDATOR_RECEIVER);
    mq_send(resq_d, msg, strlen(msg) + 1, 1);
    closeq(resq_d);
}

int create_pipes_of_one_kind(int dscs[][2]) {
    for (int i = 0; i <= st_cnt; i++) {
        int r = pipe(dscs[i]);
        if (r == -1) {
            return -1;
        }
        for (int s = 0; s < 2; s++) {
            fcntl(dscs[i][0], F_SETFL,
                               fcntl(dscs[i][0], F_GETFL) | O_NONBLOCK);
        }
    }
    return 0;
}

int create_pipes() {
    if(create_pipes_of_one_kind(ord_dsc) == -1) {
        return -1;
    }
    if (create_pipes_of_one_kind(res_dsc) == -1) {
        return -1;
    }
    if (create_pipes_of_one_kind(oth_dsc) == -1) {
        return -1;
    }
    return 0;
}

int main() {

    SET_P_NAME("run")

    char *line = malloc(sizeof(char));
    size_t linesize = 1;
    getline(&line, &linesize, stdin);
    sscanf(line, "%d %d %s", &tester_idx, &task_num, w);
    free(line);

    PRINTLNF(R_DB, "w: %s", w)

    read_automaton_from_stdin();

    bool err = false;

    if (w[0] == '_') {
        res = begst->acc;
        send_res_to_validator();
    }
    else {
        // czyli niepuste słowo
        wlen = strlen(w);
        for (int i = 0; i < wlen; i++) {
            res_for_pos[i] = -1;
        }
        res = -1;

        if(create_pipes() == -1) {
            send_res_to_validator();
            release_resources();
            syserr(EXIT, "Error in pipe");
        }
        if(create_workers() == -1) {
            send_res_to_validator();
            send_end_to_workers();
            release_resources();
            syserr(EXIT, "Error in creationg workers");
        }

        close_unused_pipes();

        bool end = false;
        msg_t msg;

        msg.from = my_num;
        msg.pos = 0;
        msg.type = ORD;
        send_msg(&msg, begst->idx);

        while (!end) {
            while (!end && rec_msg(oth_dsc, &msg)) {
                if (msg.type == END) {PRINTLNF(R_DB, "i ja kończę źle")
                    end = true;
                    err = true;
                }
            }
            while (!end && rec_msg(res_dsc, &msg)) {
                res = msg.val;
                end = true;PRINTLNF(R_DB, "dostałem wynik")
            }
        }

        send_end_to_workers();
        send_res_to_validator();

        for (int num = 0; num < st_cnt; num++) {
            wait(0);
        }
        release_worker_resources();
    }
    release_resources();
    if (err) {
        return 1;
    }
}