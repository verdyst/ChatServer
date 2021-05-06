#include "blather.h"


int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		printf("Usage: ./bl_showlog <filename>\n");
		return 1;
	}

	// obtain file from command line args

	char filename[MAXPATH];
	strncpy(filename, argv[1], MAXPATH);

	int log_fd = open(filename, O_RDONLY);

	if(log_fd == -1)
	{
		printf("Invalid File\n");
		return 1;
	}

	// create who_t struct for reading

	who_t whodunnit = {};

	// read initial who_t struct

	int who_bytes = read(log_fd, &whodunnit, sizeof(who_t));

	printf("%d CLIENTS\n", whodunnit.n_clients);

	for(int i = 0; i < whodunnit.n_clients; i++)
	{
		printf("%d: %s\n", i, whodunnit.names[i]);
	}

	// setup while loop to print mesg_t's in the log file

	printf("MESSAGES\n");

	// loop and print all messages by reading a mesg_t struct each iteration

	while(1)
	{
		mesg_t msg = {};

		int nbytes = read(log_fd, &msg, sizeof(msg));

		if(nbytes == 0)
		{
			break;
		}

		if(msg.kind == BL_JOINED)
		{
			printf("-- %s JOINED --\n", msg.name);
		}
		else if(msg.kind == BL_DEPARTED)
		{
			printf("-- %s DEPARTED --\n", msg.name);
		}
		else if(msg.kind == BL_SHUTDOWN)
		{
			printf("!!! Server Shutting Down !!!\n");
		}
		else
		{
			printf("[%s] : %s\n", msg.name, msg.body);
		}
	}


	close(log_fd);

	return 0;
}
