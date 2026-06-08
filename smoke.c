#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "uthread.h"
#include "uthread_mutex_cond.h"

#define NUM_ITERATIONS 1000

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__)
#else
#define VERBOSE_PRINT(S, ...) ((void) 0) // do nothing
#endif

struct Agent {
  uthread_mutex_t mutex;
  uthread_cond_t  match;
  uthread_cond_t  paper;
  uthread_cond_t  tobacco;
  uthread_cond_t  smoke;
};

struct Agent* createAgent() {
  struct Agent* agent = malloc (sizeof (struct Agent));
  agent->mutex   = uthread_mutex_create();
  agent->paper   = uthread_cond_create(agent->mutex);
  agent->match   = uthread_cond_create(agent->mutex);
  agent->tobacco = uthread_cond_create(agent->mutex);
  agent->smoke   = uthread_cond_create(agent->mutex);
  return agent;
}

//
// TODO
// You will probably need to add some procedures and struct etc.
//

struct Smoker {
  uthread_mutex_t mutex;
  uthread_cond_t  matchSignal;
  uthread_cond_t  paperSignal;
  uthread_cond_t  tobaccoSignal;
  uthread_cond_t  smoke;
};

struct DaddySmokerAlerter {
  uthread_mutex_t mutex;
  uthread_cond_t  match;
  uthread_cond_t  paper;
  uthread_cond_t  tobacco;
  uthread_cond_t  smoke;
  uthread_cond_t  matchSignal;
  uthread_cond_t  paperSignal;
  uthread_cond_t  tobaccoSignal;
};

int hasPaperBool = 0;
int hasTobaccoBool = 0;
int hasMatchBool = 0;

enum Resource            {    MATCH = 1, PAPER = 2,   TOBACCO = 4};
char* resource_name [] = {"", "match",   "paper", "", "tobacco"};

int signal_count [5];  // # of times resource signalled
int smoke_count  [5];  // # of times smoker with resource smoked

int num_active_threads = 0;
int num_active_threads_alerters = 0;

struct SmokerAlerter {
  uthread_mutex_t mutex;
  uthread_cond_t  match;
  uthread_cond_t  paper;
  uthread_cond_t  tobacco;
  uthread_cond_t  smoke;
  uthread_cond_t  matchSignal;
  uthread_cond_t  paperSignal;
  uthread_cond_t  tobaccoSignal;
};

struct DaddySmokerAlerter* createDaddySmokerAlerter(struct Agent* a) {
  struct DaddySmokerAlerter* daddySmoker = malloc (sizeof (struct DaddySmokerAlerter));
  daddySmoker->mutex = a->mutex;
  daddySmoker->matchSignal = uthread_cond_create(daddySmoker->mutex);
  daddySmoker->paperSignal = uthread_cond_create(daddySmoker->mutex);
  daddySmoker->tobaccoSignal = uthread_cond_create(daddySmoker->mutex);
  daddySmoker->match = a->match;
  daddySmoker->paper = a->paper;
  daddySmoker->tobacco = a->tobacco;
  daddySmoker->smoke = a->smoke;
  return daddySmoker;
}

struct SmokerAlerter* createSmokerAlerter(struct DaddySmokerAlerter* d) {
  struct SmokerAlerter* smokerPush = malloc (sizeof (struct SmokerAlerter));
  smokerPush->mutex   = d->mutex;
  smokerPush->matchSignal = d->matchSignal;
  smokerPush->paperSignal = d->paperSignal;
  smokerPush->tobaccoSignal = d->tobaccoSignal;
  smokerPush->match = d->match;
  smokerPush->paper = d->paper;
  smokerPush->tobacco = d->tobacco;
  smokerPush->smoke = d->smoke;

  return smokerPush;
}

struct Smoker* createSmoker(struct SmokerAlerter* s) {
  struct Smoker* smoker = malloc (sizeof (struct Smoker));
  smoker->mutex = s->mutex;
  smoker->matchSignal = s->matchSignal;
  smoker->paperSignal = s->paperSignal;
  smoker->tobaccoSignal = s->tobaccoSignal;
  smoker->smoke = s->smoke;
  return smoker;
}

void* smokerTobaccoAlerter(void* st) {
  struct SmokerAlerter* smokerAlerter = st;
  uthread_mutex_lock(smokerAlerter->mutex);
  num_active_threads_alerters++;
  uthread_cond_signal(smokerAlerter->smoke);
  while (1) {
    VERBOSE_PRINT ("smokerTobaccoAlerter is waiting.\n");
    uthread_cond_wait(smokerAlerter->tobacco);
    VERBOSE_PRINT ("smokerTobaccoAlerter has awoken\n");
    if (hasPaperBool) {
      VERBOSE_PRINT ("hasPaperBool = 0\n");
      hasPaperBool = 0;
      uthread_cond_signal(smokerAlerter->matchSignal);
    } else if (hasMatchBool) {
      VERBOSE_PRINT ("hasMatchBool = 0\n");
      hasMatchBool = 0;
      uthread_cond_signal(smokerAlerter->paperSignal);
    } else {
      VERBOSE_PRINT ("hasTobaccoBool = 1\n");
      hasTobaccoBool = 1;
    }
  }
  uthread_mutex_unlock(smokerAlerter->mutex);
}

void* smokerTobacco(void* sm) {
  struct Smoker* smoker = sm;
  uthread_mutex_lock(smoker->mutex);
  num_active_threads++;
  while (1) {
    VERBOSE_PRINT ("smokerTobacco is waiting.\n");
    uthread_cond_signal(smoker->smoke);
    uthread_cond_wait(smoker->tobaccoSignal);
    // uthread_cond_signal(smoker->smoke);
    VERBOSE_PRINT ("smokerTobacco smoking\n");
    smoke_count[TOBACCO]++;
  }
  uthread_mutex_unlock(smoker->mutex);
}

void* smokerMatchAlerter(void* st) {
  struct SmokerAlerter* smokerAlerter = st;
  uthread_mutex_lock(smokerAlerter->mutex);
  num_active_threads_alerters++;
  uthread_cond_signal(smokerAlerter->smoke);
  while (1) {
    VERBOSE_PRINT ("smokerMatchAlerter is waiting.\n");
    uthread_cond_wait(smokerAlerter->match);
    VERBOSE_PRINT ("smokerMatchAlerter has awoken\n");
    if (hasTobaccoBool) {
      VERBOSE_PRINT ("hasTobaccoBool = 0\n");
      hasTobaccoBool = 0;
      uthread_cond_signal(smokerAlerter->paperSignal);
    } else if (hasPaperBool) {
      VERBOSE_PRINT ("hasPaperBool = 0\n");
      hasPaperBool = 0;
      uthread_cond_signal(smokerAlerter->tobaccoSignal);
    } else {
      VERBOSE_PRINT ("hasMatchBool = 1\n");
      hasMatchBool = 1;
    }
  }
  uthread_mutex_unlock(smokerAlerter->mutex);
}

void* smokerMatch(void* sm) {
  struct Smoker* smoker = sm;
  uthread_mutex_lock(smoker->mutex);
  num_active_threads++;
  while (1) {
    VERBOSE_PRINT ("smokerMatch is waiting.\n");
    uthread_cond_signal(smoker->smoke);
    uthread_cond_wait(smoker->matchSignal);
    // uthread_cond_signal(smoker->smoke);
    VERBOSE_PRINT ("smokerMatch smoking\n");
    smoke_count[MATCH]++;
  }
  uthread_mutex_unlock(smoker->mutex);
}

void* smokerPaperAlerter(void* st) {
  struct SmokerAlerter* smokerAlerter = st;
  uthread_mutex_lock(smokerAlerter->mutex);
  num_active_threads_alerters++;
  uthread_cond_signal(smokerAlerter->smoke);
  while (1) {
    VERBOSE_PRINT ("smokerPaperAlerter is waiting.\n");
    uthread_cond_wait(smokerAlerter->paper);
    VERBOSE_PRINT ("smokerPaperAlerter has awoken\n");
    if (hasMatchBool) {
      VERBOSE_PRINT ("hasMatchBool = 0\n");
      hasMatchBool = 0;
      uthread_cond_signal(smokerAlerter->tobaccoSignal);
    } else if (hasTobaccoBool) {
      VERBOSE_PRINT ("hasTobaccoBool = 0\n");
      hasTobaccoBool = 0;
      uthread_cond_signal(smokerAlerter->matchSignal);
    } else {
      VERBOSE_PRINT ("hasPaperBool = 1\n");
      hasPaperBool = 1;
    }
  }
  uthread_mutex_unlock(smokerAlerter->mutex);
}

void* smokerPaper(void* sm) {
  struct Smoker* smoker = sm;
  uthread_mutex_lock(smoker->mutex);
  num_active_threads++;
  while (1) {
    VERBOSE_PRINT ("smokerPaper is waiting.\n");
    uthread_cond_signal(smoker->smoke);
    uthread_cond_wait(smoker->paperSignal);
    //uthread_cond_signal(smoker->smoke);
    VERBOSE_PRINT ("smokerPaper smoking\n");
    smoke_count[PAPER]++;
  }
  uthread_mutex_unlock(smoker->mutex);
}


// GLOBAL VARIABLES DECLARED ABOVE
/**
 * You might find these declarations helpful.
 *   Note that Resource enum had values 1, 2 and 4 so you can combine resources;
 *   e.g., having a MATCH and PAPER is the value MATCH | PAPER == 1 | 2 == 3
 */
// enum Resource            {    MATCH = 1, PAPER = 2,   TOBACCO = 4};
// char* resource_name [] = {"", "match",   "paper", "", "tobacco"};

// # of threads waiting for a signal. Used to ensure that the agent
// only signals once all other threads are ready.

// int num_active_threads = 0;
// int num_active_threads_alerters = 0;

// int signal_count [5];  // # of times resource signalled
// int smoke_count  [5];  // # of times smoker with resource smoked

/**
 * This is the agent procedure.  It is complete and you shouldn't change it in
 * any material way.  You can modify it if you like, but be sure that all it does
 * is choose 2 random resources, signal their condition variables, and then wait
 * wait for a smoker to smoke.
 */
void* agent (void* av) {
  struct Agent* a = av;
  static const int choices[]         = {MATCH|PAPER, MATCH|TOBACCO, PAPER|TOBACCO};
  static const int matching_smoker[] = {TOBACCO,     PAPER,         MATCH};

  srandom(time(NULL));
  
  VERBOSE_PRINT ("agent at lock\n");
  uthread_mutex_lock (a->mutex);
  VERBOSE_PRINT ("gotten lock\n");
  // Wait until all other threads are waiting for a signal
  VERBOSE_PRINT ("right before\n");
  while (num_active_threads < 3) {
    VERBOSE_PRINT ("reached guard1\n");
    uthread_cond_wait (a->smoke);
  }
  
  VERBOSE_PRINT ("passed smoker guard\n");

  while (num_active_threads_alerters < 3) {
    VERBOSE_PRINT ("reached guard2\n");
    uthread_cond_wait (a->smoke);
  }
  
  VERBOSE_PRINT ("passed smokerAlerter guard\n");

  for (int i = 0; i < NUM_ITERATIONS; i++) {
    int r = random() % 6;
    switch(r) {
    case 0:
      signal_count[TOBACCO]++;
      VERBOSE_PRINT ("match available\n");
      uthread_cond_signal (a->match);
      VERBOSE_PRINT ("paper available\n");
      uthread_cond_signal (a->paper);
      break;
    case 1:
      signal_count[PAPER]++;
      VERBOSE_PRINT ("match available\n");
      uthread_cond_signal (a->match);
      VERBOSE_PRINT ("tobacco available\n");
      uthread_cond_signal (a->tobacco);
      break;
    case 2:
      signal_count[MATCH]++;
      VERBOSE_PRINT ("paper available\n");
      uthread_cond_signal (a->paper);
      VERBOSE_PRINT ("tobacco available\n");
      uthread_cond_signal (a->tobacco);
      break;
    case 3:
      signal_count[TOBACCO]++;
      VERBOSE_PRINT ("paper available\n");
      uthread_cond_signal (a->paper);
      VERBOSE_PRINT ("match available\n");
      uthread_cond_signal (a->match);
      break;
    case 4:
      signal_count[PAPER]++;
      VERBOSE_PRINT ("tobacco available\n");
      uthread_cond_signal (a->tobacco);
      VERBOSE_PRINT ("match available\n");
      uthread_cond_signal (a->match);
      break;
    case 5:
      signal_count[MATCH]++;
      VERBOSE_PRINT ("tobacco available\n");
      uthread_cond_signal (a->tobacco);
      VERBOSE_PRINT ("paper available\n");
      uthread_cond_signal (a->paper);
      break;
    }
    VERBOSE_PRINT ("agent is waiting for smoker to smoke\n");
    //printf("agent waiting for da smoke");
    uthread_cond_wait (a->smoke);
  }
  
  uthread_mutex_unlock (a->mutex);
  return NULL;
}

int main (int argc, char** argv) {
  
  struct Agent* a = createAgent();
  uthread_t agent_thread;

  uthread_init(5);
  
  // TODO
  // agent_thread = uthread_create(agent, a);

  struct DaddySmokerAlerter* ds = createDaddySmokerAlerter(a);

  struct SmokerAlerter* spT = createSmokerAlerter(ds);
  struct Smoker* sT = createSmoker(spT);
  uthread_t tobacco_smoker_thread;
  uthread_t tobacco_alerter_thread;

  struct SmokerAlerter* spM = createSmokerAlerter(ds);
  struct Smoker* sM = createSmoker(spM);
  uthread_t match_smoker_thread;
  uthread_t match_alerter_thread;

  struct SmokerAlerter* spP = createSmokerAlerter(ds);
  struct Smoker* sP = createSmoker(spP);
  uthread_t paper_smoker_thread;
  uthread_t paper_alerter_thread;

  tobacco_smoker_thread = uthread_create(smokerTobacco, sT);
  match_smoker_thread = uthread_create(smokerMatch, sM);
  paper_smoker_thread = uthread_create(smokerPaper, sP);
  //num_active_threads += 3;
  // VERBOSE_PRINT ("smokers created\n");

  tobacco_alerter_thread = uthread_create(smokerTobaccoAlerter, spT);
  match_alerter_thread = uthread_create(smokerMatchAlerter, spM);
  paper_smoker_thread = uthread_create(smokerPaperAlerter, spP);
  
  agent_thread = uthread_create(agent, a);
  uthread_join(agent_thread, NULL);

  assert (signal_count [MATCH]   == smoke_count [MATCH]);
  assert (signal_count [PAPER]   == smoke_count [PAPER]);
  assert (signal_count [TOBACCO] == smoke_count [TOBACCO]);
  assert (smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS);

  printf ("Smoke counts: %d matches, %d paper, %d tobacco\n",
          smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);

  return 0;
}
