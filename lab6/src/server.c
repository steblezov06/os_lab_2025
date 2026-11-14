#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <getopt.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "multmodulo.h"
#include "pthread.h"

struct FactorialArgs {
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
};



// Произведение чисел begin..end включительно по модулю mod.
uint64_t Factorial(const struct FactorialArgs *args) {
  uint64_t ans = 1 % args->mod;

  for (uint64_t i = args->begin; i <= args->end; i++) {
    ans = MultModulo(ans, i, args->mod);
    if (i == UINT64_MAX) // защита от переполнения счётчика
      break;
  }

  return ans;
}

void *ThreadFactorial(void *args) { //обёртка передаёт структуру в функцию выше
  struct FactorialArgs *fargs = (struct FactorialArgs *)args;
  return (void *)(uintptr_t)Factorial(fargs);
}

// Читает ровно len байт (TCP может отдать данные частями). то же что и в клиенте
static bool RecvAll(int fd, char *buf, size_t len) {
  size_t got = 0;
  while (got < len) {
    ssize_t r = recv(fd, buf + got, len - got, 0);
    if (r == 0)
      return got == 0 ? false : (fprintf(stderr, "Client closed early\n"), false);
    if (r < 0) {
      fprintf(stderr, "Client read failed\n");
      return false;
    }
    got += (size_t)r;
  }
  return true;
}

int main(int argc, char **argv) {
  int tnum = -1;
  int port = -1;

  while (true) {
    static struct option options[] = {{"port", required_argument, 0, 0},
                                      {"tnum", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        port = atoi(optarg);
        if (port <= 0 || port > 65535) {
          fprintf(stderr, "Invalid port: %s\n", optarg);
          return 1;
        }
        break;
      case 1:
        tnum = atoi(optarg);
        if (tnum <= 0) {
          fprintf(stderr, "tnum must be positive\n");
          return 1;
        }
        break;
      default:
        printf("Index %d is out of options\n", option_index);
      }
    } break;

    case '?':
      printf("Unknown argument\n");
      break;
    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  if (port == -1 || tnum == -1) {
    fprintf(stderr, "Using: %s --port 20001 --tnum 4\n", argv[0]);
    return 1;
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0); //создали сокет как и в каждом потоке клиента
  if (server_fd < 0) {
    fprintf(stderr, "Can not create server socket!");
    return 1;
  }

  struct sockaddr_in server; //структура адреса
  server.sin_family = AF_INET; //ipv4
  server.sin_port = htons((uint16_t)port);// порт
  server.sin_addr.s_addr = htonl(INADDR_ANY); //адрес сдесь 0.0.0.0 (слушать все адреса) можно и 127.0.0.1


  int err = bind(server_fd, (struct sockaddr *)&server, sizeof(server)); //привязываем наши адрес и пори из структуры к сокету
  if (err < 0) {
    fprintf(stderr, "Can not bind to socket!");
    return 1;
  }

  err = listen(server_fd, 128); //Переводит сокет в режим ожидания(ждём connect от клиента)
  if (err < 0) {
    fprintf(stderr, "Could not listen on socket\n");
    return 1;
  }

  printf("Server listening at %d\n", port);

  while (true) {  //Цикл обслуживания клиента
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int client_fd = accept(server_fd, (struct sockaddr *)&client, &client_len); 
   //server_fd продолжает слушать и принимать новых клиентов.
   //client_fd — отдельный канал именно с этим клиентом: через него идут recv и send
    if (client_fd < 0) {
      fprintf(stderr, "Could not establish new connection\n");
      continue; //Ошибка одного соединения не должна останавливать весь сервер, поэтому continue, то есть снова ждать.
    }

    while (true) {
      const size_t buffer_size = sizeof(uint64_t) * 3;
      char from_client[sizeof(uint64_t) * 3];

      if (!RecvAll(client_fd, from_client, buffer_size)) //данные от клиента
        break;

      uint64_t begin = 0;
      uint64_t end = 0;
      uint64_t mod = 0;
      memcpy(&begin, from_client, sizeof(uint64_t));
      memcpy(&end, from_client + sizeof(uint64_t), sizeof(uint64_t));
      memcpy(&mod, from_client + 2 * sizeof(uint64_t), sizeof(uint64_t));

      fprintf(stdout, "Receive: %llu %llu %llu\n", (unsigned long long)begin,
              (unsigned long long)end, (unsigned long long)mod);

      if (mod == 0) {
        fprintf(stderr, "Client sent mod = 0\n");
        break;
      }

      pthread_t threads[tnum];
      struct FactorialArgs args[tnum];

      // Делим по потокам наш диапазон
      uint64_t len = (begin <= end) ? (end - begin + 1) : 0;
      uint64_t part = len / (uint64_t)tnum;
      uint64_t rest = len % (uint64_t)tnum;
      uint64_t cur = begin;
      bool failed = false;
      int created = 0;

      for (int i = 0; i < tnum; i++) {
        uint64_t size = part + ((uint64_t)i < rest ? 1 : 0);
        args[i].mod = mod;
        if (size == 0) {
          args[i].begin = 1; // пустой диапазон
          args[i].end = 0;
        } else {
          args[i].begin = cur;
          args[i].end = cur + size - 1;
          cur += size;
        }

        if (pthread_create(&threads[i], NULL, ThreadFactorial,
                           (void *)&args[i])) {
          fprintf(stderr, "Error: pthread_create failed!\n");
          failed = true;
          break;
        }
        created++;
      }

      uint64_t total = 1 % mod;
      for (int i = 0; i < created; i++) {
        void *ret = NULL;
        pthread_join(threads[i], &ret);
        total = MultModulo(total, (uint64_t)(uintptr_t)ret, mod); //перемножаем результаты с потоков
      }

      if (failed)
        break;

      printf("Total: %llu\n", (unsigned long long)total);

      char buffer[sizeof(total)];
      memcpy(buffer, &total, sizeof(total));
      ssize_t sent = send(client_fd, buffer, sizeof(total), 0); //отправляем резутат на клиент (8 байт)
      if (sent < 0) {
        fprintf(stderr, "Can't send data to client\n");
        break;
      }
    }

    shutdown(client_fd, SHUT_RDWR); //управляет соединением. Он говорит TCP, что передача в указанном направлении прекращается(в обе стороны)
    close(client_fd); //освобождает дескриптор, ядро завершает соединение
  }

  return 0;
}