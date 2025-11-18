#include "common.h"
#include "messages.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>

int recv_all(int sockfd, void *buffer, size_t len)
{
	size_t bytes_received = 0;
	size_t bytes_remaining = len;
	char *buf = buffer;
	while (bytes_remaining) {
		int bytes = recv(sockfd, buf + bytes_received, len - bytes_received, 0);
		DIE(bytes < 0, "recv error");
		if (bytes == 0) {
			break;
		}

		bytes_received += bytes;
		bytes_remaining -= bytes;
	}
	return bytes_received;
}

int send_all(int sockfd, void *buffer, size_t len)
{
	size_t bytes_sent = 0;
	size_t bytes_remaining = len;
	char *buf = buffer;
	while (bytes_remaining) {
		int bytes = send(sockfd, buf + bytes_sent, len - bytes_sent, 0);
		DIE(bytes < 0, "send error");
		if (bytes == 0) {
			break;
		}

		bytes_sent += bytes;
		bytes_remaining -= bytes;
	}
	return bytes_sent;
}

