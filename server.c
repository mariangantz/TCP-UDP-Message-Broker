#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "common.h"
#include "messages.h"

#define MAX_CONNECTIONS 100

void run_chat_multi_server(int listenfd, int udp_listenfd, struct topic_entry *topics, int *nr_topics)
{   
    int rc;
    int num_sockets = 3;
    struct pollfd *poll_fds = calloc(num_sockets, sizeof(struct pollfd));
    int poll_max_capacity = num_sockets;
    int nr_clients = 0;
    
    // Set socket TCP soket on listen
    rc = listen(listenfd, MAX_CONNECTIONS);
    if (rc < 0) {
        perror("listen failed");
        close(listenfd);
        close(udp_listenfd);
        exit(EXIT_FAILURE);
}

    poll_fds[0] = (struct pollfd){
        .fd = listenfd,
        .events = POLLIN,
        .revents = 0 
};

    poll_fds[1] = (struct pollfd){
        .fd = udp_listenfd,
        .events = POLLIN,
        .revents = 0
};

    poll_fds[2] = (struct pollfd){
        .fd = STDIN_FILENO, 
        .events = POLLIN,
        .revents = 0
};

struct client_info *clients_info = calloc(1, sizeof(struct client_info));
if (!clients_info) {
    close(listenfd);
    close(udp_listenfd);
    exit(EXIT_FAILURE);
}
    

    while (1) {
        // Wait to receive something on one of the sockets
        int i = 0 , j = 0;
        rc = poll(poll_fds, num_sockets, -1);
        DIE(rc < 0, "poll");

         while(i < num_sockets) {
            if (poll_fds[i].revents & POLLIN) {

                /*
                Received a connection request from the TCP listen socket 
                */
               if (listenfd == poll_fds[i].fd) {
                struct sockaddr_in cli_addr = {0};  // Inițializare explicită
                socklen_t cli_len = sizeof(cli_addr);
                
                // Acceptă conexiunea cu verificare de erori
                const int newsockfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
                if (newsockfd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // Nu e eroare, doar nu sunt conexiuni disponibile momentan
                        continue;
                    }
                    perror("Eroare la acceptare conexiune");
                    continue;  // Continuă în loc să închei programul
                }
            
                // Obține portul clientului
                int client_port = ntohs(cli_addr.sin_port);
                    char *client_ip = inet_ntoa(cli_addr.sin_addr);

                    if(num_sockets == poll_max_capacity) {
                        poll_max_capacity *= 2;
                        struct pollfd *temp = realloc(poll_fds, poll_max_capacity * sizeof(struct pollfd));
                        DIE(temp == NULL, "realloc poll_fds");
                        poll_fds = temp;
                    }

                    poll_fds[num_sockets].events = POLLIN;
                    poll_fds[num_sockets].fd = newsockfd;
                    num_sockets = num_sockets + 1;

                    nr_clients = nr_clients + 1;
                    struct client_info *new_clients = realloc(clients_info, (nr_clients + 1) * sizeof(struct client_info));
                            if (!new_clients) {
                                close(newsockfd);
                                continue;
                            }
                    clients_info = new_clients;

                    clients_info[nr_clients + j - 1].IP = client_ip;
                    clients_info[nr_clients - VAR].socketFD = newsockfd;
                    clients_info[nr_clients + j - VAR].port = client_port;
                    clients_info[nr_clients + j - 1].ID = NULL;

                /*
                Received data on the socket connected to stdin
                */
                } else if (fileno(stdin) == poll_fds[i].fd) {
                    char buf[MSG_MAXSIZE + 1] = {0}; 
                    if (fgets(buf, sizeof(buf), stdin) == NULL) {
                        break;
                    }
                
                    size_t len = strlen(buf);
                    if (len > 0 && buf[len - 1] == '\n') {
                        buf[len - 1] = '\0';
                    }

                    for (size_t j = 0; j < strlen(buf); j++) {
                        if (!isspace((unsigned char)buf[j])) {
                             break;
                        }
                    }

                    if(strcmp(buf, "exit") != 0) {
                        fprintf(stderr, "Bad message!\n");
                    } else {
                        int j = 1; 
                        while(j < num_sockets) {
                            close(poll_fds[i].fd);
                            j++;
                        }
                    free(poll_fds);
                    delete_clients(&clients_info, nr_clients);
                    }
                }

                /*
                    Received a packet from the UDP client
                */
                else if (udp_listenfd == poll_fds[i].fd) {
                    struct udp_packet packet;
                    struct sockaddr_in client_addr;
                    socklen_t clen = sizeof(client_addr);

                    memset(&packet, 0, sizeof(packet));
                    memset(&client_addr, 0, sizeof(client_addr));

                    int rc = recvfrom(udp_listenfd, &packet, sizeof(packet), 0,(struct sockaddr *)&client_addr, &clen);
                        DIE(rc < 0, "recvfrom");

                if (packet.type < 0 || packet.type > 3) {
                    fprintf(stderr, "Corrupted packet! Invalid type: %d\n", packet.type);
                    }
                    int k = 0;
                    int client_port = ntohs(client_addr.sin_port);
                    char *client_ip = inet_ntoa(client_addr.sin_addr);

                    while(k < nr_clients) {
                        clients_info[k].got_packet = 0;
                        k++;
                    }
                    int m = 0;
                    while(m < *nr_topics) {
                        if(cmp_topics(packet.topic, topics[m].topic) == 0) {
                            int fd;
                            int nr_subs = topics[m].nr_subscribers;
                            int j = 0;
                            char** subs = topics[m].subscribed_clients;

                            while(j < nr_subs) {
                                int p = 0;
                                while(p < nr_clients) {
                                    if (clients_info[p].ID && strcmp(subs[j], clients_info[p].ID) == 0) {
                                        if (clients_info[p].got_packet == 0) {
                                            fd = clients_info[p].socketFD;
                                            if (send_info(fd, client_ip, client_port, packet) < 0) {
                                                perror("Eroare la trimiterea pachetului");
                                                continue;
                                            }
                                            clients_info[p].got_packet = 1;
                                        }
                                        break;
                                    }
                                p = p + 1;
                                }
                            j = j + 1;
                            }
                        }
                    m = m + 1;    
                    }
                }


                else {
                    int packet_len = 0;
                    int rc = recv_all(poll_fds[i].fd, &packet_len, sizeof(int));
                    DIE(rc < 0, "recv packet_len");

                if (packet_len <= 0 || packet_len > MSG_MAXSIZE) {
                    fprintf(stderr, "Invalid packet length: %d\n", packet_len);
                    close(poll_fds[i].fd);
                    continue;
                }

                char *buf = malloc(packet_len + 1);
                DIE(buf == NULL, "malloc");
                rc = recv_all(poll_fds[i].fd, buf, packet_len);
                DIE(rc < 0, "recv payload");
                buf[packet_len] = '\0';

                    if (rc == 0 || strcmp(buf, "exit") == 0) {
                        int k = 0;
                        while(k < nr_clients) {
                            if(clients_info[k].socketFD == poll_fds[i].fd) {
                                printf("Client %s disconnected.\n", clients_info[k].ID);
                                int p = k;
                                while(p < nr_clients - 1) {
                                    memcpy(&clients_info[k], &clients_info[k + 1], sizeof(struct client_info));
                                    p = p + 1;
                                }
                                nr_clients--;
                                clients_info = realloc(clients_info, nr_clients * sizeof(struct client_info));
                                if(nr_clients != 0) {
                                    DIE(clients_info == NULL, "realloc");
                                }
                                break;
                            }
                        k = k + 1;
                        }
                        close(poll_fds[i].fd);

                        int j = i;
                        while (j < num_sockets - 1)
                        {
                            poll_fds[j] = poll_fds[j + 1];
                            j = j + 1;
                        }
                        num_sockets = num_sockets - 1;
                    }
                    else
                    {   
                        int hasID = 1;
                        int client_index = -1;
                        int k = 0;
                        while(k < nr_clients) {
                            if(poll_fds[i].fd == clients_info[k].socketFD) {
                                client_index = k;
                                hasID = (clients_info[k].ID != NULL);
                                break;
                            
                            }
                        k = k + 1;
                        }

                        if (client_index == -1) {
                            fprintf(stderr, "Data from unregistered FD %d\n", poll_fds[i].fd);
                            close(poll_fds[i].fd);
                            poll_fds[i].fd = -1;
                        }

                        if(hasID != 1) {
                            char* new_ID = strdup(buf);
                            int k = 0;
                            int same_ID = 0;
                            while(k < nr_clients) {
                                if (clients_info[k].ID != NULL && strcmp(clients_info[k].ID, new_ID) == 0) {
                                    same_ID = 1;
                                    printf("Client %s already connected.\n", new_ID);
                                    close(poll_fds[i].fd);
                                    poll_fds[i].fd = -1;
                                    memmove(&clients_info[client_index], 
                                            &clients_info[client_index + 1],
                                            (nr_clients - client_index - 1) * sizeof(struct client_info));
                                    nr_clients--;
                                    memset(&clients_info[nr_clients], 0, sizeof(struct client_info));
                                    break;
                                
                                    clients_info = realloc(clients_info, nr_clients * sizeof(struct client_info));
                                    close(poll_fds[i].fd);
                                    int j = i;
                                    while(j < num_sockets - 1)
                                    {
                                        poll_fds[j] = poll_fds[j + 1];
                                        j = j + 1;
                                    }

                                    num_sockets = num_sockets - 1;
                                    break;
                                }
                            k = k + 1;   
                            }

                            if(client_index != -1 && same_ID == 0) {
                                char *IP = clients_info[client_index].IP;
                                int port = clients_info[client_index].port;
                                clients_info[client_index].ID = new_ID;
                                printf("New client %s connected from %s:%d\n", new_ID, IP, port);
                            }
                          
                        } else {
                            char* topic_name = get_topic(buf);
                            execute_command(topic_name, nr_topics, &topics, clients_info[client_index].ID, buf);
                        }
                    }
                    free(buf);
                }
            }
        i = i + 1;
        }
    }
    free(poll_fds);
    free(clients_info);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    char *endptr; 
    uint16_t port;
    setvbuf(stdout, NULL, _IONBF, BUFSIZ); 
    long parsed_port = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || parsed_port <= 0 || parsed_port > 65535) {
        fprintf(stderr, "Error: Invalid port number. Must be 1-65535\n");
        return EXIT_FAILURE;
    }
    port = (uint16_t)parsed_port;

    // Initialize server address structure
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    // Create TCP socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("TCP socket creation failed");
        return EXIT_FAILURE;
    }

    // Create UDP socket
    int udp_listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_listenfd < 0) {
        perror("UDP socket creation failed");
        close(listenfd);
        return EXIT_FAILURE;
    }

    // Set socket options
    const int enable = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("TCP SO_REUSEADDR failed");
        close(listenfd);
        close(udp_listenfd);
        return EXIT_FAILURE;
    }

    if (setsockopt(udp_listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("UDP SO_REUSEADDR failed");
        close(listenfd);
        close(udp_listenfd);
        return EXIT_FAILURE;
    }

    if (setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0) {
        perror("TCP_NODELAY failed");
        close(listenfd);
        close(udp_listenfd);
        return EXIT_FAILURE;
    }

    // Bind TCP socket
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("TCP bind failed");
        close(listenfd);
        close(udp_listenfd);
        return EXIT_FAILURE;
    }

    // Bind UDP socket
    if (bind(udp_listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("UDP bind failed");
        close(listenfd);
        close(udp_listenfd);
        return EXIT_FAILURE;
    }

    // Initialize topics
    struct topic_entry *topics = NULL;
    int nr_topics = 0;

    // Start server
    run_chat_multi_server(listenfd, udp_listenfd, topics, &nr_topics);

    // Cleanup
    destroy_topics(&topics, nr_topics);
    close(listenfd);
    close(udp_listenfd);

    return EXIT_SUCCESS;
}