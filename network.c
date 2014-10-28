#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "common.h"
#include "timer.h"
#include "network.h"

struct network_message {
  struct network_message *next;
  long time;
  struct packet *p;
  bool ack;
};

int sfd;
int delay;
struct mailbox *acker_inbox;
struct mailbox *sr_inbox;
struct mailbox *timer_inbox;
struct mailbox *network_inbox;
int last_seq;
bool continue_acking;

// initialized to 0 (i.e. NULL)
struct network_message *first;
struct network_message *last;

void send_scheduled_sending () {
  // avoid overhead of calling it too many times
  long t = get_time_usec();
  /*printf("network let's go !!!\n");
  if (first != NULL) {
    printf("network let's go !!! %ld %ld\n", first->time, t);
  }*/
  while (first != NULL && first->time <= t) {
    //printf("network let's goo !!!\n");
    if (first->ack) {
    //printf("network let's gooa !!!\n");
      struct message *m = (struct message*) malloc(sizeof(struct message));
      m->type = ACK_MESSAGE_TYPE;
      m->data = first->p;
      printf("network send    %d to sr\n", m->type);
      send_mail(sr_inbox, m);
    } else {
      //printf("network let's gooo !!!\n");
      // TODO do an agent that send because here we make the ack
      // waits while we are sending..
      if (write(sfd, first->p, PACKET_SIZE) != PACKET_SIZE) {
        fprintf(stderr, "partial/failed write\n");
        exit(EXIT_FAILURE);
      }
    }
    first = first->next;
    if (first == NULL) {
      last = NULL;
    }
  }
}

void schedule_sending (struct simulator_message *sm, bool ack) {
  send_scheduled_sending();
  // to avoid having the problem with 2 schedule with the same id

  struct network_message *nm = (struct network_message *) malloc(sizeof(struct network_message));
  // We only start time here (and not when SelectiveRepeat send it)
  // to simulate a bit of randomness (the time between SR et this network agent threating it)
  // --> to put in report
  // that way we are also sure that they are put in order in the list
  
  nm->time = get_time_usec() + delay * MILLION;
  //printf("network wait -> %ld\n", nm->time);
  nm->p = sm->p;
  nm->ack = ack;
  nm->next = NULL;
  if (last == NULL) {
    assert(first == NULL);
    first = nm;
  } else {
    assert(first != NULL);
    last->next = nm;
  }
  last = nm;
  struct message *m = (struct message *) malloc(sizeof(struct message));
  m->type = ALARM_MESSAGE_TYPE;
  struct alarm *alrm = (struct alarm *) malloc(sizeof(struct alarm));
  // we multiply by 3 distinguish ids for timeout, acks and normal packets
  // if we previously used this id for the timer, this timer has necessarily already expired
  alrm->id = sm->id * 3 + ack;
  alrm->timeout = nm->time;
  alrm->inbox = network_inbox;
  m->data = alrm;
  printf("network send    %d to timer\n", m->type);
  send_mail(timer_inbox, m);
}


bool network (struct message *m) {
  printf("network receives %d\n", m->type);
  if (m->type == INIT_MESSAGE_TYPE) {
    struct network_init *init = (struct network_init *) m->data;
    sfd = init->sfd;
    delay = init->delay;
    acker_inbox = init->acker_inbox;
    sr_inbox = init->sr_inbox;
    timer_inbox = init->timer_inbox;
    network_inbox = init->network_inbox;
    last_seq = -1;
    continue_acking = true;
  } else if (m->type == SEND_MESSAGE_TYPE) {
    struct simulator_message *sm = (struct simulator_message *) m->data;
    if (sm->last) {
      last_seq = sm->p->seq;
      printf("last_ack : %d\n", last_seq);
    }
    schedule_sending(sm, false);
  } else if (m->type == ACK_MESSAGE_TYPE) {
    struct simulator_message *sm = (struct simulator_message *) m->data;
    schedule_sending(sm, true);
    printf("last_ack : %d cur_ack : %d\n", last_seq, sm->p->seq);
    if (valid_ack(sm->p) && last_seq != -1 && sm->p->seq == last_seq) {
      continue_acking = false;
    }
  } else if (m->type == CONTINUE_ACKING_MESSAGE_TYPE) {
    struct message *cont = (struct message *) malloc(sizeof(struct message));
    if (continue_acking) {
      cont->type = CONTINUE_ACKING_MESSAGE_TYPE;
    } else {
      cont->type = STOP_MESSAGE_TYPE;
    }
    cont->data = NULL;
    printf("network sends    %d to acker\n", m->type);
    send_mail(acker_inbox, cont);
  } else {
    assert(m->type == TIMEOUT_MESSAGE_TYPE);
    send_scheduled_sending();
  }
  return true;
}