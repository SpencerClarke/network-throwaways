#include "rdt2.0.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
	char message_buffer[BUFFER_SIZE];
	
	struct RDT_Connection connection = rdt_connect("127.0.0.1", 8000);
	strcpy(message_buffer, "Hello world!\n");
	
	rdt_send(&connection, strlen(message_buffer), message_buffer);
	rdt_close(&connection);

	return 0;
}
