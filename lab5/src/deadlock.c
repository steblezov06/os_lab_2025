#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER;

void *thread1(void *arg) {
  pthread_mutex_lock(&m1);
  printf("поток 1: захватил m1\n");
  sleep(1); /* даём потоку 2 время захватить m2 */

  printf("поток 1: жду m2...\n");
  pthread_mutex_lock(&m2); /* заблокируется навсегда */

  printf("поток 1: захватил m2\n"); /* не выполнится */
  pthread_mutex_unlock(&m2);
  pthread_mutex_unlock(&m1);
  return NULL;
}

void *thread2(void *arg) {
  pthread_mutex_lock(&m2);
  printf("поток 2: захватил m2\n");
  sleep(1);

  printf("поток 2: жду m1...\n");
  pthread_mutex_lock(&m1); /* заблокируется навсегда */

  printf("поток 2: захватил m1\n"); /* не выполнится */
  pthread_mutex_unlock(&m1);
  pthread_mutex_unlock(&m2);
  return NULL;
}

int main() {
  
  pthread_t t1, t2;
  pthread_create(&t1, NULL, thread1, NULL);
  pthread_create(&t2, NULL, thread2, NULL);

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);

  return 0;
}