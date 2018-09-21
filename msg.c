#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>

#include "err.h"
#include "msg.h"

#define RES_FOR_TESTER "/kp385996_res_for_tester_"
#define RES_FOR_TESTER_MAX_NAMESIZE strlen(RES_FOR_TESTER) + INT_LEN

char *res_for_tester_qname(int tester_pid) {
    char *name = malloc(RES_FOR_TESTER_MAX_NAMESIZE);
    sprintf(name, "%s%d", RES_FOR_TESTER, tester_pid);
    return name;
}

mqd_t create_q(const char *qname, size_t maxsize, bool nonblock,
               void (*release_fun)(void)) {
    struct mq_attr mq_a;
    mq_a.mq_maxmsg = QSIZE;
    mq_a.mq_msgsize = maxsize;

    int flags = O_RDWR | O_CREAT;
    if (nonblock) {
        flags |= O_NONBLOCK;
    }

    mqd_t desc = mq_open(qname, flags, 0666, &mq_a);
    if (desc == (mqd_t) -1) {
        if (release_fun != NULL) {
            release_fun();
        }
        syserr(EXIT, "Error in mq_open (creating %s)", qname);
    }

    return desc;
}

mqd_t open_to_read(const char *qname) {
    mqd_t desc = mq_open(qname, O_RDONLY);
    if (desc == (mqd_t) -1) {
        syserr(NOEXIT, "Error in mq_open (opening %s for read)", qname);
    }
    return desc;
}

mqd_t open_to_write(const char *qname) {
    mqd_t desc = mq_open(qname, O_WRONLY);
    if (desc == (mqd_t) -1) {
        syserr(NOEXIT, "Error in mq_open (opening %s for write)", qname);
    }
    return desc;
}

void closeq(mqd_t desc) {
    if (mq_close(desc) == -1) {
        syserr(NOEXIT, "Error in mq_close (closing desc %d)", desc);
    }
}

void set_nonblocking(mqd_t desc) {
    struct mq_attr mq_a;
    mq_getattr(desc, &mq_a);

    mq_a.mq_flags |= O_NONBLOCK;
    mq_setattr(desc, &mq_a, NULL);
}

void unlinkq(const char *qname) {
    if (mq_unlink(qname) == -1) {
        syserr(NOEXIT, "Error in mq_unlink (unlinking %s)", qname);
    }
}