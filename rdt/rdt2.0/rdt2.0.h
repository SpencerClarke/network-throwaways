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

struct RDT_Connection
{
	int sock_fd;
	struct sockaddr_in peer_addr;
};

struct RDT_Connection rdt_listen(uint16_t port);
struct RDT_Connection rdt_connect(const char *ip, uint16_t port);
void rdt_close(struct RDT_Connection *connection);

int rdt_send(struct RDT_Connection *connection, size_t message_size, const char *message_buffer);
size_t rdt_recv(struct RDT_Connection *connection, char *message_buffer);
