#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "messages.h"

void run_client(int sockfd) {
    char buf[MSG_MAXSIZE + 1] = {0};
    struct pollfd poll_fds[2] = {
        {.fd = sockfd, .events = POLLIN, .revents = 0},
        {.fd = STDIN_FILENO, .events = POLLIN, .revents =0}
    };
    const int num_sockets = 2;

    while (1) {
        int rc = poll(poll_fds, num_sockets, -1);
        if (rc < 0) {
            if (errno == EINTR) continue;
            perror("poll failed");
            break;
        }

        if (poll_fds[1].revents & POLLIN) {
            if (!fgets(buf, sizeof(buf), stdin)) {
                if (feof(stdin)) {
                    printf("\nEOF detected. Closing client.\n");
                } else {
                    perror("fgets failed");
                }
                break;
            }

            buf[strcspn(buf, "\n")] = '\0';
            if (buf[0] == '\0') continue;

            if (is_valid(buf) != 0) {
                fprintf(stderr, "Invalid command: %s\n", buf);
                continue;
            }

            if (strncmp(buf, "unsubscribe ", 12) == 0) {
                printf("Unsubscribed from topic %s\n", buf + 12);
            } else if (strncmp(buf, "subscribe ", 10) == 0) {
                printf("Subscribed to topic %s\n", buf + 10);
            }

            if (send_buf(sockfd, buf, strlen(buf) + 1) < 0) {
                perror("send failed");
                break;
            }
        }

        if (poll_fds[0].revents & POLLIN) {
            char udp_ip[INET_ADDRSTRLEN] = {0};
            int udp_port = 0;
            struct udp_packet udp_pack = {0};

            rc = recv_info(sockfd, udp_ip, &udp_pack, &udp_port);
            if (rc <= 0) {
                if (rc == 0) {
                    printf("Server disconnected\n");
                } else {
                    perror("recv_info failed");
                }
                break;
            }

            switch (udp_pack.type) {
                case 0: { //INT
                    int received_number = parse_int(udp_pack.content);
                    printf("%s:%d - %s - INT - %d\n", 
                           udp_ip, udp_port, udp_pack.topic, received_number);
                    break;
                }
                case 1: { //SHORT
                    float received_short_real = parse_short_real(udp_pack.content);
                    printf("%s:%d - %s - SHORT_REAL - %.2f\n", 
                           udp_ip, udp_port, udp_pack.topic, received_short_real);
                    break;
                }
                case 2: {	//FLOAT
                    unsigned int nr_decimals = 0;
    				float received_float = parse_float(udp_pack.content, &nr_decimals);
    				if (nr_decimals > 10) {
        			fprintf(stderr, "Warning: Too many decimal digits (%u), limiting to 10.\n", nr_decimals);
        			nr_decimals = 10;
    }
   	 				printf("%s:%d - %s - FLOAT - %.*f\n", udp_ip, udp_port, udp_pack.topic, nr_decimals, received_float);
    					break;
}
                case 3: { //STRING
                    printf("%s:%d - %s - STRING - %s\n", 
                           udp_ip, udp_port, udp_pack.topic, udp_pack.content);
                    break;
                }
                default:
                    fprintf(stderr, "Unknown packet type: %d\n", udp_pack.type);
            }
        }

        if (poll_fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "Socket error detected\n");
            break;
        }
    }

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <client_id> <server_ip> <server_port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Disable output buffering for stdout
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Parse and validate port
    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);
    DIE(rc != 1, "Invalid port number");

    // Create TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    // Set up server address struct
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_port = htons(port);
    serv_addr.sin_family = AF_INET;
    

    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    if (rc <= 0) {
		perror("inet_pton failed");
		exit(EXIT_FAILURE);
	}

    // Connect to the server
    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
if (rc < 0) {
    perror("connect failed");
    exit(EXIT_FAILURE);
}


    // Send client ID (as null-terminated string)
    rc = send_buf(sockfd, argv[1], strlen(argv[1]) + 1);
    DIE(rc < 0, "send client ID");

    // Run the client logic
    run_client(sockfd);

    // Clean up
    close(sockfd);

    return EXIT_SUCCESS;
}