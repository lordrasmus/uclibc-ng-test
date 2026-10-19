/* libthread_db must map every thread of a stopped process to its struct
   pthread, the way gdb does it.  The thread-pointer offsets it uses come
   from DB_THREAD_SELF in the port's tls.h and have to match THREAD_SELF;
   nothing else in the library reads them, so a wrong one only shows in a
   debugger.

   The parent plays the debugger: a forked child starts some threads, each
   reports its kernel tid, pthread_self () and thread pointer, and the
   parent attaches to all of them with ptrace and asks libthread_db.  As in
   gdb, registers come from PTRACE_GETREGSET.  For the ports that describe
   the thread pointer as a constant offset (CONST_THREAD_AREA), gdb fetches
   the thread pointer with a port-specific ptrace request; the test takes
   the value the thread reported instead, since what is under test is the
   offset, not gdb.  */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

/* <thread_db.h> is only installed with PTHREADS_DEBUG_SUPPORT.  */
#ifdef __has_include
# if !__has_include (<thread_db.h>)
#  define NO_THREAD_DB_H
# endif
#endif

#ifdef NO_THREAD_DB_H
#include <stdio.h>

int main (void)
{
  puts ("SKIP: no <thread_db.h>, built without PTHREADS_DEBUG_SUPPORT");
  return 0;
}
#else
#include <dlfcn.h>
#include <link.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <thread_db.h>

#ifndef PTRACE_GETREGSET
# define PTRACE_GETREGSET 0x4204
#endif
#ifndef NT_PRSTATUS
# define NT_PRSTATUS 1
#endif

/* What libthread_db expects of its debugger, from proc_service.h, which
   comes with gdb rather than with the C library.  */
typedef enum
{
  PS_OK, PS_ERR, PS_BADPID, PS_BADLID, PS_BADADDR, PS_NOSYM, PS_NOFREGS
} ps_err_e;

struct ps_prochandle
{
  pid_t pid;
  int mem;
};

#define NTHREADS 4

struct thread_report
{
  int i;
  pid_t tid;
  void *self;
  void *tp;
};

struct report
{
  struct thread_report t[NTHREADS + 1];
};

static struct report report, *rep = &report;

/* The threads report through a pipe: no atomics needed (arc700 has none
   in libgcc), and a short write to a pipe is atomic.  */
static int report_pipe[2];

/* Only the ports whose DB_THREAD_SELF is CONST_THREAD_AREA or
   REGISTER_THREAD_AREA ask for the thread pointer, each read the way
   uClibc-ng's own tls.h reads it.  */
#if defined __x86_64__
# include <asm/prctl.h>
static void *thread_pointer (void)
{
  unsigned long base = 0;
  syscall (SYS_arch_prctl, ARCH_GET_FS, &base);
  return (void *) base;
}
#elif defined __i386__
static void *thread_pointer (void)
{
  void *p;
  __asm__ ("movl %%gs:0,%0" : "=r" (p));	/* header.tcb points to itself */
  return p;
}
#elif defined __aarch64__ || defined __arm__ || defined __arc__ \
      || defined __metag__
static void *thread_pointer (void)
{
  return __builtin_thread_pointer ();
}
#elif defined __mips__
static void *thread_pointer (void)
{
  void *p;
  __asm__ volatile (".set push\n\t.set mips32r2\n\t"
		    "rdhwr %0, $29\n\t.set pop" : "=v" (p));
  return p;
}
#elif defined __microblaze__
static void *thread_pointer (void)
{
  void *p;
  __asm__ volatile ("addk %0, r21, r0" : "=r" (p));
  return p;
}
#elif defined __xtensa__
static void *thread_pointer (void)
{
  void *p;
  __asm__ volatile ("rur %0, threadptr" : "=r" (p));
  return p;
}
#elif defined __CSKYABIV2__
static void *thread_pointer (void)
{
  void *p;
  __asm__ volatile ("mov %0, r31" : "=r" (p));
  return p;
}
#elif defined __kvx__
static void *thread_pointer (void)
{
  void *p;
  __asm__ volatile ("copyd %0 = $r13" : "=r" (p));
  return p;
}
#elif defined __m68k__
static void *thread_pointer (void)
{
  return (void *) syscall (SYS_get_thread_area);
}
#else
static void *thread_pointer (void)
{
  return NULL;	/* never asked for: the port reads a register */
}
#endif

static void record (int i)
{
  struct thread_report r;

  r.i = i;
  r.tid = syscall (SYS_gettid);
  r.self = (void *) pthread_self ();
  r.tp = thread_pointer ();
  if (write (report_pipe[1], &r, sizeof r) != sizeof r)
    _exit (3);
}

static void *thread_main (void *arg)
{
  record ((int) (intptr_t) arg);
  for (;;)
    pause ();
  return NULL;
}

static void child (void)
{
  pthread_t th;
  int i;

  for (i = 1; i <= NTHREADS; i++)
    if (pthread_create (&th, NULL, thread_main, (void *) (intptr_t) i) != 0)
      _exit (2);
  record (0);
  for (;;)
    pause ();
}

/* The debugger side.  */

static struct ps_prochandle ph;

/* gdb looks symbols up in the full .symtab of each object, and some of the
   ones libthread_db wants are static (nptl_version), so dlsym alone does
   not do.  The child is a fork of this process: same objects, same
   addresses.  */
struct symsearch
{
  const char *name;
  void *addr;
};

static int search_object (struct dl_phdr_info *info, size_t size, void *data)
{
  struct symsearch *ss = data;
  const char *path = info->dlpi_name;
  ElfW(Ehdr) eh;
  ElfW(Shdr) *sh = NULL;
  char *strtab = NULL;
  int fd, i;

  (void) size;
  if (path == NULL || path[0] == '\0')
    path = "/proc/self/exe";
  fd = open (path, O_RDONLY);
  if (fd < 0)
    return 0;
  if (pread (fd, &eh, sizeof eh, 0) != sizeof eh
      || (sh = malloc (eh.e_shnum * sizeof *sh)) == NULL
      || pread (fd, sh, eh.e_shnum * sizeof *sh, eh.e_shoff)
	 != (ssize_t) (eh.e_shnum * sizeof *sh))
    goto out;
  for (i = 0; i < eh.e_shnum && ss->addr == NULL; i++)
    {
      ElfW(Shdr) *str = &sh[sh[i].sh_link];
      ElfW(Sym) sym;
      size_t k;

      if (sh[i].sh_type != SHT_SYMTAB)
	continue;
      free (strtab);
      if ((strtab = malloc (str->sh_size)) == NULL
	  || pread (fd, strtab, str->sh_size, str->sh_offset)
	     != (ssize_t) str->sh_size)
	goto out;
      for (k = 0; k < sh[i].sh_size / sizeof sym; k++)
	if (pread (fd, &sym, sizeof sym, sh[i].sh_offset + k * sizeof sym)
	    == sizeof sym
	    && sym.st_name < str->sh_size && sym.st_shndx != SHN_UNDEF
	    && strcmp (strtab + sym.st_name, ss->name) == 0)
	  {
	    ss->addr = (void *) (info->dlpi_addr + sym.st_value);
	    break;
	  }
    }
out:
  free (strtab);
  free (sh);
  close (fd);
  return ss->addr != NULL;
}

ps_err_e ps_pglobal_lookup (struct ps_prochandle *h, const char *obj,
			    const char *name, psaddr_t *addr)
{
  struct symsearch ss = { name, NULL };

  (void) h; (void) obj;
  ss.addr = dlsym (RTLD_DEFAULT, name);
  if (ss.addr == NULL)
    dl_iterate_phdr (search_object, &ss);
  if (ss.addr == NULL)
    return PS_NOSYM;
  *addr = ss.addr;
  return PS_OK;
}

ps_err_e ps_pdread (struct ps_prochandle *h, psaddr_t addr, void *buf,
		    size_t size)
{
  return pread (h->mem, buf, size, (off_t) (uintptr_t) addr)
	 == (ssize_t) size ? PS_OK : PS_ERR;
}

ps_err_e ps_pdwrite (struct ps_prochandle *h, psaddr_t addr, const void *buf,
		     size_t size)
{
  return pwrite (h->mem, buf, size, (off_t) (uintptr_t) addr)
	 == (ssize_t) size ? PS_OK : PS_ERR;
}

ps_err_e ps_lgetregs (struct ps_prochandle *h, lwpid_t lwp,
		      prgregset_t regs)
{
  struct iovec iov = { regs, sizeof (prgregset_t) };

  (void) h;
  return ptrace (PTRACE_GETREGSET, lwp, (void *) NT_PRSTATUS, &iov) == 0
	 ? PS_OK : PS_ERR;
}

ps_err_e ps_lsetregs (struct ps_prochandle *h, lwpid_t lwp,
		      const prgregset_t regs)
{
  (void) h; (void) lwp; (void) regs;
  return PS_ERR;
}

ps_err_e ps_lgetfpregs (struct ps_prochandle *h, lwpid_t lwp,
			prfpregset_t *regs)
{
  (void) h; (void) lwp; (void) regs;
  return PS_ERR;
}

ps_err_e ps_lsetfpregs (struct ps_prochandle *h, lwpid_t lwp,
			const prfpregset_t *regs)
{
  (void) h; (void) lwp; (void) regs;
  return PS_ERR;
}

pid_t ps_getpid (struct ps_prochandle *h)
{
  return h->pid;
}

/* gdb's meaning per port: x86-64 hands back the fs base, i386 the base of
   the descriptor IDX names, everybody else the thread pointer minus IDX.  */
ps_err_e ps_get_thread_area (const struct ps_prochandle *h, lwpid_t lwp,
			     int idx, void **base)
{
  int i;

  (void) h;
  for (i = 0; i <= NTHREADS; i++)
    if (rep->t[i].tid == lwp)
      {
#if defined __x86_64__ || defined __i386__
	(void) idx;
	*base = rep->t[i].tp;
#else
	*base = (char *) rep->t[i].tp - idx;
#endif
	return PS_OK;
      }
  return PS_BADLID;
}

int main (void)
{
  td_err_e (*p_td_init) (void);
  td_err_e (*p_td_ta_new) (struct ps_prochandle *, td_thragent_t **);
  td_err_e (*p_td_ta_map_lwp2thr) (const td_thragent_t *, lwpid_t,
				   td_thrhandle_t *);
  td_err_e (*p_td_thr_get_info) (const td_thrhandle_t *, td_thrinfo_t *);
  td_thragent_t *ta;
  void *db;
  char path[64];
  pid_t pid;
  int i, err, bad = 0;

  db = dlopen ("libthread_db.so.1", RTLD_NOW);
  if (db == NULL)
    {
      printf ("SKIP: no libthread_db (%s)\n", dlerror ());
      return 0;
    }
  p_td_init = dlsym (db, "td_init");
  p_td_ta_new = dlsym (db, "td_ta_new");
  p_td_ta_map_lwp2thr = dlsym (db, "td_ta_map_lwp2thr");
  p_td_thr_get_info = dlsym (db, "td_thr_get_info");
  if (!p_td_init || !p_td_ta_new || !p_td_ta_map_lwp2thr || !p_td_thr_get_info)
    {
      puts ("libthread_db lacks td_* functions");
      return 1;
    }

  if (pipe (report_pipe) != 0)
    {
      perror ("pipe");
      return 1;
    }
  pid = fork ();
  if (pid == 0)
    child ();
  close (report_pipe[1]);
  for (i = 0; i <= NTHREADS; i++)
    {
      struct thread_report r;

      if (read (report_pipe[0], &r, sizeof r) != sizeof r
	  || r.i < 0 || r.i > NTHREADS)
	{
	  puts ("child did not report all threads");
	  kill (pid, SIGKILL);
	  waitpid (pid, NULL, 0);
	  return 1;
	}
      rep->t[r.i] = r;
    }

  for (i = 0; i <= NTHREADS; i++)
    {
      if (ptrace (PTRACE_ATTACH, rep->t[i].tid, NULL, NULL) != 0)
	{
	  printf ("SKIP: ptrace attach: %s\n", strerror (errno));
	  kill (pid, SIGKILL);
	  while (waitpid (-1, NULL, __WALL) > 0)
	    ;
	  return 0;
	}
      waitpid (rep->t[i].tid, NULL, __WALL);
    }
  snprintf (path, sizeof path, "/proc/%d/mem", (int) pid);
  ph.pid = pid;
  ph.mem = open (path, O_RDWR);

  if ((err = p_td_init ()) != TD_OK)
    {
      printf ("td_init failed: %d\n", err);
      bad = 1;
    }
  else if ((err = p_td_ta_new (&ph, &ta)) == TD_NOLIBTHREAD)
    /* Like gdb, libthread_db needs the static symbols of the thread
       library, which a stripped one no longer has.  */
    puts ("SKIP: libthread_db finds no thread library (stripped?)");
  else if (err != TD_OK)
    {
      printf ("td_ta_new failed: %d\n", err);
      bad = 1;
    }
  else
    for (i = 0; i <= NTHREADS; i++)
      {
	td_thrhandle_t th;
	td_thrinfo_t info;

	err = p_td_ta_map_lwp2thr (ta, rep->t[i].tid, &th);
	if (err != TD_OK)
	  {
	    printf ("thread %d: td_ta_map_lwp2thr failed: %d\n", i, err);
	    bad = 1;
	    continue;
	  }
	if ((void *) th.th_unique != rep->t[i].self)
	  {
	    printf ("thread %d: libthread_db says struct pthread at %p, "
		    "pthread_self () is %p\n", i, (void *) th.th_unique,
		    rep->t[i].self);
	    bad = 1;
	    continue;
	  }
	err = p_td_thr_get_info (&th, &info);
	if (err != TD_OK || info.ti_lid != rep->t[i].tid)
	  {
	    printf ("thread %d: td_thr_get_info: err %d, lid %d, want %d\n",
		    i, err, (int) info.ti_lid, (int) rep->t[i].tid);
	    bad = 1;
	  }
      }

  /* Every attached thread is ours to reap, not only the process.  */
  kill (pid, SIGKILL);
  while (waitpid (-1, NULL, __WALL) > 0)
    ;
  return bad;
}
#endif
