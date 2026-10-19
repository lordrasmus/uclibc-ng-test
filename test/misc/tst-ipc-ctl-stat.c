#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>

union semun {
    int val;
    struct semid_ds *buf;
    struct seminfo *__buf;
};

/* Compare public fields, not padding or private time64 backing words.
   Each object belongs to this test and is idle while both calls run. */
#define CHECK_FIELD(field) do { \
    if (got.field != want.field) { \
        printf("%s differs between IPC_STAT and *_STAT\n", #field); \
        bad = 1; \
    } \
} while (0)

#define CHECK_PERM(field) do { \
    CHECK_FIELD(field.uid); \
    CHECK_FIELD(field.gid); \
    CHECK_FIELD(field.cuid); \
    CHECK_FIELD(field.cgid); \
    CHECK_FIELD(field.mode); \
} while (0)

static int check_sem(int id)
{
    struct semid_ds want, got;
    union { struct seminfo info; struct semid_ds ds; } info;
    struct sembuf op = { 0, 1, 0 };
    union semun arg;
    int index, max, ret, bad = 0;

    if (semop(id, &op, 1) == -1) {
        perror("semop before SEM_STAT");
        return 1;
    }
    memset(&want, 0, sizeof want);
    arg.buf = &want;
    if (semctl(id, 0, IPC_STAT, arg) == -1) {
        perror("semctl IPC_STAT");
        return 1;
    }
    arg.__buf = &info.info;
    max = semctl(0, 0, IPC_INFO, arg);
    if (max == -1) {
        perror("semctl IPC_INFO");
        return 1;
    }
    for (index = 0; index <= max; index++) {
        memset(&got, 0, sizeof got);
        arg.buf = &got;
        ret = semctl(index, 0, SEM_STAT, arg);
        if (ret != id)
            continue;
        CHECK_PERM(sem_perm);
        CHECK_FIELD(sem_nsems);
        CHECK_FIELD(sem_otime);
        CHECK_FIELD(sem_ctime);
        return bad;
    }
    printf("SEM_STAT did not find semaphore id %d\n", id);
    return 1;
}

static int check_msg(int id)
{
    struct msqid_ds want, got;
    union { struct msginfo info; struct msqid_ds ds; } info;
    struct { long type; char text[7]; } msg = { 1, "abcdef" };
    int index, max, ret, bad = 0;

    /* Leave one message queued, with both send and receive times set. */
    if (msgsnd(id, &msg, sizeof msg.text, 0) == -1
        || msgrcv(id, &msg, sizeof msg.text, 1, 0) == -1
        || msgsnd(id, &msg, sizeof msg.text, 0) == -1) {
        perror("message operations before MSG_STAT");
        return 1;
    }
    memset(&want, 0, sizeof want);
    if (msgctl(id, IPC_STAT, &want) == -1) {
        perror("msgctl IPC_STAT");
        return 1;
    }
    max = msgctl(0, IPC_INFO, (struct msqid_ds *) (void *) &info.info);
    if (max == -1) {
        perror("msgctl IPC_INFO");
        return 1;
    }
    for (index = 0; index <= max; index++) {
        memset(&got, 0, sizeof got);
        ret = msgctl(index, MSG_STAT, &got);
        if (ret != id)
            continue;
        CHECK_PERM(msg_perm);
        CHECK_FIELD(msg_cbytes);
        CHECK_FIELD(msg_qnum);
        CHECK_FIELD(msg_qbytes);
        CHECK_FIELD(msg_lspid);
        CHECK_FIELD(msg_lrpid);
        CHECK_FIELD(msg_stime);
        CHECK_FIELD(msg_rtime);
        CHECK_FIELD(msg_ctime);
        return bad;
    }
    printf("MSG_STAT did not find message queue id %d\n", id);
    return 1;
}

static int check_shm(int id)
{
    struct shmid_ds want, got;
    union { struct shminfo info; struct shmid_ds ds; } info;
    int index, max, ret, bad = 0;

#if !defined(__UCLIBC__) || defined(__ARCH_USE_MMU__)
    void *addr = shmat(id, NULL, 0);

    if (addr == (void *) -1) {
        perror("shmat before SHM_STAT");
        return 1;
    }
    if (shmdt(addr) == -1) {
        perror("shmdt before SHM_STAT");
        return 1;
    }
#endif
    memset(&want, 0, sizeof want);
    if (shmctl(id, IPC_STAT, &want) == -1) {
        perror("shmctl IPC_STAT");
        return 1;
    }
    max = shmctl(0, IPC_INFO, (struct shmid_ds *) (void *) &info.info);
    if (max == -1) {
        perror("shmctl IPC_INFO");
        return 1;
    }
    for (index = 0; index <= max; index++) {
        memset(&got, 0, sizeof got);
        ret = shmctl(index, SHM_STAT, &got);
        if (ret != id)
            continue;
        CHECK_PERM(shm_perm);
        CHECK_FIELD(shm_segsz);
        CHECK_FIELD(shm_cpid);
        CHECK_FIELD(shm_lpid);
        CHECK_FIELD(shm_nattch);
        CHECK_FIELD(shm_atime);
        CHECK_FIELD(shm_dtime);
        CHECK_FIELD(shm_ctime);
        return bad;
    }
    printf("SHM_STAT did not find shared memory id %d\n", id);
    return 1;
}

int main(void)
{
    int semid, msqid, shmid, bad = 0;

    /* No clock changes and no ownership changes: all failures can clean up
       these private objects, including when the tests run unprivileged. */
    semid = semget(IPC_PRIVATE, 3, IPC_CREAT | 0642);
    msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0642);
    shmid = shmget(IPC_PRIVATE, 1234, IPC_CREAT | 0642);
    if (semid == -1 || msqid == -1 || shmid == -1) {
        perror("creating SysV IPC objects");
        bad = 1;
        goto out;
    }
    bad |= check_sem(semid);
    bad |= check_msg(msqid);
    bad |= check_shm(shmid);
out:
    if (semid != -1 && semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
        bad = 1;
    }
    if (msqid != -1 && msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("msgctl IPC_RMID");
        bad = 1;
    }
    if (shmid != -1 && shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl IPC_RMID");
        bad = 1;
    }
    return bad ? EXIT_FAILURE : EXIT_SUCCESS;
}
