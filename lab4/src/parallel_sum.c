#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <getopt.h>
#include <time.h>
#include <limits.h>
#include "sum.h"
#include "utils.h"


void *ThreadSum(void *args) {
  struct SumArgs *sum_args = (struct SumArgs *)args;
  return (void *)(size_t)Sum(sum_args);
}


int main(int argc, char **argv) {
  
  uint32_t threads_num = 0;
  uint32_t array_size = 0;
  uint32_t seed = 0;

  while (1) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {
        {"seed", required_argument, 0, 0},
        {"array_size", required_argument, 0, 0},
        {"threads_num", required_argument, 0, 0},
        {0, 0, 0}
    };
    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            if (seed <= 0) {
              printf("seed must be a positive number\n");
              return 1;
            }
            break;
          case 1:
            array_size = atoi(optarg);
            if (array_size <= 0) {
              printf("array_size must be a positive number\n");
              return 1;
            }
            break;
          case 2:
            threads_num = atoi(optarg);
            if (threads_num <= 0) {
              printf("threads_num must be a positive number\n");
              return 1;
            }
            break;
          default:
            printf("Index %d is out of options\n", option_index);
            return 1;
        }
        break;

      case '?':
        break;
      default:
        printf("getopt returned character code 0%o?\n", c);
        return 1;
    }
  }

  if (threads_num == 0 || array_size == 0 || seed == 0) {
        printf("Usage: %s --seed num --array_size num --threads_num num\n", argv[0]);
        return 1;
    }


  int *array = malloc(sizeof(int) * array_size);
  pthread_t threads[threads_num];
  GenerateArray(array, array_size, seed);

    // for (int i = 0; i < array_size; i++) {
    //   printf("%d ", array[i]);
    // }
    // printf("\n");

  struct timeval start_time;
  gettimeofday(&start_time, NULL);
  struct SumArgs args[threads_num];

  for (int i = 0; i < threads_num; i++) {
    int chunk = array_size / threads_num;
    int remainder = array_size % threads_num;
    int start = i * chunk + (i < remainder ? i : remainder);
    int lenght = chunk + (i < remainder ? 1 : 0);
    args[i].array = array;
    args[i].begin = start;
    args[i].end = start + lenght;
  }

  for (uint32_t i = 0; i < threads_num; i++) {
    if (pthread_create(&threads[i], NULL, ThreadSum, (void *)&args[i])) {
      printf("Error: pthread_create failed!\n");
      return 1;
    }
  }

  long total_sum = 0;
  for (uint32_t i = 0; i < threads_num; i++) {
    long sum = 0;
    pthread_join(threads[i], (void **)&sum);
    total_sum += sum;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);
  printf("Total: %ld\n", total_sum);
  printf("Elapsed time: %fms\n", elapsed_time);
  return 0;
}