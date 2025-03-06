#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>

#define PORT 8000       // Port to listen on
#define BUFFER_SIZE 1024  // Size of the buffer for receiving the packet

enum State
{
	AWAITING_RQ,
	WRITING_BLOCKS
};
int main(void)
{
	int sockfd;
	struct sockaddr_in server_addr, client_addr;
	socklen_t addr_len = sizeof(client_addr);
	char buffer[BUFFER_SIZE];

	enum State state;
	FILE *fp;
	uint16_t block_num;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all available interfaces
	server_addr.sin_port = htons(PORT);        // Set the port to listen on

	if(bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
	{
		perror("Bind failed");
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	printf("UDP Echo server listening on port %d...\n", PORT);
	state = AWAITING_RQ;
	while (1)
	{
		// Receive a message from the client
		int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
		if (n < 0) {
			perror("Receive failed");
			continue;
		}
	
		if(state == AWAITING_RQ)
		{
			if(n < 3)
			{
				perror("Invalid rq");
				continue;
			}
			if(ntohs(((uint16_t *)buffer)[0]) != 2)
			{
				perror("Unsupported operation");
				continue;
			}

			buffer[n] = '\0';  // Null-terminate the received data
			fp = fopen(buffer + 2, "w");
			if(fp == NULL)
			{
				perror("failed to open file");
				continue;
			}
			/* Send ACK */
			((uint16_t *)buffer)[0] = htons(4);
			((uint16_t *)buffer)[1] = htons(0);
			int sent = sendto(sockfd, (const char *)buffer, n, 0, (const struct sockaddr *)&client_addr, addr_len);
			if(sent < 0) 
			{
				perror("Send failed");
			} 
			else 
			{
				printf("Sent ACK to %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
			}
			state = WRITING_BLOCKS;
			block_num = 1;
		}
		else if(state == WRITING_BLOCKS)
		{
			if(n < 4)
			{
				perror("Invalid data packet");
				fclose(fp);
				state = AWAITING_RQ;
				continue;
			}
			if(ntohs(((uint16_t *)buffer)[0]) != 3)
			{
				perror("Invalid data packet op code");
				fclose(fp);
				state = AWAITING_RQ;
				continue;
			}
			if(ntohs(((uint16_t *)buffer)[1]) != block_num)
			{
				perror("Invalid data block number");
				fclose(fp);
				state = AWAITING_RQ;
				continue;
			}
			fwrite(buffer + 4, 1, n-4, fp);

			/* Send ACK */
			((uint16_t *)buffer)[0] = htons(4);
			((uint16_t *)buffer)[1] = htons(block_num++);
			int sent = sendto(sockfd, (const char *)buffer, n, 0, (const struct sockaddr *)&client_addr, addr_len);
			if(sent < 0) 
			{
				perror("Send failed");
			} 
			else 
			{
				printf("Sent ACK to %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
			}
			
			if(n < 516)
			{
				fclose(fp);
				state = AWAITING_RQ;
			}
		}

	}
	// Close the socket (this part is never reached, but it's good practice to include it)
	close(sockfd);
	return 0;
}

