#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "qthread.h"
#include <unistd.h>
/*
mutex lock/unlock
calling lock when the mutex is: unlocked, locked with no threads waiting, locked with one thread waiting, locked with two threads waiting
again, you'll probably have to use qthread_usleep to set up these scenarios
make sure that you call qthread_usleep and/or qthread_yield while holding the mutex, and increment a global variable or something inside the mutex so you can check to make sure no one else has entered the mutex.

cond wait/signal/broadcast
calling wait() when there are 0, 1, and 2 threads already waiting
calling signal() when there are 0, 1, 2, and 3 threads waiting
calling broadcast() with 0,1,2,3 threads waiting
Again you'll probably need to use qthread_usleep to coordinate things. For this and the mutex test, it's likely that if the test fails your code is just going to hang. That's OK, just fix it. (I'm the only one who has to come up with code that can run all the tests, even if some of them fail)

qthread_usleep, qthread_read 
I'll trust that your _write and _accept code works if you got _read to work
usleep - test with 1, 2, and 3 threads sleeping simultaneously, with (a) equal timeouts and (b) different timeouts, that they wait "about" the right amount of time. (e.g. use get_usecs to time how long the call to qthread_usleep blocks before returning)
for testing read you can create a pipe (see 'man 2 pipe') - it generates a read and write file descriptor; you can have one thread call read() on the read descriptor, wait a while, and then have another thread call write() on the other descriptor
run usleep tests under two cases: (a) no threads waiting for I/O, and (b) one thread blocked in qthread_read
test read with 1, 2, and 3 threads calling read (on separate pipes) at the same time. Do it (a) without, and (b) with one thread blocked in qthread_usleep
*/

/*
  create/yield/join/exit, make sure you test the following cases:
    1 thread, yield several times, then exit
    2 threads, yield back and forth a few times, both exit
    3 or more threads, same
    call join before the child thread exits
    call join after the child thread exits (you may want to use qthread_usleep for this)
*/
qthread_mutex_t m;
qthread_cond_t c1, c2, c3;

void* mutex_test1(void* arg)
{
    for(int i =0; i<5; i++){        
        qthread_mutex_lock(&m);
        printf("Thread %s: Locking mutex\n", (char *)arg);
        printf("%s\n",(char *)arg);
        qthread_usleep(100000);
        printf("Thread %s: Unlocking mutex\n", (char *)arg);
        qthread_mutex_unlock(&m);
    }
    return arg;
}

void mutex_test_3_threads(void)
{
    printf("\nTesting mutex with 3 threads\n");
    qthread_t t[3];
    qthread_mutex_init(&m);
    t[0] = qthread_create(mutex_test1, "1");
    t[1] = qthread_create(mutex_test1, "2");
    t[2] = qthread_create(mutex_test1, "3");
    qthread_run();
    return;
}    

void* cond_test1(void* arg)
{
    qthread_usleep(100000);
    qthread_mutex_lock(&m);
    for(int i =0; i<5; i++){   
        if(i != 0){     
            printf("Thread %s: wait c1\n", (char *)arg);
            qthread_cond_wait(&c1, &m);
        }
        else
            qthread_mutex_unlock(&m);
        printf("%s\n",(char *)arg);
        qthread_usleep(100000);
        printf("Thread %s: signal c2\n", (char *)arg);
        qthread_cond_signal(&c2);
    }
    qthread_mutex_unlock(&m);
    return arg;
}

void* cond_test2(void* arg)
{
    qthread_usleep(100000);
    qthread_mutex_lock(&m);
    for(int i =0; i<5; i++){        
        printf("Thread %s: wait c2\n", (char *)arg);
        qthread_cond_wait(&c2, &m);
        printf("%s\n",(char *)arg);
        qthread_usleep(100000);
        printf("Thread %s: signal c3\n", (char *)arg);
        qthread_cond_signal(&c3);

        printf("Thread %s: wait c2\n", (char *)arg);
        qthread_cond_wait(&c2, &m);
        printf("%s\n",(char *)arg);
        qthread_usleep(100000);
        printf("Thread %s: signal c1\n", (char *)arg);
        qthread_cond_signal(&c1);
    }
    qthread_mutex_unlock(&m);
    return arg;
}

void* cond_test3(void* arg)
{
    qthread_usleep(100000);
    qthread_mutex_lock(&m);
    for(int i =0; i<5; i++){        
        printf("Thread %s: wait c3\n", (char *)arg);
        qthread_cond_wait(&c3, &m);
        printf("%s\n",(char *)arg);
        qthread_usleep(100000);
        printf("Thread %s: signal c2\n", (char *)arg);
        qthread_cond_signal(&c2);
    }
    qthread_mutex_unlock(&m);
    return arg;
}

void cond_test(void)
{
    printf("\nTesting cond with 3 threads\n");
    qthread_t t[3];
    qthread_mutex_init(&m);
    t[0] = qthread_create(cond_test1, "1");
    t[1] = qthread_create(cond_test2, "2");
    t[2] = qthread_create(cond_test3, "3");
    qthread_run();
    return;
}  

void* cond_test4(void* arg)
{
    qthread_usleep(100000);
    qthread_mutex_lock(&m);     
    printf("Thread %s: wait c1\n", (char *)arg);
    qthread_cond_wait(&c1, &m);
    printf("%s\n",(char *)arg);
    qthread_usleep(100000);
    qthread_mutex_unlock(&m);
    return arg;
}
void* cond_test5(void* arg)
{
    qthread_usleep(100000);
    printf("Waking up all threads\n");
    qthread_cond_broadcast(&c1);
    printf("%s\n",(char *)arg);
    qthread_usleep(100000);
    qthread_yield();
    return arg;
}
void cond_broadcast_test(void)
{
    printf("\nTesting cond broadcast with 3 threads\n");
    qthread_t t[3];
    qthread_mutex_init(&m);
    qthread_cond_init(&c1);
    t[0] = qthread_create(cond_test4, "1");
    t[1] = qthread_create(cond_test4, "2");
    t[2] = qthread_create(cond_test5, "3");
    qthread_run();    
    return;
} 

void *read_test(void * arg)
{
    char buf;
    int *fd = arg;
    int val = qthread_read(fd[0], &buf, 1);
    if (buf == 'c')
        printf("Read %c\n", buf);
    return arg;
}

void *write_test(void * arg)
{
    char buf = 'c';
    int *fd =arg;
    int val = qthread_write(fd[1], &buf, 1);
    printf("Writing %c\n", buf);
    qthread_usleep(100000);
    return arg;
}

void io_test(void)
{
    printf("\nTesting IO\n");
    int fd[2];
    pipe(fd);
    qthread_t t[2];
    t[0] = qthread_create(read_test, fd);
    t[1] = qthread_create(write_test, fd);
    qthread_run();
    close(fd[0]);
    close(fd[1]);

}

void *run_test1(void* arg)
{
    for(int i =0;i< 5; i++)
    {
        printf("%s\n", (char*)arg);
        qthread_usleep(10000);
    }
    return arg;
}

void *join_test1(void * arg)
{
    char * val;
    qthread_t *t = arg;
    qthread_t tt = qthread_create(run_test1, "3");
        val= qthread_join(tt);
        printf("%s\n", val);
    return 0;

}
        
void join_test(void)
{
    printf("\nTesting join with 3 threads\n");
    qthread_t t[3];
    t[0] = qthread_create(run_test1, "1");
    t[1] = qthread_create(run_test1, "2");
    t[2] = qthread_create(join_test1, t);

    qthread_run();    
    qthread_join(t[2]);
    return;
} 

int main(int argc, char** argv)
{
    printf("Starting tests..\n");
    mutex_test_3_threads();
    cond_test(); 
    cond_broadcast_test();
    io_test();
    join_test();
    printf("Tests Completed\n");
    return 0;
}
