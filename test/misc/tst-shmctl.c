#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>

struct timespec ts = {
  .tv_sec = 3468960000,  // 2075-12-05 Destination timestamp
  .tv_nsec = 0
};

void print_shmid_ds(struct shmid_ds *buf) {
    printf("shm_perm.uid: %d \n", buf->shm_perm.uid);
    printf("shm_perm.gid: %d \n", buf->shm_perm.gid);
    printf("shm_perm.cuid: %d \n", buf->shm_perm.cuid);
    printf("shm_perm.cgid: %d \n", buf->shm_perm.cgid);
    printf("shm_perm.mode: %o \n", buf->shm_perm.mode);
    printf("shm_segsz: %lu \n", buf->shm_segsz);
    printf("shm_lpid: %d \n", buf->shm_lpid);
    printf("shm_cpid: %d \n", buf->shm_cpid);
    printf("shm_nattch: %lu \n", buf->shm_nattch);
    printf("shm_atime: %s", buf->shm_atime ? ctime(&buf->shm_atime) : "Not set\n");
    printf("shm_dtime: %s", buf->shm_dtime ? ctime(&buf->shm_dtime) : "Not set\n");
    printf("shm_ctime: %s\n", ctime(&buf->shm_ctime));
}

int main() {

    struct timespec ts_init, ts_final;
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

    key_t key = ftok(".", 'S');
    int shmid = shmget(key, 1024, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    struct shmid_ds buf;
    if (shmctl(shmid, IPC_STAT, &buf) == -1) {
        perror("shmctl");
        exit(1);
    }

    printf("Shared Memory Segment Info:\n");
    print_shmid_ds(&buf);

    /* Same idea: shm_segsz must be what shmget() was asked for.  It read 0 on
       hppa while every time field happened to land right, so the test passed
       with a structure that was off by a word.  */
    if (buf.shm_segsz != 1024) {
        printf("\nshm_segsz is %lu, expected 1024\n",
               (unsigned long) buf.shm_segsz);
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    if (buf.shm_cpid != getpid()) {
        printf("\nshm_cpid is %d, expected %d\n",
               (int) buf.shm_cpid, (int) getpid());
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    /* Attach it: shm_nattch, shm_atime and shm_dtime only ever get set by
       shmat/shmdt, and no test in this suite attached a segment at all -- which
       is how a shmat() returning EFAULT stayed unnoticed.

       Only where there is an MMU.  Without one the kernel cannot hand out a
       shared mapping at all and shmat() answers ENODEV -- mm/nommu.c,
       validate_mmap_request().  Keying this on __ARCH_USE_MMU__ rather than on
       "ENODEV means skip" keeps a real ENODEV on an MMU target a failure,
       which is what it is: this suite has already met kernel images that
       silently lacked SysV IPC or inotify.  */
#ifdef __ARCH_USE_MMU__
    char *addr = shmat(shmid, NULL, 0);
    if (addr == (char *) -1) {
        perror("shmat");
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    strcpy(addr, "shm works");
    if (strcmp(addr, "shm works") != 0) {
        printf("\nsegment does not hold what was written to it\n");
        shmdt(addr);
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    if (shmctl(shmid, IPC_STAT, &buf) == -1) {
        perror("shmctl");
        shmdt(addr);
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    if (buf.shm_nattch != 1) {
        printf("\nshm_nattch is %lu while attached, expected 1\n",
               (unsigned long) buf.shm_nattch);
        shmdt(addr);
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    if (shmdt(addr) == -1) {
        perror("shmdt");
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
#endif /* __ARCH_USE_MMU__ */

    // Change to new permissions
    buf.shm_perm.mode = 0600;
    
    if (shmctl(shmid, IPC_SET, &buf) == -1) {
        perror("shmctl IPC_SET failed");
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    if ((buf.shm_ctime - ref > 60) || (ref - buf.shm_ctime > 60)) {
        printf("\nShmctl get a error time! \n");
        exit(EXIT_FAILURE);
    }

    printf("Shared Memory Segment Info:\n");
    print_shmid_ds(&buf);

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl IPC_RMID failed");
        exit(EXIT_FAILURE);
    }

    // Restore system time
    if (time64) {
        clock_gettime(CLOCK_REALTIME, &ts_final);
        ts_init.tv_sec = ts_init.tv_sec + ts_final.tv_sec - ts.tv_sec;
        clock_settime(CLOCK_REALTIME, &ts_init);
    }

    return 0;
}
