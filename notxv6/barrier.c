#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

static int nthread = 1;
static int round = 0;

struct barrier {
  pthread_mutex_t barrier_mutex;
  pthread_cond_t barrier_cond;
  int nthread;      // Number of threads that have reached this round of the barrier
  int round;     // Barrier round
} bstate;

static void
barrier_init(void)
{
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
  bstate.nthread = 0;
}

static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  pthread_mutex_lock(&bstate.barrier_mutex); 
  bstate.nthread++; // step1: reach barrier, count++
  int curRound = bstate.round;
  while(1){ // in case of spurious wake up
    if(bstate.nthread == nthread){ // step2: whether all thread reach?
      bstate.round++; // step3: all reach, round++
      bstate.nthread = 0; // re count n thread
      break;
    } else if (curRound < bstate.round) { // step4: other round has begin
      break;
    } else {
      pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex); // wait other thread reach
    }
  }

  pthread_mutex_unlock(&bstate.barrier_mutex); 
  pthread_cond_broadcast(&bstate.barrier_cond); // this thread pass, wake up other thread to go though next round
}

static void *
thread(void *xa)
{
  long n = (long) xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {
    int t = bstate.round;
    assert (i == t);// 如果没有全部到达，round就不能自增，此时另一个线程到达的时候i才能与t一致。如果t增加了，那么进来的时候就会出现不等。
    barrier();
    usleep(random() % 100);
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);

  barrier_init();

  for(i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, thread, (void *) i) == 0);
  }
  for(i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  printf("OK; passed\n");
}
