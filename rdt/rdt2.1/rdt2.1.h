#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stddef.h>

#define PORT 8000       // Port to listen on
#define BUFFER_SIZE 1024  // Size of the buffer for receiving the packet


#pragma pack(push, 1)
struct RDT_Connection_Request_Packet
{
	uint16_t magic_number; /* 27 */
};

struct RDT_Connection_Request_Response_Packet
{
	uint16_t magic_number; /* 28 */
};
struct RDT_Data_Packet
{
	uint16_t checksum;
	uint8_t seq_number;
	char buffer[BUFFER_SIZE];
};

struct RDT_Response_Packet
{
	uint16_t checksum;
	uint16_t value;
};

#pragma pack(pop)

struct RDT_Connection
{
	int sock_fd;
	struct sockaddr_in peer_addr;

	int current_seq_number;
};

struct RDT_Connection rdt_listen(uint16_t port);
struct RDT_Connection rdt_connect(const char *ip, uint16_t port);
void rdt_close(struct RDT_Connection *connection);

int rdt_send(struct RDT_Connection *connection, size_t message_size, const char *message_buffer);
size_t rdt_recv(struct RDT_Connection *connection, char *message_buffer);
