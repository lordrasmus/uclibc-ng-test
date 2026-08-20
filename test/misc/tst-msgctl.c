#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// Define the message structure
struct message {
  long mtype;       // Message Type
  char mtext[100];  // Message body
};

struct timespec ts = {
  .tv_sec = 3468960000,  // 2075-12-05 Destination timestamp
  .tv_nsec = 0
};

void print_msqid_ds(struct msqid_ds *buf) {
  printf("perms: %o\n", buf->msg_perm.mode);
  printf("UID: %d\n", buf->msg_perm.uid);
  printf("GID: %d\n", buf->msg_perm.gid);
  printf("Current number of bytes in the queue:  %d\n", buf->msg_cbytes);
  printf("Number of messages in the queue:  %d\n", buf->msg_qnum);
  printf("Maximum number of bytes allowed in the queue: %d\n", buf->msg_qbytes);
  printf("Last sent time:  %s", buf->msg_stime ? ctime(&buf->msg_stime) + 4 : "Not set \n");
  printf("Last received time:  %s", buf->msg_rtime ? ctime(&buf->msg_rtime) + 4 : "Not set \n");
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

  key_t key = ftok(".", 123);
  if (key == -1) {
    perror("ftok");
    return 1;
  }

  int msqid = msgget(key, 0644 | IPC_CREAT);  // Set to write/read only (not full permissions)
  if (msqid == -1) {
    perror("msgget");
    return 1;
  }

  // Get message queue status
  struct msqid_ds buf;
  memset(&buf, 0, sizeof(buf));  // Clear the structure
  if (msgctl(msqid, IPC_STAT, &buf) == -1) {
    perror("msgctl");
    return 1;
  }

  // Print message queue information
  printf("=== Initial queue status ===\n");
  printf("key: %x\n", key);
  printf("msqid: %d\n", msqid);
  print_msqid_ds(&buf);

  // Prepare the message to be sent
  struct message msg = {0};
  msg.mtype = 1;  // Set the message type to 1
  int i =0;
  size_t sent_bytes = 0;

  for(i =0; i< 2; i++)
  {
  snprintf(msg.mtext, sizeof(msg.mtext), "Hello, Message Queue %d!", i);
  msg.mtext[sizeof(msg.mtext) - 1] = '\0';  // Ensure the message ends with a '\0'

  // Send the message
  if (msgsnd(msqid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
    perror("msgsnd");
    return 1;
  }
  printf("Message sent: %s\n", msg.mtext);
  sent_bytes += strlen(msg.mtext) + 1;

  // Check the queue status again
  memset(&buf, 0, sizeof(buf));  // Clear the structure
  if (msgctl(msqid, IPC_STAT, &buf) == -1) {
    perror("msgctl");
    return 1;
  }

  printf("\n=== Queue status after the message is sent ===\n");
  print_msqid_ds(&buf);

  /* Every field the kernel fills is a check on the structure layout, and a
     shifted layout is the failure this test keeps meeting.  Only assert what
     is predictable: the counters, the sizes and the sender's pid.  */
  if (buf.msg_qnum != (msgqnum_t) (i + 1)) {
      printf("\nmsg_qnum is %lu, expected %d\n",
             (unsigned long) buf.msg_qnum, i + 1);
      msgctl(msqid, IPC_RMID, NULL);
      exit(EXIT_FAILURE);
  }
  if (buf.msg_cbytes != sent_bytes) {
      printf("\nmsg_cbytes is %lu, expected %zu\n",
             (unsigned long) buf.msg_cbytes, sent_bytes);
      msgctl(msqid, IPC_RMID, NULL);
      exit(EXIT_FAILURE);
  }
  if (buf.msg_lspid != getpid()) {
      printf("\nmsg_lspid is %d, expected %d\n",
             (int) buf.msg_lspid, (int) getpid());
      msgctl(msqid, IPC_RMID, NULL);
      exit(EXIT_FAILURE);
  }
  }

  /* Receive one of them: msgrcv is the only way msg_rtime and msg_lrpid ever
     get set, so without it those two fields cannot be checked at all.  It also
     exercises the receive path itself, which no test in this suite did.  */
  struct message got = {0};
  ssize_t rlen = msgrcv(msqid, &got, sizeof(got.mtext), 1, 0);
  if (rlen < 0) {
      perror("msgrcv");
      msgctl(msqid, IPC_RMID, NULL);
      exit(EXIT_FAILURE);
  }
  printf("\nMessage received: %s\n", got.mtext);
  if (got.mtype != 1 || strcmp(got.mtext, "Hello, Message Queue 0!") != 0) {
      printf("\nreceived mtype %ld text \"%s\", expected 1 and the first message\n",
             (long) got.mtype, got.mtext);
      msgctl(msqid, IPC_RMID, NULL);
      exit(EXIT_FAILURE);
  }

  memset(&buf, 0, sizeof(buf));
  if (msgctl(msqid, IPC_STAT, &buf) == -1) {
      perror("msgctl");
      msgctl(msqid, IPC_RMID, NULL);
      exit(EXIT_FAILURE);
  }
  print_msqid_ds(&buf);
  if (buf.msg_qnum != 1) {
      printf("\nmsg_qnum is %lu after receiving one of two, expected 1\n",
             (unsigned long) buf.msg_qnum);
      msgctl(msqid, IPC_RMID, NULL);
      exit(EXIT_FAILURE);
  }
  if (buf.msg_lrpid != getpid()) {
      printf("\nmsg_lrpid is %d, expected %d\n",
             (int) buf.msg_lrpid, (int) getpid());
      msgctl(msqid, IPC_RMID, NULL);
      exit(EXIT_FAILURE);
  }
  if ((buf.msg_rtime - ref > 60) || (ref - buf.msg_rtime > 60)) {
      printf("\nmsg_rtime is off: %ld against %ld\n",
             (long) buf.msg_rtime, (long) ref);
      msgctl(msqid, IPC_RMID, NULL);
      exit(EXIT_FAILURE);
  }

  // Change permissions
  buf.msg_perm.mode = 0600;  // New permissions

  if (msgctl(msqid, IPC_SET, &buf) == -1) {
      perror("msgctl IPC_SET failed");
      msgctl(msqid, IPC_RMID, NULL);
      exit(EXIT_FAILURE);
  }

  if ((buf.msg_stime - ref > 60) || (ref - buf.msg_stime > 60)) {
      printf("\nMsgctl get a error time! \n");
      exit(EXIT_FAILURE);
  }

  if (msgctl(msqid, IPC_RMID, NULL) == -1) {
      perror("msgctl IPC_RMID failed");
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
