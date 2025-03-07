#include "rdt2.0.h"
#include <stdio.h>

int main(void)
{
	char message_buffer[BUFFER_SIZE];
	
	struct RDT_Connection connection = rdt_listen(8000);
	
	size_t message_size = rdt_recv(&connection, message_buffer);
	printf("Message: ");
	for(size_t i = 0; i < message_size; i++)
	{
		putchar(message_buffer[i]);
	}
	
	rdt_close(&connection);

	return 0;
}
