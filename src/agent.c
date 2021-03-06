#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#include "agent.h"
#include "error.h"

struct agent_args {
  struct mailbox *inbox;
  bool (*handler) (struct message*);
};

void *agent (void *data) {
  struct agent_args *args = (struct agent_args*) data;
  struct mailbox *inbox = args->inbox;
  bool block = true;
  bool stop = false;
  while (!stop && !panic) {
    struct message *mail = get_mail(inbox, block);
    if (mail != NULL && mail->type == STOP_MESSAGE_TYPE) {
      stop = true;
    } else {
      // maybe `get_mail' generated some panic
      if (!panic)
        block = args->handler(mail);
    }
    if (mail != NULL) {
      if (mail->data != NULL) {
        free(mail->data);
      }
      free(mail);
    }
  }

  struct message *stop_m = get_stop_message();
  if (stop_m != NULL) {
    args->handler(stop_m);
    free(stop_m);
  }
  free(args);
  return NULL;
}

struct mailbox* make_agent(pthread_t *thread, bool (*handler) (struct message*)) {
  struct agent_args *args = (struct agent_args*) malloc(sizeof(struct agent_args));
  if (args == NULL) {
    myperror("malloc");
    return NULL;
  }
  args->inbox = new_mailbox();
  if (args->inbox == NULL) {
    free(args);
    return NULL;
  }
  args->handler = handler;
  int err = pthread_create(thread, NULL, agent, args);
  if (err != 0) {
    myerror(err, "pthread_create");
    delete_mailbox(args->inbox);
    free(args);
    return NULL;
  }
  return args->inbox;
}
