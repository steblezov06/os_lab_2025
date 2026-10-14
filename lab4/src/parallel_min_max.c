#include <ctype.h>
#include <limits.h>
#include <signal.h>  // [ДОБАВЛЕНО] нужен для signal, SIGALRM, SIGKILL, kill
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


static volatile sig_atomic_t timeout_expired = 0;


static void alarm_handler(int signo) {
  (void)signo;
  timeout_expired = 1;
}

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  int timeout = -1; 
  bool with_files = false;

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"timeout", required_argument, 0, 0},
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
            timeout = atoi(optarg);
            
            if (timeout <= 0) {
              printf("timeout must be a positive number\n");
              return 1;
            }
            break;
          case 4:
            with_files = true;
            break;

          
          default:
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
   
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" "
           "[--timeout \"sec\"] [--by_files | -f]\n",
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

  
  pid_t *child_pids = malloc(sizeof(pid_t) * pnum);
  bool *alive = calloc(pnum, sizeof(bool));
  bool *killed = calloc(pnum, sizeof(bool));

  if (!with_files) {
    pipes = malloc(sizeof(int[2]) * pnum);
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipes[i]) == -1) {
        perror("pipe");
        return 1;
      }
    }
  } else {
    filenames = malloc(sizeof(char[64]) * pnum);
    for (int i = 0; i < pnum; i++) {
      snprintf(filenames[i], 64, "/tmp/parallel_min_max_%d_%d.txt", getpid(),
               i);
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
          if (fp == NULL) {
            perror("fopen");
            return 1;
          }
          fprintf(fp, "%d %d", local.min, local.max);
          fclose(fp);
        } else {
          close(pipes[i][0]);
          if (write(pipes[i][1], &local, sizeof(struct MinMax)) !=
              sizeof(struct MinMax)) {
            perror("write");
            return 1;
          }
          close(pipes[i][1]);
        }
        return 0;
      }
      
      child_pids[i] = child_pid;
      alive[i] = true;
    } else {
      printf("Fork failed!\n");
      return 1;
    }
  }


  
  if (!with_files) {
    for (int i = 0; i < pnum; i++) close(pipes[i][1]);
  }

  //Таймер запускается только если --timeout задан.
  
  if (timeout > 0) {
    signal(SIGALRM, alarm_handler);
    alarm(timeout);
  }


  // Теперь: неблокирующий waitpid(WNOHANG) + проверка флага таймаута.
  bool kill_sent = false;
  while (active_child_processes > 0) {
    // Таймаут истёк: посылаем SIGKILL всем ещё живым детям
    if (timeout_expired && !kill_sent) {
      for (int i = 0; i < pnum; i++) {
        if (alive[i]) kill(child_pids[i], SIGKILL);
      }
      kill_sent = true;
      printf("Timeout (%d s) expired, children killed with SIGKILL\n",
             timeout);
    }

    int status;
    pid_t finished = waitpid(-1, &status, WNOHANG);
    if (finished > 0) {
      // определяем, какой ребёнок завершился и убит ли он
      for (int i = 0; i < pnum; i++) {
        if (child_pids[i] == finished) {
          alive[i] = false;
          if (WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL)
            killed[i] = true;
          break;
        }
      }
      active_child_processes -= 1;
    } else if (finished == 0) {
      // никто ещё не завершился — немного ждём
      usleep(1000);
    } else {
      break;
    }
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;
  int results = 0;  // [ДОБАВЛЕНО] сколько детей реально вернули результат

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    // Убитый ребёнок результата не дал: чистим ресурсы, пропускаем
    if (killed[i]) {
      if (with_files) unlink(filenames[i]);
      else close(pipes[i][0]);
      continue;
    }

    if (with_files) {
      FILE *fp = fopen(filenames[i], "r");
      if (fp == NULL) {
        perror("fopen");
        return 1;
      }
      if (fscanf(fp, "%d %d", &min, &max) != 2) {
        fprintf(stderr, "Failed to read from %s\n", filenames[i]);
        fclose(fp);
        return 1;
      }
      fclose(fp);
      unlink(filenames[i]);
    } else {
      struct MinMax local;
      if (read(pipes[i][0], &local, sizeof(struct MinMax)) !=
          sizeof(struct MinMax)) {
        perror("read");
        return 1;
      }
      close(pipes[i][0]);
      min = local.min;
      max = local.max;
    }

    results++;
    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);
  free(child_pids);
  free(alive);
  free(killed);
  if (pipes) free(pipes);
  if (filenames) free(filenames);

  
  
  if (results == 0) {
    printf("No results: all children were killed by timeout\n");
  } else {
    if (results < pnum)
      printf("Warning: result is partial (%d of %d children finished)\n",
             results, pnum);
    printf("Min: %d\n", min_max.min);
    printf("Max: %d\n", min_max.max);
  }
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}