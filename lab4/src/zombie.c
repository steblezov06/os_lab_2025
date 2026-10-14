#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CHILDREN 16

static void mode_zombie(int n) {
  printf("[zombie] Создаю %d детей и НЕ жну их.\n", n);
  printf("[zombie] Родитель заснёт на 60 секунд.\n");
  printf("[zombie] Открой другой терминал и запусти:\n");
  printf("           ps -eo pid,ppid,stat,cmd | grep zombie_demo\n");
  printf("         Состояние 'Z' или 'Z+' — это зомби.\n\n");

  for (int i = 0; i < n; i++) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      return;
    }
    if (pid == 0) {
      _exit(0);
    }
    printf("  порождён ребёнок pid=%d\n", pid);
  }

  sleep(60);
  printf("[zombie] Родитель завершился. Дети-зомби будут усыновлены init и исчезнут.\n");
}


static void mode_reap(int n) {
  printf("[reap] Создаю %d детей и сразу жну их через waitpid().\n", n);

  pid_t pids[MAX_CHILDREN];
  for (int i = 0; i < n; i++) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }
    if (pid == 0) _exit(0);
    pids[i] = pid;
    printf("  порождён ребёнок pid=%d\n", pid);
  }

  for (int i = 0; i < n; i++) {
    int status;
    pid_t done = waitpid(pids[i], &status, 0);
    if (done > 0) {
      printf("  пожнат ребёнок pid=%d, код=%d\n", done, WEXITSTATUS(status));
    }
  }
  printf("[reap] Растения выиграли, зомби нет.\n");
}

static void mode_double(int n) {
  printf("[double] Двойной fork: внуков усыновит init, зомби не будет.\n");

  for (int i = 0; i < n; i++) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }
    if (pid == 0) {
      pid_t grand = fork();
      if (grand < 0) _exit(1);
      if (grand == 0) {
        sleep(2);
        _exit(0);
      }
      _exit(0);
    }
  }

  while (wait(NULL) > 0) {}
  if (errno != ECHILD) perror("wait");

  printf("[double] Все дети пожаты. Внуки живут с init и не станут зомби.\n");
  printf("[double] Сплю 5 секунд — проверь ps.\n");
  sleep(5);
}

int main(int argc, char **argv) {
  int n = 5;
  const char *mode = "zombie";

  if (argc >= 2) {
    if (strcmp(argv[1], "reap") == 0 ||
        strcmp(argv[1], "double") == 0) {
      mode = argv[1];
    } else {
      n = atoi(argv[1]);
      if (n <= 0 || n > MAX_CHILDREN) {
        fprintf(stderr, "usage: %s [N|reap|ignore|double]\n", argv[0]);
        return 1;
      }
    }
  }

  printf("PID родителя: %d\n\n", getpid());

  if (strcmp(mode, "zombie") == 0) mode_zombie(n);
  else if (strcmp(mode, "reap")   == 0) mode_reap(n);
  else if (strcmp(mode, "double") == 0) mode_double(n);
  else mode_zombie(n);

  return 0;
}