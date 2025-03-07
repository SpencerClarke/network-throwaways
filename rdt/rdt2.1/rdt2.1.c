#include "rdt2.1.h"

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
	struct RDT_Connection_Request_Packet request;
	struct RDT_Connection_Request_Response_Packet response;

	out.current_seq_number = 0;

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
		packet_size = recvfrom(out.sock_fd, &request, 2, 0, (struct sockaddr *)&(out.peer_addr), &sockaddr_len);
		if(packet_size == sizeof(request) && ntohs(request.magic_number) == 27)
		{
			/* Send an ack to complete the handshake */
			response.magic_number = htons(28);
			while(sendto(out.sock_fd, &response, sizeof(response), 0, (struct sockaddr *)&(out.peer_addr), sockaddr_len) < 0);
			break;
		}
	}
	
	return out;
}

struct RDT_Connection rdt_connect(const char *ip, uint16_t port)
{
	struct RDT_Connection out;
	struct RDT_Connection_Request_Packet request;
	struct RDT_Connection_Request_Response_Packet response;
	ssize_t packet_size;
	socklen_t sockaddr_len = sizeof(out.peer_addr);
	
	out.current_seq_number = 0;

	out.sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(out.sock_fd < 0)
	{
		fprintf(stderr, "rdt_connect: failed to open socket\n");
		return out;
	}
	
	out.peer_addr.sin_addr.s_addr = inet_addr(ip);
	out.peer_addr.sin_family = AF_INET;
	out.peer_addr.sin_port = htons(port);

	request.magic_number = htons(27);

	for(;;)
	{
		while(sendto(out.sock_fd, &request, sizeof(request), 0, (const struct sockaddr *)&(out.peer_addr), sizeof(out.peer_addr)) < 0);
	
		packet_size = recvfrom(out.sock_fd, &response, sizeof(response), 0, (struct sockaddr *)&(out.peer_addr), &sockaddr_len);
		if(packet_size == sizeof(response) && ntohs(response.magic_number) == 28)
		{
			break;
		}
	}
	printf("rdt_connect: connected\n");
	return out;
}

void rdt_close(struct RDT_Connection *connection)
{
	printf("Closing socket\n");
	close(connection->sock_fd);
}
int rdt_send(struct RDT_Connection *connection, size_t message_size, const char *message_buffer)
{
	size_t i;
	ssize_t packet_size;
	socklen_t sockaddr_len = sizeof(connection->peer_addr);
	
	struct RDT_Data_Packet packet;
	struct RDT_Response_Packet response;

	memcpy(packet.buffer, message_buffer, message_size);	
	
	packet.seq_number = connection->current_seq_number;
	connection->current_seq_number = !(connection->current_seq_number);
	
	packet.checksum = 0;
	for(i = 0; i < message_size; i++)
	{
		packet.checksum ^= (message_buffer[i] << 8 * (i % 2));
	}
	for(;;)
	{
		while(sendto(connection->sock_fd, &packet, message_size + 3, 0, (struct sockaddr *)&(connection->peer_addr), sizeof(connection->peer_addr)) < 0);

		packet_size = recvfrom(connection->sock_fd, &response, sizeof(response), 0, (struct sockaddr *)&(connection->peer_addr), &sockaddr_len);
		if(packet_size == 4 && ntohs(response.checksum) == ntohs(response.value) && ntohs(response.value) == 1)
		{
			printf("rdt_send: received ack\n");
			break;
		}
		else
		{
			fprintf(stderr, "rdt_send: received nak\n");
		}
	}
	return 1;
}
size_t rdt_recv(struct RDT_Connection *connection, char *message_buffer)
{
	size_t i;
	ssize_t packet_size;
	socklen_t sockaddr_len = sizeof(connection->peer_addr);
	struct RDT_Data_Packet packet;
	struct RDT_Response_Packet response;
	uint16_t checksum;

	for(;;)
	{
		/* Receive data packet */
		packet_size = recvfrom(connection->sock_fd, &packet, BUFFER_SIZE + 2, 0, (struct sockaddr *)&(connection->peer_addr), &sockaddr_len);
		if(packet_size < 4)
		{
			fprintf(stderr, "rdt_recv: failed to receive packet\n");
			continue;	
		}

		/* Compute and verify checksum */
		checksum = 0;
		for(i = 0; i < packet_size - 3; i++)
		{
			checksum ^= (packet.buffer[i] << 8 * (i % 2));
		}

		if(checksum != packet.checksum)
		{
			fprintf(stderr, "rdt_recv: bit errors detected, sending nak\n");
			response.value = htons(0);
			response.checksum = response.value;
			while(sendto(connection->sock_fd, &response, sizeof(response), 0, (struct sockaddr *)&(connection->peer_addr), sockaddr_len) < 0);
			continue;
		}

		/* Verify sequence number */
		if(packet.seq_number != connection->current_seq_number)
		{
			/* Duplicate packet, send an ACK to induce sending of the next packet */

			response.value = htons(1);
			response.checksum = response.value;
			while(sendto(connection->sock_fd, &response, sizeof(response), 0, (struct sockaddr *)&(connection->peer_addr), sockaddr_len) < 0);
			continue;
		}

		printf("rdt_recv: checksum and sequence number %d verified, sending ack and flipping sequence seq_number\n", connection->current_seq_number);
		connection->current_seq_number = !(connection->current_seq_number);
		response.value = htons(1);
		response.checksum = response.value;
		while(sendto(connection->sock_fd, &response, sizeof(response), 0, (struct sockaddr *)&(connection->peer_addr), sockaddr_len) < 0);
		break;
	}

	memcpy(message_buffer, packet.buffer, packet_size - 3);
	
	return packet_size - 3;
}
