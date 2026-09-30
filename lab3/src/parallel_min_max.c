#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

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
            pnum = atoi(optarg);
            if (pnum <= 0) {
              printf("pnum must be a positive number\n");
              return 1;
            }
            break;
          case 3:
            with_files = true;
            break;

          defalut:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case 'f':
        with_files = true;
        break;

      case '?':
        break;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" \n",
           argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  GenerateArray(array, array_size, seed);
  int active_child_processes = 0;

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  int (*pipes)[2] = NULL;
  char (*filenames)[64] = NULL;

  if (!with_files){
    pipes = malloc(sizeof(int[2]) * pnum);
    for (int i = 0; i < pnum; i++){
      if (pipe(pipes[i]) == -1){
        perror("pipe");
        return 1;
      }
    }
  } else {
    filenames = malloc(sizeof(char[64]) * pnum);
    for (int i = 0; i < pnum; i++){
      snprintf(filenames[i], 64, "/tmp/parallel_min_max_%d_%d.txt", getpid(), i);
    }
  }


  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();
    if (child_pid >= 0) {
      // successful fork
      active_child_processes += 1;
      if (child_pid == 0) {
        // child process

        int chunk = array_size / pnum;
        int remainder = array_size % pnum;
        int start = i * chunk + (i < remainder ? i : remainder);
        int lenght = chunk + (i < remainder ? 1 : 0);

        struct MinMax local = GetMinMax(array, start, start + lenght);

        if (with_files) {
          FILE *fp = fopen(filenames[i], "w");
          if (fp == NULL){
            perror("fopen");
            return 1;
          }
          fprintf(fp, "%d %d", local.min, local.max);
          fclose(fp);
        } else {
          close(pipes[i][0]);
          if (write(pipes[i][1], &local, sizeof(struct MinMax)) != sizeof(struct MinMax)){
            perror("write");
            return 1;
          }
          close(pipes[i][1]);
        }
        return 0;
      }
    } else {
      printf("Fork failed!\n");
      return 1;
    }
  }

  while (active_child_processes > 0) {
    pid_t finished = wait(NULL);
    if (finished > 0) active_child_processes -= 1;
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    if (with_files) {
      FILE *fp = fopen(filenames[i], "r");
      if (fp == NULL) {
        perror("fopen");
        return 1;
      }
      if (fscanf(fp, "%d %d", &min, &max) != 2){
        fprintf(stderr, "Failed to read from %s\n", filenames[i]);
        fclose(fp);
        return 1;
      }
      fclose(fp);
      unlink(filenames[i]);
    } else {
      struct MinMax local;
      if (read(pipes[i][0], &local, sizeof(struct MinMax)) != sizeof(struct MinMax)){
        perror("read");
        return 1;
      }
      close(pipes[i][0]);
      min = local.min;
      max = local.max;
    }

    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);
  if (pipes) free(pipes);
  if (filenames) free(filenames);

  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}