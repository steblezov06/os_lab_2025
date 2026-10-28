#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

long long k, mod, pnum;
long long result = 1;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

void *worker(void *arg) {
  int id = *(int *)arg;
  long long partial = 1;

  /* поток id перемножает числа id+1, id+1+pnum, id+1+2*pnum, */
  for (long long i = id + 1; i <= k; i += pnum)
    partial = partial * i % mod;

  pthread_mutex_lock(&mut);
  result = result * partial % mod;
  pthread_mutex_unlock(&mut);
  return NULL;
}

int main(int argc, char **argv) {
  struct option opts[] = {{"k", required_argument, 0, 'k'},
                          {"pnum", required_argument, 0, 'p'},
                          {"mod", required_argument, 0, 'm'},
                          {0, 0, 0, 0}};
  int c;
  while ((c = getopt_long(argc, argv, "k:", opts, NULL)) != -1) {
    if (c == 'k') k = atoll(optarg);
    if (c == 'p') pnum = atoll(optarg);
    if (c == 'm') mod = atoll(optarg);
  }
  if (k < 0 || pnum < 1 || mod < 1) {
    printf("Usage: %s -k 10 --pnum=4 --mod=10\n", argv[0]);
    return 1;
  }

  result = 1 % mod;
  pthread_t threads[pnum];
  int ids[pnum];

  for (int i = 0; i < pnum; i++) {
    ids[i] = i;
    pthread_create(&threads[i], NULL, worker, &ids[i]);
  }
  for (int i = 0; i < pnum; i++)
    pthread_join(threads[i], NULL);

  printf("%lld! mod %lld = %lld\n", k, mod, result);
  return 0;
}