#include "rdt2.0.h"

#define PORT 8000       // Port to listen on
#define BUFFER_SIZE 1024  // Size of the buffer for receiving the packet


int rdt_send_ack(struct RDT_Connection *connection, int nak)
{
	uint16_t ack;

	if(nak)
	{
		ack = htons(0);
	}
	else
	{
		ack = htons(27);
	}
	
	if(sendto(connection->sock_fd, &ack, 2, 0, (const struct sockaddr *)&(connection->peer_addr), sizeof(connection->peer_addr)) < 0)
	{
		fprintf(stderr, "failed to send ack or nack\n");
		return 0;
	}
	return 1;
}

struct RDT_Connection rdt_listen(uint16_t port)
{
	struct RDT_Connection out;
	socklen_t sockaddr_len = sizeof(out.peer_addr);
	ssize_t packet_size;
	uint16_t buffer;

	memset(&(out.peer_addr), 0, sizeof(out.peer_addr));
	
	out.sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(out.sock_fd < 0)
	{
		fprintf(stderr, "rdt_listen: failed to open socket\n");
		return out;
	}

	out.peer_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	out.peer_addr.sin_family = AF_INET;
	out.peer_addr.sin_port = htons(port);
	
	if(bind(out.sock_fd, (struct sockaddr *)&(out.peer_addr), sizeof(out.peer_addr)) < 0)
	{
		fprintf(stderr, "rdt_listen: failed to bind address to socket\n");
		return out;
	}

	/* Wait for a valid connection request */
	/* A connection request is 2 bytes containing the value 26 */
	for(;;)
	{
		printf("Server listening on port %d\n", (int)port);
		packet_size = recvfrom(out.sock_fd, &buffer, 2, 0, (struct sockaddr *)&(out.peer_addr), &sockaddr_len);
		if(packet_size == 2 && ntohs(buffer) == 26)
		{
			/* Send an ack to complete the handshake */
			rdt_send_ack(&out, 0);
			break;
		}
	
		rdt_send_ack(&out, 1);
	}
	
	return out;
}
struct RDT_Connection rdt_connect(const char *ip, uint16_t port)
{
	struct RDT_Connection out;
	uint16_t connection_req_packet = htons(26);
	ssize_t packet_size;
	socklen_t sockaddr_len = sizeof(out.peer_addr);

	out.sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(out.sock_fd < 0)
	{
		fprintf(stderr, "rdt_connect: failed to open socket\n");
		return out;
	}
	
	out.peer_addr.sin_addr.s_addr = inet_addr(ip);
	out.peer_addr.sin_family = AF_INET;
	out.peer_addr.sin_port = htons(port);

	if(sendto(out.sock_fd, &connection_req_packet, 2, 0, (const struct sockaddr *)&(out.peer_addr), sizeof(out.peer_addr)) < 0)
	{
		fprintf(stderr, "rdt_connect: failed to send connection request packet\n");
		return out;
	}
	
	packet_size = recvfrom(out.sock_fd, &connection_req_packet, 2, 0, (struct sockaddr *)&(out.peer_addr), &sockaddr_len);
	if(packet_size != 2 || ntohs(connection_req_packet) != 27)
	{
		fprintf(stderr, "rdt_connect: failed to receive initial handshake ack\n");
		return out;
	}

	printf("rdt_connect: connected\n");
	return out;
}

void rdt_close(struct RDT_Connection *connection)
{
	close(connection->sock_fd);
}
int rdt_send(struct RDT_Connection *connection, size_t message_size, const char *message_buffer)
{
	size_t i;
	ssize_t packet_size;
	socklen_t sockaddr_len = sizeof(connection->peer_addr);
	char modified_buffer[BUFFER_SIZE + 2];
	uint16_t ack_buffer;

	memcpy(modified_buffer + 2, message_buffer, message_size);	
	modified_buffer[0] = modified_buffer[1] = 0;
	for(i = 0; i < message_size; i++)
	{
		modified_buffer[i % 2] ^= message_buffer[i];
	}

	for(;;)
	{
		if(sendto(connection->sock_fd, modified_buffer, message_size + 2, 0, (struct sockaddr *)&(connection->peer_addr), sizeof(connection->peer_addr)) < 0)
		{
			fprintf(stderr, "rdt_send: failed to send\n");
			return 0;
		}

		packet_size = recvfrom(connection->sock_fd, &ack_buffer, 2, 0, (struct sockaddr *)&(connection->peer_addr), &sockaddr_len);
		if(packet_size == 2 && ntohs(ack_buffer) == 27)
		{
			printf("rdt_send: received ack\n");
			break;
		}
		else if(packet_size == 2 && ntohs(ack_buffer) == 0)
		{
			fprintf(stderr, "rdt_send: received nak\n");
		}
		else
		{
			fprintf(stderr, "rdt_send: received invalid ack\n");
		}
	}
	return 1;
}
size_t rdt_recv(struct RDT_Connection *connection, char *message_buffer)
{
	size_t i;
	ssize_t packet_size;
	socklen_t sockaddr_len = sizeof(connection->peer_addr);
	char modified_buffer[BUFFER_SIZE + 2];
	uint8_t checksum[2];
	
	for(;;)
	{
		packet_size = recvfrom(connection->sock_fd, modified_buffer, BUFFER_SIZE + 2, 0, (struct sockaddr *)&(connection->peer_addr), &sockaddr_len);
		if(packet_size < 2)
		{
			fprintf(stderr, "rdt_recv: failed to receive packet\n"); 
		}
		checksum[0] = checksum[1] = 0;
		for(i = 0; i < packet_size - 2; i++)
		{
			checksum[i % 2] ^= modified_buffer[i+2];
		}
		if(modified_buffer[0] != checksum[0] || modified_buffer[1] != checksum[1])
		{
			fprintf(stderr, "rdt_recv: bit errors detected, sending nak\n");
			rdt_send_ack(connection, 1);
		}
		else
		{
			printf("rdt_recv: checksum verified, sending ack\n");
			rdt_send_ack(connection, 0);
			break;
		}
	}
	memcpy(message_buffer, modified_buffer + 2, packet_size - 2);
	return packet_size - 2;
}
