#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Maximum size of the message sent by user
#define MSG_MAXSIZE 1024

#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                               \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

int recv_all(int sockfd, void *buff, size_t len);
int send_all(int sockfd, void *buff, size_t len);

#endif