#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include"multmodulo.h"
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/limits.h>

struct Server {
  char ip[255]; // 255 - максимальная длина DNS-имени хоста
  int port;
};

struct Job {
  struct Server server;
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result;
  bool ok;
};


bool ConvertStringToUI64(const char *str, uint64_t *val) {
  char *end = NULL;
  errno = 0;
  unsigned long long i = strtoull(str, &end, 10);
  if (errno == ERANGE) {
    fprintf(stderr, "Out of uint64_t range: %s\n", str);
    return false;
  }

  if (errno != 0 || end == str || *end != '\0') {
    fprintf(stderr, "Not a number: %s\n", str);
    return false;
  }

  *val = i;
  return true;
}

static bool RecvAll(int fd, char *buf, size_t len) {
  size_t got = 0;
  while (got < len) {
    ssize_t r = recv(fd, buf + got, len - got, 0);
    if (r <= 0)
      return false;
    got += (size_t)r;
  }
  return true;
}

// Читает файл со строками "ip:port". Возвращает массив серверов,
// в *count - их количество. При ошибке возвращает NULL.
static struct Server *ReadServers(const char *path, unsigned int *count) {
  FILE *f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "Can not open servers file: %s\n", path);
    return NULL;
  }

  struct Server *list = NULL;
  unsigned int n = 0;
  char *line = NULL;
  size_t cap = 0;

  while (getline(&line, &cap, f) != -1) {
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0')
      continue;

    char *colon = strrchr(line, ':');
    if (!colon || colon == line) {
      fprintf(stderr, "Bad server line (need ip:port): %s\n", line);
      free(line);
      free(list);
      fclose(f);
      return NULL;
    }
    *colon = '\0';
    int port = atoi(colon + 1);
    if (port <= 0 || port > 65535 || strlen(line) >= sizeof(list[0].ip)) {
      fprintf(stderr, "Bad server entry: %s:%s\n", line, colon + 1);
      free(line);
      free(list);
      fclose(f);
      return NULL;
    }

    struct Server *tmp = realloc(list, sizeof(struct Server) * (n + 1));
    if (!tmp) {
      fprintf(stderr, "Out of memory\n");
      free(line);
      free(list);
      fclose(f);
      return NULL;
    }
    list = tmp;
    strcpy(list[n].ip, line);
    list[n].port = port;
    n++;
  }

  free(line);
  fclose(f);
  *count = n;
  return list;
}

// Поток: соединяется с одним сервером, отправляет кусок, получает результат.
static void *RunJob(void *arg) {
  struct Job *job = (struct Job *)arg;
  job->ok = false;

  struct hostent *hostname = gethostbyname(job->server.ip);
  if (hostname == NULL) {
    fprintf(stderr, "gethostbyname failed with %s\n", job->server.ip);
    return NULL;
  }

  struct sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons((uint16_t)job->server.port);
  memcpy(&server.sin_addr.s_addr, hostname->h_addr_list[0],
         sizeof(server.sin_addr.s_addr));

  int sck = socket(AF_INET, SOCK_STREAM, 0);
  if (sck < 0) {
    fprintf(stderr, "Socket creation failed!\n");
    return NULL;
  }

  if (connect(sck, (struct sockaddr *)&server, sizeof(server)) < 0) {
    fprintf(stderr, "Connection to %s:%d failed\n", job->server.ip,
            job->server.port);
    close(sck);
    return NULL;
  }

  char task[sizeof(uint64_t) * 3];
  memcpy(task, &job->begin, sizeof(uint64_t));
  memcpy(task + sizeof(uint64_t), &job->end, sizeof(uint64_t));
  memcpy(task + 2 * sizeof(uint64_t), &job->mod, sizeof(uint64_t));

  if (send(sck, task, sizeof(task), 0) < 0) {
    fprintf(stderr, "Send to %s:%d failed\n", job->server.ip, job->server.port);
    close(sck);
    return NULL;
  }

  char response[sizeof(uint64_t)];
  if (!RecvAll(sck, response, sizeof(response))) {
    fprintf(stderr, "Receive from %s:%d failed\n", job->server.ip,
            job->server.port);
    close(sck);
    return NULL;
  }

  memcpy(&job->result, response, sizeof(uint64_t));
  job->ok = true;
  close(sck);
  return NULL;
}

int main(int argc, char **argv) {
  uint64_t k = -1;
  uint64_t mod = -1;
  char servers[255] = {'\0'}; // путь до файла
  bool k_set = false, mod_set = false;

  while (true) {
    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        k_set = ConvertStringToUI64(optarg, &k);
        if (!k_set)
          return 1;
        break;
      case 1:
        mod_set = ConvertStringToUI64(optarg, &mod);
        if (!mod_set)
          return 1;
        break;
      case 2:
        if (strlen(optarg) >= sizeof(servers)) {
          fprintf(stderr, "Servers path is too long\n");
          return 1;
        }
        strcpy(servers, optarg);
        break;
      default:
        printf("Index %d is out of options\n", option_index);
      }
    } break;

    case '?':
      printf("Arguments error\n");
      break;
    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  if (!k_set || !mod_set || !strlen(servers)) {
    fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n",
            argv[0]);
    return 1;
  }

  if (mod == 0) {
    fprintf(stderr, "mod must be positive\n");
    return 1;
  }

  unsigned int servers_num = 0;
  struct Server *to = ReadServers(servers, &servers_num);
  if (to == NULL)
    return 1;
  if (servers_num == 0) {
    fprintf(stderr, "Servers file is empty\n");
    free(to);
    return 1;
  }

  // k! = 1 * 2 * ... * k. Если k = 0, то 0! = 1.
  // Серверов не больше, чем чисел для перемножения.
  if (k == 0) {
    printf("answer: %llu\n", (unsigned long long)(1 % mod));
    free(to);
    return 0;
  }
  if (servers_num > k)
    servers_num = (unsigned int)k;

  struct Job *jobs = calloc(servers_num, sizeof(struct Job));
  pthread_t *threads = malloc(sizeof(pthread_t) * servers_num);
  if (!jobs || !threads) {
    fprintf(stderr, "Out of memory\n");
    return 1;
  }

  // Делим [1, k] между серверами на примерно равные куски.
  uint64_t part = k / servers_num;
  uint64_t rest = k % servers_num;
  uint64_t cur = 1;
  for (unsigned int i = 0; i < servers_num; i++) {
    uint64_t size = part + (i < rest ? 1 : 0);
    jobs[i].server = to[i];
    jobs[i].begin = cur;
    jobs[i].end = cur + size - 1;
    jobs[i].mod = mod;
    cur += size;
  }

  // Параллельная работа с серверами: по потоку на сервер.
  for (unsigned int i = 0; i < servers_num; i++) {
    if (pthread_create(&threads[i], NULL, RunJob, &jobs[i])) {
      fprintf(stderr, "pthread_create failed\n");
      return 1;
    }
  }

  // Объединяем результаты.
  uint64_t answer = 1 % mod;
  bool all_ok = true;
  for (unsigned int i = 0; i < servers_num; i++) {
    pthread_join(threads[i], NULL);
    if (!jobs[i].ok) {
      all_ok = false;
      continue;
    }
    answer = MultModulo(answer, jobs[i].result, mod);
  }

  free(threads);
  free(jobs);
  free(to);

  if (!all_ok) {
    fprintf(stderr, "Some servers failed, answer is unavailable\n");
    return 1;
  }

  printf("answer: %llu\n", (unsigned long long)answer);
  return 0;
}