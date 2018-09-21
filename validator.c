#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include "err.h"
#include "msg.h"

#define V_DB false

// zapisane wejście (opis automatu)
#define BLOCK_SIZE 1000
int b_cnt;
ssize_t r_bytes;
size_t to_send_size = sizeof(char*);
char **blocks, *res_qname = NULL;

// statystyki:
int rcd, snt, acc, pids, t_size, *t_pid, *t_rcd, *t_acc, *t_snt, run_cnt;

// deskryptory
mqd_t recpt_d, ticket_d, still_allive_d, tickets_available_d;

void switch_off_tickets() {

    closeq(tickets_available_d);
    unlinkq(TICKETS_AVAILABLE);

    struct mq_attr mq_a;
    mq_getattr(ticket_d, &mq_a);

    mq_a.mq_flags |= O_NONBLOCK;
    mq_setattr(ticket_d, &mq_a, NULL);

    for (int i = 0; i < mq_a.mq_maxmsg - mq_a.mq_curmsgs; i++) {
        mq_send(ticket_d, "0", sizeof(char), 2);
    }
    closeq(ticket_d);
    unlinkq(TICKETS_FOR_VALIDATION);
}

void release_resource() {
    char send[100];
    for (int idx = 0; idx < pids; idx++) {
        if (t_rcd[idx] > t_snt[idx]) {
            mqd_t dt = open_to_write(res_for_tester_qname(t_pid[idx]));
            if (dt != -1) {
                for (int i = 0; i < t_rcd[idx] - t_snt[idx]; i++) {

                    sprintf(send, "%d %d%c", -1, -1, '\0');
                    if (mq_send(dt, send, strlen(send) + 1, 1) == -1) {
                        syserr(NOEXIT, "Error in sending res");
                    }
                }
                closeq(dt);
            }
        }
    }

    for (int i = 0; i <= b_cnt; i++) {
        free(blocks[i]);
    }
    free(blocks);
    free(t_pid);
    free(t_rcd);
    free(t_acc);

    if (res_qname != NULL) {
        free(res_qname);
    }

    switch_off_tickets();

    closeq(still_allive_d);
    unlinkq(VALIDATOR_STILL_ALIVE);

    closeq(recpt_d);
    unlinkq(VALIDATOR_RECEIVER);

    for (int i = 0; i < run_cnt; i++) {
        wait(0);
    }
}

void send_ticket() {
    if (mq_send(ticket_d, "1", sizeof(char), 2) == -1) {
        release_resource();
        syserr(EXIT, "Error in send ticket");
    }
}

void expand_arrays(int new_size) {
    t_pid = realloc(t_pid, new_size * sizeof(int));
    t_rcd = realloc(t_rcd, new_size * sizeof(int));
    t_acc = realloc(t_acc, new_size * sizeof(int));
    t_snt = realloc(t_snt, new_size * sizeof(int));
    t_size = new_size;
}

int find_idx(int pid) {
    for (int i = 0; i < pids; i++) {
        if (t_pid[i] == pid) {
            return i;
        }
    }
    if (pids == t_size) {
        expand_arrays(2 * t_size + 1);
    }
    t_pid[pids] = pid;
    pids++;
    return pids - 1;
}

void save_stdin() {
    blocks = malloc(to_send_size * sizeof(char*));
    do {
        if (b_cnt * sizeof(char*) == to_send_size) {
            to_send_size *= 2;
            blocks = realloc(blocks, to_send_size);
        }
        blocks[b_cnt] = malloc(BLOCK_SIZE);

        if ((r_bytes = read(0, blocks[b_cnt], BLOCK_SIZE)) == -1) {
            release_resource();
            syserr(EXIT, "Error in read from stdin");
        }
        b_cnt++;
    } while (r_bytes == BLOCK_SIZE);
}

void write_saved_blocks(int fd) {
    for (int i = 0; i < b_cnt - 1; i++) {
        if (write(fd, blocks[i], BLOCK_SIZE) == -1) {
            release_resource();
            syserr(EXIT, "Error in write to fd (writing saved blocks)");
        }
    }
    if (write(fd, blocks[b_cnt - 1], r_bytes) == -1) {
        syserr(EXIT, "Error in write to fd (writing last saved block)");
    }
}

void prepare_pipe_for_run(int pipe_dsc[]) {
    if (close(0) == -1) {
        release_resource();
        syserr(EXIT, "Error in child, close (0)\n");
    }
    if (dup(pipe_dsc[0]) != 0) {
        syserr(EXIT,
               "Error in child, dup (pipe_dsc [0])\n");
    }
    if (close(pipe_dsc[0]) == -1) {
        release_resource();
        syserr(EXIT,
               "Error in child, close (pipe_dsc [0])\n");
    }
    if (close(pipe_dsc[1]) == -1) {
        release_resource();
        syserr(EXIT,
               "Error in child, close (pipe_dsc [1])\n");
    }
}

void print_output() {
    printf("Rcd: %d\nSnt: %d\nAcc: %d\n", rcd, snt, acc);
    for (int i = 0; i < pids; i++) {
        printf("PID: %d\nRcd: %d\nAcc: %d\n", t_pid[i], t_rcd[i], t_acc[i]);
    }
}

int main() {
    SET_P_NAME("validator");

    recpt_d = create_q(VALIDATOR_RECEIVER, VALIDATOR_RECEIVER_MAX_MSGSIZE,
                       false, NULL);
    ticket_d = create_q(TICKETS_FOR_VALIDATION,
                        TICKETS_FOR_VALIDATION_MAX_MSGSIZE, false,
                        release_resource);

    still_allive_d = create_q(VALIDATOR_STILL_ALIVE, 1, true, release_resource);
    tickets_available_d = create_q(TICKETS_AVAILABLE, 1, true,
                                   release_resource);

    send_ticket();

    save_stdin();

    t_pid = malloc(t_size * sizeof(int));
    t_rcd = malloc(t_size * sizeof(int));
    t_acc = malloc(t_size * sizeof(int));
    t_snt = malloc(t_size * sizeof(int));

    int pid, num, res, order_cnt = 0, idx;
    bool no_more = false;
    char buff[VALIDATOR_RECEIVER_MAX_MSGSIZE], send[100], word[MAXLEN + 1];
    unsigned priority;

    run_cnt = 0;

    while(!no_more || order_cnt > 0) {

        int len = mq_receive(recpt_d, (char*) &buff, VALIDATOR_RECEIVER_MAX_MSGSIZE,
                             &priority);

        if (len == -1) {
            syserr(NOEXIT, "Error in rec");
        }

        PRINTLNF(V_DB, "buff %s", buff);
        switch(buff[0]) {
            case 'r': // odebranie wyniku

                sscanf(buff + 2, "%d %d %d", &idx, &num, &res);

                if (res != -1) {

                    if (res == 1) {
                        acc++;
                        t_acc[idx]++;
                    }

                    res_qname = res_for_tester_qname(t_pid[idx]);
                    mqd_t dt = open_to_write(res_qname);
                    if (dt != -1) {
                        sprintf(send, "%d %d%c", num, res, '\0');
                        if (mq_send(dt, send, strlen(send) + 1, 1) == -1) {
                            syserr(NOEXIT, "Error in sending res");
                        } else {
                            snt++;
                            t_snt[idx]++;
                        }
                        closeq(dt);
                    }
                }
                order_cnt--;
//                else {
//                    no_more =
//                }
                break;

            default: // nowe zlecenie
                sscanf(buff + 2, "%s %d %d", word, &pid, &num);

                if(word[0] == '!') { // zakończenie przyjmowania zleceń
                    no_more = true; // nie dajemy więcej biletów testerom
                    set_nonblocking(ticket_d);
                    PRINTLNF(V_DB, "!");
                    break;
                }

                order_cnt++;
                idx = find_idx(pid);
                t_rcd[idx]++;
                rcd++;

                int pipe_dsc[2];
                if (pipe(pipe_dsc) == -1) {
                    release_resource();
                    syserr(EXIT, "Error in pipe");
                } else {
                    switch (fork()) {
                        case -1 :
                            release_resource();
                            syserr(EXIT, "Error in fork");
                            break;
                        case 0:
                            prepare_pipe_for_run(pipe_dsc);
                            execl("run", "run", NULL);
                            break;
                        default:
                            run_cnt++;
                            if (close(pipe_dsc[0]) == -1) {
                                release_resource();
                                syserr(EXIT,
                                       "Error in parent, close (pipe_dsc[1])\n");
                            }

                            len = sprintf(buff, "%d %d ", idx, num);
                            if (write(pipe_dsc[1], buff, len) == -1) {
                                release_resource();
                                syserr(EXIT, "Error in write");
                            }

                            len = strlen(word);
                            word[len] = '\n';
                            if (write(pipe_dsc[1], word, len + 1) == -1) {
                                release_resource();
                                syserr(EXIT, "Error in write");
                            }
                            write_saved_blocks(pipe_dsc[1]);

                            if (close(pipe_dsc[1]) == -1) {
                                release_resource();
                                syserr(EXIT, "Error in parent, close "
                                        "(pipe_dsc[1])\n");
                            }
                            break;
                    }
                    send_ticket();

                    break;
                }
        }
    }
    print_output();
    release_resource();
    return 0;
}