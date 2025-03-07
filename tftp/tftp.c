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
	WRITING_BLOCKS,
	SENDING_BLOCKS
};

int fill_data_buffer(char buffer[512], FILE *fp)
{
	int i;
	int c;
	for(i = 0; i < 512; i++)
	{
		if((c = getc(fp)) != EOF)
		{
			buffer[i] = (char)c;
		}
		else
		{
			break;
		}
	}

	return i;
}
int main(void)
{
	int sockfd;
	struct sockaddr_in server_addr, client_addr;
	socklen_t addr_len = sizeof(client_addr);
	char buffer[BUFFER_SIZE];

	enum State state;
	FILE *fp;
	uint16_t block_num;

	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr.sin_port = htons(PORT);

	if(bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
	{
		perror("Bind failed");
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	printf("TFTP server listening on port %d...\n", PORT);
	state = AWAITING_RQ;
	for(;;)
	{
		// Receive a message from the client
		int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
		if(n < 0) 
		{
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
			if(ntohs(((uint16_t *)buffer)[0]) == 2) /* WRQ */
			{
				buffer[BUFFER_SIZE-1] = '\0';  /* Null-terminate the received data to prevent overflow */
				fp = fopen(buffer + 2, "w");
				if(fp == NULL)
				{
					perror("failed to open file");
					continue;
				}

				/* Send ACK  blocknum=0*/
				((uint16_t *)buffer)[0] = htons(4);
				((uint16_t *)buffer)[1] = htons(0);
				int sent = sendto(sockfd, (const char *)buffer, 4, 0, (const struct sockaddr *)&client_addr, addr_len);
				if(sent < 0) 
				{
					perror("Send failed");
				} 
				else 
				{
					printf("Sent ACK to %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
				}
				block_num = 1;
				state = WRITING_BLOCKS;
			}
			
			else if(ntohs(((uint16_t *)buffer)[0]) == 1) /* RRQ */
			{
				fp = fopen(buffer + 2, "r");
				if(fp == NULL)
				{
					perror("failed to open file");

					((uint16_t *)buffer)[0] = htons(5);
					((uint16_t *)buffer)[1] = htons(1); /* Error code file not found */
					((uint8_t *)buffer)[5] = '\0'; /* Empty message string */
					
					/* Send error packet */
					int sent = sendto(sockfd, (const char *)buffer, 5, 0, (const struct sockaddr *)&client_addr, addr_len);
					if(sent < 0) 
					{
						perror("Failed to send error packet");
					} 
					else 
					{
						printf("Error packet sent to %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
					}
					continue;
				}
				block_num = 1;
				state = SENDING_BLOCKS;
			}
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
				fclose(fp);
				state = AWAITING_RQ;
				continue;
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
		if(state == SENDING_BLOCKS) /* Fall through from AWAITING_RQ */
		{
			if(block_num != 1) /* No ACK before the first block */
			{
				if(ntohs(((uint16_t *)buffer)[0]) != 4) 
				{
					perror("Error packet received while sending file");
					fclose(fp);
					state = AWAITING_RQ;
					continue;
				}
				else
				{
					printf("ACK received\n");
				}
			}

			((uint16_t *)buffer)[0] = htons(3);
			((uint16_t *)buffer)[1] = htons(block_num);

			n = fill_data_buffer(buffer + 4, fp);
			
			int sent = sendto(sockfd, (const char *)buffer, n + 4, 0, (const struct sockaddr *)&client_addr, addr_len);
			if(sent < 0) 
			{
				perror("Send failed");
				fclose(fp);
				state = AWAITING_RQ;
				continue;
			} 
			else 
			{
				printf("Sent block num %d of size %d to %s:%d\n", block_num, n, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
			}
			
			if(n < 512)
			{
				printf("File sent\n");
				fclose(fp);
				state = AWAITING_RQ;
			}
			block_num++;
		}
		
	}
	close(sockfd);
	return 0;
}

