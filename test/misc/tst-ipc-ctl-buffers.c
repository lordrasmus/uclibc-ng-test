#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

enum ctl_kind { CTL_SEM, CTL_MSG, CTL_SHM };

static int semid = -1, msqid = -1, shmid = -1;

union ctl_buffer {
    struct semid_ds sem;
    struct msqid_ds msg;
    struct shmid_ds shm;
};

static int call_ctl_id(enum ctl_kind kind, int id, int cmd, void *buf)
{
    union semun arg;

    switch (kind) {
    case CTL_SEM:
        arg.buf = buf;
        return semctl(id, 0, cmd, arg);
    case CTL_MSG:
        return msgctl(id, cmd, buf);
    default:
        return shmctl(id, cmd, buf);
    }
}

static int call_ctl(enum ctl_kind kind, int cmd, void *buf)
{
    int id = kind == CTL_SEM ? semid : kind == CTL_MSG ? msqid : shmid;

    return call_ctl_id(kind, id, cmd, buf);
}

static const char *kind_name(enum ctl_kind kind)
{
    static const char *const names[] = { "semctl", "msgctl", "shmctl" };
    return names[kind];
}

/* These checks also run on noMMU targets.  A failed IPC_STAT must leave
   its output alone; IPC_SET only reads its input, on success or failure. */
static int check_writable(enum ctl_kind kind)
{
    union ctl_buffer buf, saved;
    struct ipc_perm *perm;
    unsigned char *bytes = (unsigned char *) &buf;
    size_t i;
    int ret, error, bad = 0;

    for (i = 0; i < sizeof buf; i++)
        bytes[i] = (unsigned char) (i ^ 0x5a);
    memcpy(&saved, &buf, sizeof buf);
    errno = 0;
    ret = call_ctl_id(kind, -1, IPC_STAT, &buf);
    error = errno;
    if (ret != -1 || error != EINVAL) {
        printf("%s IPC_STAT(-1): result %d errno %d, expected -1/EINVAL\n",
               kind_name(kind), ret, error);
        bad = 1;
    }
    if (memcmp(&buf, &saved, sizeof buf)) {
        printf("%s failed IPC_STAT changed its output buffer\n", kind_name(kind));
        bad = 1;
    }

    memset(&buf, 0, sizeof buf);
    if (call_ctl(kind, IPC_STAT, &buf) == -1) {
        perror("IPC_STAT before writable IPC_SET");
        return 1;
    }
    perm = kind == CTL_SEM ? &buf.sem.sem_perm
         : kind == CTL_MSG ? &buf.msg.msg_perm : &buf.shm.shm_perm;
    /* The kernel uses the permission bits and ignores the upper bits.
       A library must preserve both in the caller's input object. */
    perm->mode = 0x5a000000U | 0642;
    memcpy(&saved, &buf, sizeof buf);
    if (call_ctl(kind, IPC_SET, &buf) != 0) {
        perror("writable IPC_SET");
        bad = 1;
    }
    if (memcmp(&buf, &saved, sizeof buf)) {
        printf("%s successful IPC_SET changed its input buffer\n", kind_name(kind));
        bad = 1;
    }

    memcpy(&buf, &saved, sizeof buf);
    errno = 0;
    ret = call_ctl_id(kind, -1, IPC_SET, &buf);
    error = errno;
    if (ret != -1 || error != EINVAL) {
        printf("%s IPC_SET(-1): result %d errno %d, expected -1/EINVAL\n",
               kind_name(kind), ret, error);
        bad = 1;
    }
    if (memcmp(&buf, &saved, sizeof buf)) {
        printf("%s failed IPC_SET changed its input buffer\n", kind_name(kind));
        bad = 1;
    }

    memset(&buf, 0, sizeof buf);
    if (call_ctl(kind, IPC_STAT, &buf) == -1) {
        perror("IPC_STAT after writable IPC_SET");
        return 1;
    }
    if ((perm->mode & 0777) != 0642) {
        printf("%s IPC_SET did not set mode 0642\n", kind_name(kind));
        bad = 1;
    }
    return bad;
}

#if !defined(__UCLIBC__) || defined(__ARCH_USE_MMU__)
/* Run a call which may expose an unwanted libc access without losing the
   parent process and its IPC cleanup. */
static int run_child(enum ctl_kind kind, int cmd, void *buf, int want_efault)
{
    pid_t pid = fork();
    int status;

    if (pid == -1) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        int ret;

        errno = 0;
        ret = call_ctl(kind, cmd, buf);
        if (want_efault)
            _exit(ret == -1 && errno == EFAULT ? 0 : 1);
        _exit(ret == 0 ? 0 : 1);
    }
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR)
            continue;
        perror("waitpid");
        return 1;
    }
    if (WIFSIGNALED(status)) {
        if (cmd == IPC_SET)
            printf("%s IPC_SET wrote to its read-only input: signal %d\n",
                   kind_name(kind), WTERMSIG(status));
        else
            printf("%s IPC_STAT accessed its output after EFAULT: signal %d\n",
                   kind_name(kind), WTERMSIG(status));
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("%s %s returned the wrong result\n", kind_name(kind),
               cmd == IPC_SET ? "IPC_SET" : "IPC_STAT");
        return 1;
    }
    return 0;
}

static int check_one(enum ctl_kind kind, size_t len)
{
    long pagesize = sysconf(_SC_PAGESIZE);
    void *page;
    int bad = 0;

    if (pagesize <= 0 || len > (size_t) pagesize) {
        printf("invalid page size for %s test\n", kind_name(kind));
        return 1;
    }
    page = mmap(NULL, pagesize, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    memset(page, 0, len);
    if (call_ctl(kind, IPC_STAT, page) == -1) {
        perror("IPC_STAT before protection");
        munmap(page, pagesize);
        return 1;
    }
    if (mprotect(page, pagesize, PROT_READ) == -1) {
        perror("mprotect PROT_READ");
        munmap(page, pagesize);
        return 1;
    }
    bad |= run_child(kind, IPC_SET, page, 0);

    if (mprotect(page, pagesize, PROT_NONE) == -1) {
        perror("mprotect PROT_NONE");
        munmap(page, pagesize);
        return 1;
    }
    bad |= run_child(kind, IPC_STAT, page, 1);
    munmap(page, pagesize);
    return bad;
}
#endif

int main(void)
{
    int bad = 0;

    semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600);
    if (semid == -1 || msqid == -1 || shmid == -1) {
        perror("creating SysV IPC objects");
        bad = 1;
        goto out;
    }

    bad |= check_writable(CTL_SEM);
    bad |= check_writable(CTL_MSG);
    bad |= check_writable(CTL_SHM);

#if !defined(__UCLIBC__) || defined(__ARCH_USE_MMU__)
    bad |= check_one(CTL_SEM, sizeof(struct semid_ds));
    bad |= check_one(CTL_MSG, sizeof(struct msqid_ds));
    bad |= check_one(CTL_SHM, sizeof(struct shmid_ds));
#else
    puts("no MMU: only the mapping protection checks are skipped");
#endif

out:
    if (semid != -1)
        semctl(semid, 0, IPC_RMID);
    if (msqid != -1)
        msgctl(msqid, IPC_RMID, NULL);
    if (shmid != -1)
        shmctl(shmid, IPC_RMID, NULL);
    return bad ? EXIT_FAILURE : EXIT_SUCCESS;
}
