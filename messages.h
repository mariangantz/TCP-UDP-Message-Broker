#include <stddef.h>
#include <stdint.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <math.h>

// Maximum size of the message sent by user
#define MSG_MAXSIZE 1024
#define VAR 1

int send_buf(int sockfd, void *buffer, size_t len);
char* get_topic(char message[MSG_MAXSIZE + 1]);

struct client_info {
  int socketFD;
  char* ID;
  char* IP;
  int port;
  int got_packet;
};

struct udp_packet {
  char topic[50];
  char type;
  char content[1500];
};

struct topic_entry {
  int nr_subscribers;
  char topic[50];
  char** subscribed_clients;
};

struct chat_packet {
  uint16_t len;
  char message[MSG_MAXSIZE + 1];
};

void destroy_topics(struct topic_entry **topics_addr, int nr_topics);
int send_info(int sockfd, char* udp_ip, int udp_port, struct udp_packet udp_pack);
int parse_int(char *content);
float parse_short_real(char *content);
int cmp_topics(char packet_topic[50], char existing_topic[50]);
float parse_float(char *content, unsigned int *nr_decimals);
void delete_clients(struct client_info **clients, int nr_clients);
void execute_command(char *topic_name,  int *nr_topics, struct topic_entry **topics, char *clientID, char *command);
int is_valid(char* buf);
int recv_info(int sockfd, char* udp_ip, struct udp_packet *udp_pack, int *udp_port);

