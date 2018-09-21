#include <stdio.h>
#include <unistd.h>
#include "string.h"

#include "err.h"
#include "msg.h"

#define TST_DB false

int snt = 0, rcd = 0, acc = 0, t_cnt = 0, size = 1, bytes,
        exp_res_cnt = 0, *t_acc;
char ticket[10], msg[VALIDATOR_RECEIVER_MAX_MSGSIZE], **word, *res_qname;
bool end;
size_t msgsize;
mqd_t res_d;

void print_output_and_release_resources() {
    for (int i = 0; i < t_cnt; i++) {
        if (t_acc[i] != -1) {
            if (word[i][0] != '_') {
                printf("%s", word[i]);
            }
            printf(" %c\n", t_acc[i] ? 'A' : 'N');
        }
        free(word[i]);
    }
    free(word);

    printf("Snt: %d\nRcd: %d\nAcc: %d\n", snt, rcd, acc);

    free(t_acc);

    closeq(res_d);
    unlinkq(res_qname);
    free(res_qname);
}

void save_scaned_word() {
    if (t_cnt == size) {
        size *= 2;
        word = realloc(word, size * sizeof(char*));
        t_acc = realloc(t_acc, size * sizeof(int));
    }
    t_acc[t_cnt] = -1;
    word[t_cnt] = malloc(msgsize + 1);
    for (int i = 0; i < (int) msgsize; i++) {
        word[t_cnt][i] = msg[i + 2];
    }
    word[t_cnt][msgsize] = '\0';
}

void prepare_msg() {
    msgsize += 2;
    msg[0] = 'o';
    msg[1] = ' ';
    sprintf(msg + msgsize, " %d %d%c", getpid(), t_cnt, '\0');
    msgsize = strlen(msg) + 1;
    t_cnt++;
}

void send_msg() {
    mqd_t tickets_available = mq_open(TICKETS_AVAILABLE, O_RDONLY);
    if (tickets_available == -1) {
        syserr(NOEXIT, "Error in opening tickets_available");
        end = true;
    }
    else {
        mq_close(tickets_available);
        mqd_t ticket_d = mq_open(TICKETS_FOR_VALIDATION, O_RDONLY);
        if (ticket_d == -1) {
            syserr(NOEXIT, "Error in opening tickets");
            end = true;
        }

        unsigned int priority;
        ssize_t rec_res = mq_receive(ticket_d, ticket, 10, &priority);
        if (rec_res == -1) {
            syserr(EXIT, "ticket rec");
        }
        bool is_ticket = rec_res != -1 && ticket[0] == '1';
        PRINTLNF(TST_DB, "ticket: %s", ticket);
        closeq(ticket_d);

        if (is_ticket) {

            PRINTLNF(TST_DB, "I have ticket, msg to send: %s", msg);
            mqd_t ord_d = open_to_write(VALIDATOR_RECEIVER);
            if (ord_d == -1 || mq_send(ord_d, msg, msgsize, 5) == -1) {
                print_output_and_release_resources();
                syserr(EXIT, "Error in send order having ticket");
            } else {
                if (!end) {
                    snt++;
                    exp_res_cnt++;
                }
            }
            closeq(ord_d);
        } else {
            end = true;
        }
    }
}

void get_res() {
    int num, res;
    char resmsg[RES_FOR_TESTER_MAX_MSGSIZE];
    ssize_t recsize = mq_receive(res_d, resmsg, RES_FOR_TESTER_MAX_MSGSIZE, NULL);

    if (recsize != -1) {
        PRINTLNF(TST_DB, "resmsg: %s\n", resmsg)
        sscanf(resmsg, "%d %d", &num, &res);
        if (num != -1 && res != -1) {
            rcd++;
            t_acc[num] = res;
            acc += res;
        }
    }
    exp_res_cnt--;
}

int main() {
    SET_P_NAME("tester");

    t_acc = malloc(size * sizeof(int));
    word = malloc(size * sizeof(char*));

    res_qname = res_for_tester_qname(getpid());
    res_d = create_q(res_qname, RES_FOR_TESTER_MAX_MSGSIZE,
                     false, print_output_and_release_resources);

    printf("PID: %d\n", getpid());

    size_t line_size = 1003 * sizeof(char);
    ssize_t read;
    char *line = malloc(line_size);

    while(!end) {
        mqd_t vsta_d = open_to_read(VALIDATOR_STILL_ALIVE);

        end = true;
        if (vsta_d != -1 ) {
            read = getline(&line, &line_size, stdin);
            if (read != -1) {
                end = false;
            }
            PRINTLNF(TST_DB, "read %d", read);
            closeq(vsta_d);
        }

        if (!end) {

            if (line[0] != '\n') {
                sprintf(msg + 2, "%s", line);
                msgsize = strlen(msg + 2) - 1;
            }
            else {
                msg[2] = '_'; // puste słowo
                msgsize = sizeof(char);
            }

            save_scaned_word();

            end = msg[2] == '!';

            prepare_msg();

            PRINTLNF(TST_DB, "'%s' - size %d\n", msg, msgsize)

            send_msg();

            PRINTLNF(TST_DB, "exp_res_cnt %d", exp_res_cnt)

            // nie możemy wysłać za dużo zleceń, bo nie zmieszczą się
            // w kolejce wyników
            if (exp_res_cnt == QSIZE) {
                get_res();
            }
        }
    }
    free(line);

    while(exp_res_cnt > 0) {
        get_res();
    }

    print_output_and_release_resources();
}