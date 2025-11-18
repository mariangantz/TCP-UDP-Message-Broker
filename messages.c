#include "common.h"
#include "messages.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>


char *get_topic(char message[MSG_MAXSIZE + 1])
{	
	char *topic = NULL;
    char *msg_copy = strdup(message); 
	char *token = strtok(msg_copy, " ");
    DIE(msg_copy == NULL, "strdup");
    
    if (token != NULL) {
        token = strtok(NULL, " ");
    }
    
    if (token != NULL) {
        topic = strdup(token);  
        DIE(topic == NULL, "strdup");
    }
    
    free(msg_copy);
    return topic;
}

void execute_command(char *topic_name,  int *nr_topics, struct topic_entry **topics, char *clientID, char *command)
{
	int i = 0;
	struct topic_entry *topic = NULL;

	// Get a pointer to the topic info we are interested in
	while (i < *nr_topics) {
		if (memcmp((*topics)[i].topic, topic_name, strlen((*topics)[i].topic)) == 0 && strlen(topic_name) == strlen((*topics)[i].topic) ) {
			topic = &((*topics)[i]);
		}
	i = i + 1;
	}

	if (topic == NULL) {
		(*nr_topics)++;
	
		struct topic_entry *new_topics = realloc(*topics, (*nr_topics) * sizeof(struct topic_entry));
		DIE(new_topics == NULL, "realloc");
		*topics = new_topics;
	
		topic = &((*topics)[*nr_topics - 1]);
		memset(topic, 0, sizeof(struct topic_entry));
	
		size_t name_len = strlen(topic_name);
		size_t copy_len;
		if (name_len <= 50) {
			copy_len = name_len;
		} else {
			copy_len = 50;
		}
	
		memcpy(topic->topic, topic_name, copy_len);
		topic->topic[copy_len] = '\0';
	
		topic->nr_subscribers = 0;
	}
	
	int nr_subs = topic->nr_subscribers;
	char **clients = topic->subscribed_clients;
	
	// Remove the client from the subscribed_clients array, if the command is "unsubscribe"
	if (strstr(command, "unsubscribe") == command)
	{int j = 0;
		while (j < nr_subs)
		{
			if (strncmp(clientID, clients[j], 0) == 0)
			{
				free(clients[j]);
				int k = j + 1;
				 while(k < nr_subs)
				{
					clients[k - 1] = clients[k];
					k++;
				}
				
				topic->nr_subscribers--;
				topic->subscribed_clients = realloc(topic->subscribed_clients, topic->nr_subscribers * sizeof(int));
				return;
			}
		j = j + 1;
		}
	}

	// Add the client to the subscribed_clients array, if the command is "subscribe"
	else if (strstr(command, "subscribe") == command)
	{int j = 0;
		 while (j < nr_subs)
		{
			if (strcmp(clients[j], clientID) == 0)
			{
				return;
			}
		j = j + 1;
		}
		nr_subs = nr_subs + 1;
		topic->nr_subscribers++;

		int new_count = topic->nr_subscribers;
		char **new_clients = realloc(topic->subscribed_clients, new_count * sizeof(char *));
		topic->subscribed_clients = new_clients;
		topic->subscribed_clients[new_count - 1] = strdup(clientID);
		topic->nr_subscribers = new_count;

	}
}

// Send buffer of length len with length prefixation
int send_buf(int sockfd, void *buffer, size_t len)
{
    char *new_buff = malloc(sizeof(int) + len);
    if (new_buff == NULL) {
        return -1;
    }

    int *p = (int *)new_buff;
    *p = (int)len;

    memcpy(new_buff + sizeof(int), buffer, len);

    int bytes_sent = send_all(sockfd, new_buff, sizeof(int) + len);
    free(new_buff);

    return bytes_sent;
}

// Send UDP packet message using length prefixation
int send_info(int sockfd, char *udp_ip, int udp_port, struct udp_packet udp_pack) {
    int lengths[5] = {
        strlen(udp_ip) + 1,           
        sizeof(int),                 
        strlen(udp_pack.topic) + 1,   
        sizeof(int),                
        0                          
    };

    switch (udp_pack.type) {
        case 0: lengths[4] = 5; break;
        case 1: lengths[4] = 2; break;
        case 2: lengths[4] = 6; break;
        default:
            lengths[4] = (strlen(udp_pack.content) != 1500) ? 
                        (strlen(udp_pack.content) + 1) : 1500;
            break;
    }

    int header_size = 5 * sizeof(int);
    int payload_size = 0;
    for (int i = 0; i < 5; i++) {
        payload_size += lengths[i];
    }
    int total_size = header_size + payload_size;

    char *buffer = malloc(total_size);
    if (!buffer) {
        return -1;
    }

    memcpy(buffer, lengths, header_size);

    char *payload_ptr = buffer + header_size;
    size_t remaining = payload_size;
    int field_index = 0;

    void *sources[] = {udp_ip, &udp_port, udp_pack.topic, &udp_pack.type, udp_pack.content};
    
    while (field_index < 5) {
        if (lengths[field_index] > remaining) {
            free(buffer);
            return -1;
        }
        
        memcpy(payload_ptr, sources[field_index], lengths[field_index]);
        payload_ptr += lengths[field_index];
        remaining -= lengths[field_index];
        field_index++;
    }

    if (remaining != 0) {
        free(buffer);
        return -1;
    }

    int rc = send_all(sockfd, buffer, total_size);
    free(buffer);
    return rc;
}

// Function for receving UDP packet, used by TCP clients
int recv_info(int sockfd, char *udp_ip,struct udp_packet *udp_pack, int *udp_port) {
    if (!udp_ip || !udp_port || !udp_pack) {
        return -1;
    }

    int lengths[5];
    int rc = recv_all(sockfd, lengths, sizeof(lengths));
    if (rc != sizeof(lengths)) {
        return (rc < 0) ? rc : -1;
    }

    size_t total_length = 0;
    for (int i = 0; i < 5; i++) {
        if (lengths[i] <= 0) {
            return -1; 
        }
        total_length += lengths[i];
    }

    if (total_length > (10 * 1024 * 1024)) { 
        return -1;
    }

    char *buf = malloc(total_length);
    if (!buf) {
        return -1;
    }

    rc = recv_all(sockfd, buf, total_length);
    if (rc != total_length) {
        free(buf);
        return (rc < 0) ? rc : -1;
    }

    char *p = buf;
    size_t remaining = total_length;
    int field_index = 0;

    void *destinations[] = {
        udp_ip,
        udp_port,
        udp_pack->topic,
        &udp_pack->type,
        udp_pack->content
    };

    while (field_index < 5) {
        if (lengths[field_index] > remaining) {
            free(buf);
            return -1;
        }

        memcpy(destinations[field_index], p, lengths[field_index]);
        p += lengths[field_index];
        remaining -= lengths[field_index];
        field_index++;
    }

    if (remaining != 0) {
        free(buf);
        return -1;
    }

    free(buf);
    return total_length + sizeof(lengths);
}

int parse_int(char *content)
{	
	int multiplier = 1;
	char first_byte = *content;
	if (first_byte == 1) {
		multiplier = -1;
	}

	content++;
	unsigned int number = *((unsigned int *)content);
	return ntohl(number) * multiplier; 
}

float parse_short_real(char *content)
{
	unsigned short nr = *((unsigned short *)content);
	nr = ntohs(nr);
	return (nr * (1.0)) / 100;
}

float parse_float(char *content, unsigned int *decimals)
{	
	int multiplier = 1;
	char first_byte = *content;
	if (first_byte == 1) {
		multiplier = -1;
	}
	content++;
	unsigned int glued_nr = *((unsigned int *)content);
	content = content + 4;
	glued_nr = ntohl(glued_nr);

	int power = *content;
	*decimals = power;
	int res = multiplier * glued_nr;
	return ((1.0) * res) * pow(10, -power);
}

void delete_clients(struct client_info **clients, int nr_clients)
{int i = 0;
	while(i < nr_clients) {
		free((*clients)[i].IP);
		free((*clients)[i].ID);
	i++;
	}
	free(*clients);
}

// Topics matching function
int cmp_topics(char packet_topic[50], char existing_topic[50]) {
    if (strncmp(packet_topic, existing_topic, 50) == 0) {
        return 0;
    }

    char ptopic_copy[50], ltopic_copy[50];
    strncpy(ptopic_copy, packet_topic, 50);
    strncpy(ltopic_copy, existing_topic, 50);
    ptopic_copy[49] = ltopic_copy[49] = '\0';

    char pwords[20][50];
    char lwords[20][50]; 
    int pindex = 0, lindex = 0;

    char *p = strtok(ptopic_copy, "/");
    while (p != NULL && pindex < 20) {
        strncpy(pwords[pindex], p, 50);
        pwords[pindex][49] = '\0';
        pindex++;
        p = strtok(NULL, "/");
    }

    char *l = strtok(ltopic_copy, "/");
    while (l != NULL && lindex < 20) {
        strncpy(lwords[lindex], l, 50);
        lwords[lindex][49] = '\0';
        lindex++;
        l = strtok(NULL, "/");
    }

    int pi = 0, li = 0;
    while (pi < pindex && li < lindex) {
        int is_wildcard = (strcmp(lwords[li], "*") == 0) || (strcmp(lwords[li], "+") == 0);
        
        if (!is_wildcard) {
            if (strcmp(pwords[pi], lwords[li]) != 0) {
                return -1;
            }
			li = li + 1;
            pi = pi + 1;
           
        }
        else {
            if (strcmp(lwords[li], "+") == 0) {
				li = li + 1;
				pi = pi + 1;   
                continue;
            }

			pi = pi + 1;
			li = li + 1;
            if (li >= lindex) {
                return 0;
            }

            while (pi < pindex && strcmp(pwords[pi], lwords[li]) != 0) {
				pi = pi + 1;
            }
            if (pi == pindex) {
                return -1; 
            }
			pi = pi + 1;
			li = li + 1;
        }
    }

    if (pi < pindex || li < lindex) {
        if (li < lindex && strcmp(lwords[li], "*") == 0) {
            return 0;
        }
        return -1;
    }

    return 0;
}
int is_valid(char *buf) {
    if (!buf) return -1;
    
    // Fast path for "exit" command
    if (strcmp(buf, "exit") == 0) return 0;

    char *dup = strdup(buf);
    if (!dup) return -1;

    int valid = 0;
    char *command = strtok(dup, " ");
    
    if (command && (strcmp(command, "subscribe") == 0 || strcmp(command, "unsubscribe") == 0)) {
        char *topic = strtok(NULL, " ");
        if (topic) {
            valid = 1;
        }
    }

    free(dup);
    return valid ? 0 : -1;
}

void destroy_topics(struct topic_entry **topics_addr, int nr_topics)
{int i = 0 , j = 0;
	while (i < nr_topics) {
		while (j < (*topics_addr)[i].nr_subscribers) {
			free((*topics_addr)[i].subscribed_clients[j]);
			j++;
		}
	i++;
	}
	free(*topics_addr);
	*topics_addr = NULL;
}