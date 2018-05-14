/*
 * file:        qthread.c
 * description: assignment - simple emulation of POSIX threads
 * class:       CS 5600, Spring 2018
 * authors:     Akshdeep Rungta, Hongxiang Wang
 */

/* a bunch of includes which will be useful */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include "qthread.h"

struct qthread{
    void *stack;                //Stack
    void *sp;                   //Stack Pointer
    void *retval;               //Return value 
    int done_flag;              //Flag to indicate if thread is done (1 for done)
    struct qthread *waiter;     //The thread waiting for this thread
    struct qthread *next;       //Pointer to the next thread in thread queue
    int fd;                     //file descriptor 
    int read_flag;              //Flag for indicating read or write (1 for read, 0 for write)
};

struct qthread *current; 
struct threadq active;
struct threadq sleepers;
struct threadq io_waiters;

#define STACCK_SIZE 8192

void *main_stack; 

/* prototypes for stack.c and switch.s
 * see source files for additional details
 */
extern void switch_to(void **location_for_old_sp, void *new_value);
extern void *setup_stack(int *stack, void *func, void *arg1, void *arg2);

// pop
struct qthread * tq_pop(struct threadq *tq){
    if (tq->head == NULL)
        return NULL;
    else{
        struct qthread *t = tq->head;
        tq->head = t->next;
        if (tq->head == NULL)
            tq->tail = NULL;
        return t;
    }
}

// append
void  tq_append(struct threadq *tq, struct qthread *item){
    item->next = NULL;
    if (tq->head == NULL)
        tq->head = tq->tail = item;
    else{
        tq->tail->next = item;
        tq->tail = item;
    }
}

//Check if threadq is empty
int tq_empty(struct threadq *tq)
{
    return tq->head == NULL;
}

/* qthread_start, qthread_create
 * (function passed to qthread_create is allowed to return)
 */
qthread_t qthread_start(void (*f)(void*, void*), void *arg1, void *arg2){
    /* your code here */
    struct qthread *th = calloc(sizeof(*th) , 1);
    th->stack = malloc(STACCK_SIZE);
    th->sp = setup_stack(th->stack + STACCK_SIZE, f, arg1, arg2);
    th->done_flag = 0;
    th->waiter = NULL;
    th->fd = (int) NULL;
    th->read_flag = -1;
    tq_append(&active, th);
    return th;
}

// Function that makes sure that the thread exits
void qthread_run2(void * (*f)(void *), void * arg1){
    void *val;
    val = f(arg1);
    qthread_exit(val);
}

/* qthread_start, qthread_create
 * (function passed to qthread_create is allowed to return)
 */
qthread_t qthread_create(void* (*f)(void*), void *arg1){
    struct qthread *th = calloc(sizeof(*th), 1);
    th->stack = malloc(STACCK_SIZE);
    th->sp = setup_stack(th->stack + STACCK_SIZE, qthread_run2, f, arg1);
    th->done_flag = 0;
    th->waiter = NULL;
    th->fd = (int) NULL;
    th->read_flag = -1;
    tq_append(&active, th);
    return th;
}

//io_wait: selects a fd which is available
void io_wait() {
    fd_set rfds;
    fd_set wfds;
    int nfds = 0;
    struct timeval tv;    
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    tv.tv_sec = 0;
    tv.tv_usec = 10000;
    struct qthread * i =  io_waiters.head;
    while(i != NULL) {
        nfds++;
        if(i->read_flag == 1) 
            FD_SET(i->fd, &rfds);
        else
            FD_SET(i->fd, &wfds);

        i = i->next;
    }    

    select(nfds, &rfds, &wfds, NULL, &tv);
}


//Schedule: Switches to next available thread
void schedule(void *save_location){
    struct qthread *self = current;
again:
    current = tq_pop(&active);
    if(current == self)
        return;
    if(current == NULL && tq_empty(&sleepers) && tq_empty(&io_waiters))
        switch_to(NULL, main_stack);
    else if (current == NULL){
        io_wait();
        while(!tq_empty(&io_waiters)) {
            tq_append(&active, tq_pop(&io_waiters));
        }
        usleep(10000);
        while (!tq_empty(&sleepers)){
            struct qthread *t = tq_pop(&sleepers);
            tq_append(&active, t);
        }
        goto again;
    }
    switch_to(save_location, current->sp);
}

/* qthread_run - run until the last thread exits
 */
void qthread_run(void){
    schedule(&main_stack);
}

/* qthread_yield - yield to the next runnable thread.
 */
void qthread_yield(void){
    struct qthread *self = current;
    tq_append(&active, self);
    current = tq_pop(&active);
    if (current == self)
        return;
    switch_to(&self->sp, current->sp);
}

/* qthread_exit, qthread_join - exit argument is returned by
 * qthread_join. Note that join blocks if thread hasn't exited yet,
 * and may crash if thread doesn't exist.
 */
void qthread_exit(void *val){
    struct qthread *self = current;
    self->retval = val;
    self->done_flag = 1;
    if (self->waiter != NULL) {
        tq_append(&active, self->waiter);
        schedule(&self->sp);
    }
        
    current = tq_pop(&active);
    if (current == NULL)
        switch_to(&self->sp, main_stack);
    else
        switch_to(&self->sp, current->sp);
}

//qthread_join: wait for thread to finish and return value returned by thread
void *qthread_join(qthread_t thread){
    thread->waiter = current;
    void *value;
    again2:
        if(thread->done_flag){
            value = thread->retval;
            free(thread);
            return value;
        }
        else{
            schedule(&current->sp);
        }
    goto again2;   

}

/* qthread_mutex_init/lock/unlock
 */

void qthread_mutex_init(qthread_mutex_t *mutex){
    mutex->locked = 0;
    mutex->waiters.tail = NULL;
    mutex->waiters.head = NULL;
}

//qthread_mutex_lock: Try to lock the given mutex else wait
void qthread_mutex_lock(qthread_mutex_t *mutex){
    if (mutex->locked == 0) {
        mutex->locked = 1;
    } 
    else {
        tq_append(&mutex->waiters, current);
        schedule(&current->sp);
    }
}

//qthread_mutex_unlock: Unlock given mutex
void qthread_mutex_unlock(qthread_mutex_t *mutex){
    if (tq_empty(&mutex->waiters)) {
        mutex->locked = 0;
    }
    else {
        tq_append(&active, tq_pop(&mutex->waiters));
    }
}

/* qthread_cond_init/wait/signal/broadcast
 */
void qthread_cond_init(qthread_cond_t *cond){
    cond->waiters.head = NULL;
    cond->waiters.tail = NULL;
}

//qthread_cond_wait: Wait for condition
void qthread_cond_wait(qthread_cond_t *cond, qthread_mutex_t *mutex){
    tq_append(&cond->waiters, current);
    qthread_mutex_unlock(mutex);
    schedule(&current->sp);
    qthread_mutex_lock(mutex);
}

//qthread_cond_signal: Wake up first thread waiting on condition
void qthread_cond_signal(qthread_cond_t *cond){
    /* your code here */
    if (tq_empty(&cond->waiters))
        return;
    tq_append(&active, tq_pop(&cond->waiters));
}

//qthread_cond_broadcast: Wake up all threads waiting on condition
void qthread_cond_broadcast(qthread_cond_t *cond){
    /* your code here */
    while(!tq_empty(&cond->waiters)) {
        tq_append(&active, tq_pop(&cond->waiters));
    }    
}

/* POSIX replacement API. These are all the functions (well, the ones
 * used by the sample application) that might block.
 *
 * If there are no runnable threads, your scheduler needs to block
 * waiting for one of these blocking functions to return. You should
 * probably do this using the select() system call, indicating all the
 * file descriptors that threads are blocked on, and with a timeout
 * for the earliest thread waiting in qthread_usleep()
 */

/* You'll need to tell time in order to implement qthread_usleep.
 * Here's an easy way to do it. 
 */
unsigned get_usecs(void){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000000 + tv.tv_usec;
}

/* qthread_usleep - yield to next runnable thread, making arrangements
 * to be put back on the active list after 'usecs' timeout. 
 */
void qthread_usleep(long int usecs){
    /* your code here */
    unsigned wakeup = get_usecs() + usecs;
    while(get_usecs() < wakeup){
        tq_append(&sleepers, current);
        schedule(&current->sp);
    }
}

/* make sure that the file descriptor is in non-blocking mode, try to
 * read from it, if you get -1 / EAGAIN then add it to the list of
 * file descriptors to go in the big scheduling 'select()' and switch
 * to another thread.
 */
ssize_t qthread_io(ssize_t (*op)(int, void*, size_t), int fd, void *buf, size_t len){
    /* set non-blocking mode every time. If we added some more
     * wrappers we could set non-blocking mode from the beginning, but
     * this is a lot simpler (if less efficient). Do this for _write
     * and _accept, too.
     */
    int val, tmp = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, tmp | O_NONBLOCK);
}

//qthread_read: Read from the given fd and return value
ssize_t qthread_read(int fd, void *buf, size_t len){
    int val, tmp = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, tmp | O_NONBLOCK);
    current->fd = fd;
    current->read_flag = 1;
    while ( (val = read(fd, buf, len)) == -1 && errno == EAGAIN ) {
        tq_append(&io_waiters, current);
        schedule(&current->sp);
    }
    return val;
}

ssize_t qthread_recv(int sockfd, void *buf, size_t len, int flags){
    return qthread_read(sockfd, buf, len);
}

/* like read - make sure the descriptor is in non-blocking mode, check
 * if if there's anything there - if so, return it, otherwise save fd
 * and switch to another thread.
 */
int qthread_accept(int fd, struct sockaddr *addr, socklen_t *addrlen){
    /* your code here */
    int val, tmp = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, tmp | O_NONBLOCK);
    current->fd = fd;
    current->read_flag = 1;
    while ( (val = accept(fd, addr, addrlen)) == -1 && errno == EAGAIN ) {
        tq_append(&io_waiters, current);
        schedule(&current->sp);
    }
    return val;
}

//qthread_write: Writes to the given fd 
ssize_t qthread_write(int fd, void *buf, size_t len){
    /* your code here */
    int val, tmp = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, tmp | O_NONBLOCK);
    current->fd = fd;
    current->read_flag = 0;
    while ( (val = write(fd, buf, len)) == -1 && errno == EAGAIN ) {
        tq_append(&io_waiters, current);
        schedule(&current->sp);
    }
    return val;
}

ssize_t qthread_send(int fd, void *buf, size_t len, int flags){
    return qthread_write(fd, buf, len);
}
