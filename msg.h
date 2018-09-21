#ifndef MSG_H
#define MSG_H

#include <stdbool.h>
#include <mqueue.h>

#include "err.h"

#define QSIZE 10

#define VALIDATOR_STILL_ALIVE "/kp385996_validator_still_alive"
#define TICKETS_AVAILABLE "/kp385996_tickets_available"

#define VALIDATOR_RECEIVER "/kp385996_validator_receiver"
#define TICKETS_FOR_VALIDATION "/kp385996_tickets_for_validation"

#define INT_LEN 12 // długość zapisu INT-a w systemi dziesiętnym (ze spacją)
#define MAXLEN 1007 // maks. długość słowa

#define VALIDATOR_RECEIVER_MAX_MSGSIZE 3 * INT_LEN + MAXLEN
#define TICKETS_FOR_VALIDATION_MAX_MSGSIZE 1
#define RES_FOR_TESTER_MAX_MSGSIZE 3 * INT_LEN

char *res_for_tester_qname(int tester_pid);

mqd_t create_q(const char *qname, size_t maxsize, bool nonblock,
               void (*release_fun)(void));

mqd_t open_to_read(const char *qname);

mqd_t open_to_write(const char *qname);

void closeq(mqd_t desc);

void set_nonblocking(mqd_t desc);

void unlinkq(const char *qname);

#endif // MSG_H