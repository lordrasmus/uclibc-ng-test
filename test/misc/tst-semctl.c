#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <time.h>
#include <unistd.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

struct timespec ts = {
  .tv_sec = 3468960000,  // 3468960000 2075-12-05 Destination timestamp
  .tv_nsec = 0
};

void print_semid_ds(struct semid_ds *ds) {
    printf("sem_perm.uid: %d\n", ds->sem_perm.uid);
    printf("sem_perm.gid: %d\n", ds->sem_perm.gid);
    printf("sem_perm.cuid: %d\n", ds->sem_perm.cuid);
    printf("sem_perm.cgid: %d\n", ds->sem_perm.cgid);
    printf("sem_perm.mode: %o\n", ds->sem_perm.mode);
    printf("sem_nsems: %d\n", ds->sem_nsems);
    printf("sem_otime: %s", ctime(&ds->sem_otime));
    printf("sem_ctime: %s \n", ctime(&ds->sem_ctime));
}

/* The time64 fixup in semctl() writes two 64-bit words at offsets 56 and 64 of
   whatever the fourth argument points at.  40 semaphores make the array 80
   bytes, so a stray write stays inside memory we own and shows up as wrong
   values instead of a corrupted neighbour -- no sanitizer needed, and none of
   the cross toolchains has one.  */
#define NSEMS 40

/* No fourth argument is defined for these three. */
static const int noarg_cmds[] = { GETPID, GETNCNT, GETZCNT };
static const char *const noarg_names[] = { "GETPID", "GETNCNT", "GETZCNT" };

/* Position dependent, and injective over the buffer: a stray write that
   copies bytes from one offset of the buffer to another is then visible
   whatever the byte order.  A constant fill would hide exactly that.  */
#define CANARY(i) ((unsigned char) ((i) ^ 0x5a))

static int check_canary(unsigned char *buf, int from, int len, const char *who)
{
    int i;

    for (i = from; i < len; i++)
        if (buf[i] != CANARY(i)) {
            printf("%s wrote 0x%02x at offset %d, expected 0x%02x\n",
                   who, buf[i], i, CANARY(i));
            return 1;
        }
    return 0;
}

static void fill_canary(unsigned char *buf, int len)
{
    int i;

    for (i = 0; i < len; i++)
        buf[i] = CANARY(i);
}

static int check_other_cmds(void)
{
    unsigned short vals[NSEMS], readback[NSEMS];
    unsigned char canary[128];
    union semun arg;
    struct sembuf op = { .sem_num = 0, .sem_op = 1, .sem_flg = 0 };
    int semid, i, ret, bad = 0;

    if ((semid = semget(IPC_PRIVATE, NSEMS, 0666 | IPC_CREAT)) == -1) {
        perror("semget NSEMS");
        return 1;
    }

    for (i = 0; i < NSEMS; i++)
        vals[i] = i + 1;
    arg.array = vals;
    if (semctl(semid, 0, SETALL, arg) == -1) {
        perror("semctl SETALL");
        semctl(semid, 0, IPC_RMID);
        return 1;
    }

    /* Compare against the formula, not against vals[]: SETALL may have
       clobbered the source array itself.  */
    arg.array = readback;
    if (semctl(semid, 0, GETALL, arg) == -1) {
        perror("semctl GETALL");
        semctl(semid, 0, IPC_RMID);
        return 1;
    }
    for (i = 0; i < NSEMS; i++)
        if (readback[i] != (unsigned short) (i + 1)) {
            printf("GETALL: element %d is %u, expected %d\n",
                   i, readback[i], i + 1);
            bad = 1;
        }

    /* GETVAL is not caught by the faulty command test, so it must agree. */
    for (i = 0; i < NSEMS; i++) {
        ret = semctl(semid, i, GETVAL, arg);
        if (ret != i + 1) {
            printf("GETVAL: semaphore %d is %d, expected %d\n", i, ret, i + 1);
            bad = 1;
        }
    }

    if (semop(semid, &op, 1) == -1) {
        perror("semop");
        semctl(semid, 0, IPC_RMID);
        return 1;
    }

    /* Passing a fourth argument anyway keeps a stray write inside the canary
       instead of wherever a leftover register happens to point.  */
    for (i = 0; i < 3; i++) {
        fill_canary(canary, sizeof canary);
        arg.array = (unsigned short *) canary;
        ret = semctl(semid, 0, noarg_cmds[i], arg);
        if (ret == -1) {
            printf("semctl %s: %s\n", noarg_names[i], strerror(errno));
            bad = 1;
            continue;
        }
        bad |= check_canary(canary, 0, sizeof canary, noarg_names[i]);
        if (noarg_cmds[i] == GETPID && ret != (int) getpid()) {
            printf("GETPID returned %d, expected %d\n", ret, (int) getpid());
            bad = 1;
        }
        if (noarg_cmds[i] == GETNCNT && ret != 0) {
            printf("GETNCNT returned %d, expected 0\n", ret);
            bad = 1;
        }
    }

#ifdef IPC_INFO
    /* IPC_INFO fills a struct seminfo, 40 bytes -- everything behind it must
       stay untouched. */
    fill_canary(canary, sizeof canary);
    arg.array = (unsigned short *) canary;
    if (semctl(semid, 0, IPC_INFO, arg) != -1)
        bad |= check_canary(canary, sizeof(struct seminfo), sizeof canary,
                            "IPC_INFO");
#endif

    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID NSEMS");
        return 1;
    }
    return bad;
}

int main() {
    int semid;
    union semun arg;
    struct semid_ds ds;
    struct timespec ts_init, ts_final;

    /* Before the clock is moved: a failure here must not leave the system
       time in 2075. */
    if (check_other_cmds() != 0)
        return EXIT_FAILURE;

    /* The post-2038 date only fits into a 64-bit time_t; the ctl mechanics
       below are checked either way.  */
    int time64 = sizeof(time_t) >= 8;
    time_t ref;

    // Save system time
    if (clock_gettime(CLOCK_REALTIME, &ts_init) == -1) {
        perror("Error getting time");
        return 1;
    }
    ref = ts_init.tv_sec;

    if (time64) {
        if (clock_settime(CLOCK_REALTIME, &ts) == -1) { // Set the time to after 2038
            perror("Error setting time");
            return 1;
        }
        ref = ts.tv_sec;
    } else {
        printf("32-bit time_t: skipping the post-2038 part\n");
    }

    // Create a semaphore set
    if ((semid = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT)) == -1) {
        perror("semget failed");
        exit(1);
    }

    // Get the semid_ds structure
    arg.buf = &ds;
    if (semctl(semid, 0, IPC_STAT, arg) == -1) {
        perror("semctl IPC_STAT failed");
        exit(1);
    }

    // Print the structure contents
    printf("=== semid_ds structure values ===\n");
    print_semid_ds(&ds);

    /* semget() asked for exactly one semaphore.  Reading anything else means
       semid_ds does not match the kernel's layout -- which is how riscv64 was
       found, where sem_nsems read a pad word.  */
    if (ds.sem_nsems != 1) {
        printf("sem_nsems is %lu, expected 1\n", (unsigned long) ds.sem_nsems);
        semctl(semid, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }


    /* One semop, so that sem_otime is set at all -- it printed the epoch in
       every run so far because nothing in this suite ever operated on a
       semaphore.  */
    struct sembuf op = { .sem_num = 0, .sem_op = 1, .sem_flg = 0 };
    if (semop(semid, &op, 1) == -1) {
        perror("semop");
        semctl(semid, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }
    if (semctl(semid, 0, IPC_STAT, arg) == -1) {
        perror("semctl IPC_STAT after semop failed");
        semctl(semid, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }
    if ((ds.sem_otime - ref > 60) || (ref - ds.sem_otime > 60)) {
        printf("sem_otime is %ld after semop, reference is %ld\n",
               (long) ds.sem_otime, (long) ref);
        semctl(semid, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }

    // Change permissions
    ds.sem_perm.mode = 0600;  // Change to new permissions

    if (semctl(semid, 0, IPC_SET, arg) == -1) {
        perror("semctl IPC_SET failed");
        semctl(semid, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }

    // Print the structure contents
    printf("=== semid_ds structure values ===\n");
    print_semid_ds(&ds);

    if ((ds.sem_ctime - ref > 60) || (ref - ds.sem_ctime > 60)) {
        printf("\nSemctl get a error time! \n");
        exit(EXIT_FAILURE);
    }

    // Delete a semaphore
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID failed");
        exit(1);
    }

    // Restore system time
    if (time64) {
        clock_gettime(CLOCK_REALTIME, &ts_final);
        ts_init.tv_sec = ts_init.tv_sec + ts_final.tv_sec - ts.tv_sec;
        clock_settime(CLOCK_REALTIME, &ts_init);
    }

    return 0;
}
