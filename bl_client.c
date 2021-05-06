#include "blather.h"

simpio_t simpio;
sem_t* server_sem;

char client_name[MAXNAME];
char server_name[MAXPATH];
int to_server_fd;
int to_client_fd;
int server_log_fd;

// establish global vars for threads

pthread_t input_thread;
pthread_t server_thread;

void print_who()
{
	// read who struct and print

	sem_wait(server_sem);

	lseek(server_log_fd, 0, SEEK_SET); // set file pointer to beginning

	who_t who = {};

	int nbytes = read(server_log_fd, &who, sizeof(who)); // read who struct
																											 // located at beginning

	iprintf(&simpio, "====================\n%d CLIENTS\n", who.n_clients);
	for(int i = 0; i < who.n_clients; i++)
	{
		iprintf(&simpio, "%d: %s\n", i, who.names[i]);
	}
	iprintf(&simpio, "====================\n");
	sem_post(server_sem);
}

void print_log(int num)
{
	// seek to end of file
	lseek(server_log_fd, -num*sizeof(mesg_t), SEEK_END);

	iprintf(&simpio, "====================\nLAST %d MESSAGES\n", num);

	while(1)
	{
		mesg_t msg = {};
		int nbytes = read(server_log_fd, &msg, sizeof(msg));

		if(nbytes == 0)
		{
			break;
		}

		if(msg.kind == BL_JOINED)
		{
			iprintf(&simpio, "-- %s JOINED --\n", msg.name);
		}
		else if(msg.kind == BL_DEPARTED)
		{
			iprintf(&simpio, "-- %s DEPARTED --\n", msg.name);
		}
		else if(msg.kind == BL_SHUTDOWN)
		{
			iprintf(&simpio, "!!! Server Shutting Down !!!\n");
		}
		else
		{
			iprintf(&simpio, "[%s] : %s\n", msg.name, msg.body);
		}
	}
	iprintf(&simpio, "====================\n");
	close(server_log_fd);
}

void* input_thread_func(void* x)
{
	// get input
	while(!simpio.end_of_input)
	{
		simpio_reset(&simpio); // reset each iteration

		while(!simpio.line_ready && !simpio.end_of_input)
		{
			simpio_get_char(&simpio); // obtain character if line is not ready
		}
		if(simpio.line_ready)
		{
			// create mesg_t with line + write it to to_server fifo
			if(!strncmp("%last", simpio.buf, 5))
			{
				// obtain int from string

				char* test = simpio.buf + 6;

				// call print_log
				print_log(atoi(test));
			}
			else if(!strncmp("%who", simpio.buf, 4))
			{
				print_who();
			}
			mesg_t msg = {};
			strncpy(msg.name, client_name, MAXNAME);
			strncpy(msg.body, simpio.buf, MAXLINE);
			msg.kind = BL_MESG;
			write(to_server_fd, &msg, sizeof(mesg_t));
		}
	}
	iprintf(&simpio, "End of Input, Departing\n");
	mesg_t depart_msg = {};
	strncpy(depart_msg.name, client_name, MAXNAME);
	depart_msg.kind = BL_DEPARTED;
	write(to_server_fd, &depart_msg, sizeof(mesg_t));

	pthread_cancel(server_thread); // kill the server thread
	return NULL;
}

void* server_thread_func(void* x)
{
	// listen to server

	mesg_t msg = {};

	while(1)
	{
		// read mesg_t struct from server
		int nbytes = read(to_client_fd, &msg, sizeof(mesg_t));

		if(msg.kind == BL_SHUTDOWN)
		{
			iprintf(&simpio, "!!! server is shutting down !!!\n");
			break;
		}

		if(msg.kind == BL_MESG)
		{
			iprintf(&simpio, "[%s] : %s\n", msg.name, msg.body);
		}
		else if (msg.kind == BL_JOINED)
		{
			iprintf(&simpio, "-- %s JOINED --\n", msg.name);
		}
		else if (msg.kind == BL_DEPARTED)
		{
			iprintf(&simpio, "-- %s DEPARTED --\n", msg.name);
		}
		else if (msg.kind == BL_PING) // ensure client writes a ping back
		{
			mesg_t ping = {};
			ping.kind = BL_PING;
			write(to_server_fd, &ping, sizeof(ping));
		}
		else if(msg.kind == BL_DISCONNECTED)
		{
			iprintf(&simpio, "--- %s has disconnected ---\n", msg.name);
		}
	}

	pthread_cancel(input_thread);
	return NULL;
}

int main(int argc, char* argv[])
{
	// read name of server and name of user from cmd line args
	if(argc < 3)
	{
		printf("USAGE: ./bl_client <server name> <client name>\n");
		return 1;
	}

	// obtain names for server and client

	strncpy(server_name, argv[1], MAXPATH);
	strncpy(client_name, argv[2], MAXNAME);

	// initialize simpio

	char prompt[MAXNAME];
	int ret = snprintf(prompt, MAXNAME, "%s%s", client_name, PROMPT);

	simpio_set_prompt(&simpio, prompt); // set prompt
	simpio_reset(&simpio); // initialize io
	simpio_noncanonical_terminal_mode(); // sets terminal to compatible mode

	int do_adv = 0; // variable to indicate advanced features should be enabled

	if(getenv("BL_ADVANCED")) // only open log file is advanced enabled
	{
		do_adv = 1;
		char server_log[MAXPATH];
		int ret = snprintf(server_log, MAXPATH, "%s.log", server_name);
		server_log_fd = open(server_log, O_RDONLY);
	}


	int pid = getpid();

	// create to-server and to-client fifos

	char to_server[MAXPATH];
	sprintf(to_server, "%d.to_server.fifo", pid);

	char to_client[MAXPATH];
	sprintf(to_client, "%d.to_client.fifo", pid);

	remove(to_server);
	remove(to_client); // remove duplicate fifos

	mkfifo(to_server, DEFAULT_PERMS);
	mkfifo(to_client, DEFAULT_PERMS);

	// open the fifo's

	to_server_fd = open(to_server, O_RDWR);
	to_client_fd = open(to_client, O_RDWR);


	// write a join_t request to the server's join fifo

	join_t join_request;

	strncpy(join_request.name, client_name, MAXPATH);
	strncpy(join_request.to_client_fname, to_client, MAXPATH);
	strncpy(join_request.to_server_fname, to_server, MAXPATH);

	char server_fifo_name[MAXPATH];
	ret = snprintf(server_fifo_name, MAXPATH, "%s.fifo", server_name);
	if(ret == -1)
	{
		return 1;
	}

	int server_fd = open(server_fifo_name, O_WRONLY);

	write(server_fd, &join_request, sizeof(join_t));

	if(do_adv)
	{
		char log_sem_name[MAXNAME];
		ret = snprintf(log_sem_name, MAXNAME, "/%s.sem", server_name);
		server_sem = sem_open(log_sem_name, 0, DEFAULT_PERMS, 0); // no mode as it should be created already
	}

	// create a thread to read input
	input_thread;
	pthread_create(&input_thread, NULL, input_thread_func, NULL);

	// start a server thread to listen to the server

	server_thread;
	pthread_create(&server_thread, NULL, server_thread_func, NULL);

	// wait for threads to finish

	pthread_join(input_thread, NULL);

	pthread_join(server_thread, NULL);

	// restore standard terminal output

	simpio_reset_terminal_mode();

	if(do_adv)
	{
		sem_close(server_sem);
	}

	return 0;
}
