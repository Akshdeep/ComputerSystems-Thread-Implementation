/*
 * file:        qthread.h
 * description: assignment - simple emulation of POSIX threads
 * class:       CS 5600, Spring 2018
 */
#ifndef __QTHREAD_H__
#define __QTHREAD_H__

#include <sys/socket.h>

/* you need to define the following (typedef) types:
 *  qthread_t, qthread_mutex_t, qthread_cond_t
 * I'll assume you're defining them as aliases for structures of the
 *  same name (minus the "_t") for mutex and cond, and a pointer to
 *  'struct qthread' for qthread_t.
 */

/* forward reference. Define the contents in qthread.c
 */
struct qthread; 
typedef struct qthread *qthread_t;

/* you'll probably need to define struct threadq here, since you'll be
 * using it in your mutex and condvar definitions
 */
struct threadq {
    struct qthread *head;
    struct qthread *tail;
};

/* you need to define 'struct qthread_mutex' and 'struct qthread_cond'
 */
struct qthread_mutex {
    int            locked;
    struct threadq waiters;
};
struct qthread_cond {
    struct threadq waiters;
};

typedef struct qthread_mutex qthread_mutex_t;
typedef struct qthread_cond qthread_cond_t;

/* prototypes - see qthread.c for function descriptions
 */

void qthread_run(void);
qthread_t qthread_start(void (*f)(void*, void*), void *arg1, void *arg2);
qthread_t qthread_create(void* (*f)(void*), void *arg1);
void qthread_yield(void);
void qthread_exit(void *val);
void *qthread_join(qthread_t thread);
void qthread_mutex_init(qthread_mutex_t *mutex);
void qthread_mutex_lock(qthread_mutex_t *mutex);
void qthread_mutex_unlock(qthread_mutex_t *mutex);
void qthread_cond_init(qthread_cond_t *cond);
void qthread_cond_wait(qthread_cond_t *cond, qthread_mutex_t *mutex);
void qthread_cond_signal(qthread_cond_t *cond);
void qthread_cond_broadcast(qthread_cond_t *cond);
void qthread_usleep(long int usecs);
ssize_t qthread_read(int sockfd, void *buf, size_t len);
ssize_t qthread_recv(int sockfd, void *buf, size_t len, int flags);
int qthread_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t qthread_write(int sockfd, void *buf, size_t len);
ssize_t qthread_send(int sockfd, void *buf, size_t len, int flags);
unsigned get_usecs(void);

#endif
