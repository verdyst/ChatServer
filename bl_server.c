#include "blather.h"

int shutdown = 0;
int second_passed = 0;
int DO_ADVANCED = 0;
server_t server;

void blath_handler(int sig_num)
{
	shutdown = 1;
}

void ping_handler(int signum)
{
	second_passed = 1;
}


void* log_thread_func(void* x)
{
	while(1)
	{
		if(second_passed)
		{
			server_write_who(&server);
		}
		if(shutdown)
		{
			break;
		}
	}
	return NULL;
}


int main(int argc, char* argv[])
{

	if(argc < 2)
	{
		printf("Usage: ./bl_server <server name>\n");
		return 1;
	}

	// setup signal handler for signal interrupts and terms

	struct sigaction my_sa =
	{ .sa_flags = SA_RESTART, .sa_handler = blath_handler };

	sigaction(SIGINT, &my_sa, NULL);
	sigaction(SIGTERM, &my_sa, NULL);


	if(getenv("BL_ADVANCED"))
	{
		DO_ADVANCED = 1;
	}

	char server_name[MAXNAME];
	strncpy(server_name, argv[1], MAXNAME);

	// set up signal handler for pings

	if(DO_ADVANCED)
	{
		struct sigaction ping_sa =
		{.sa_flags = SA_RESTART, .sa_handler = ping_handler};

		sigaction(SIGALRM, &ping_sa, NULL);

		// create server.log file

		char server_log[MAXPATH];
		int ret = snprintf(server_log, MAXPATH, "%s.log", server_name);
		server.log_fd = open(server_log, O_CREAT | O_APPEND | O_WRONLY, DEFAULT_PERMS); // assign to log_fd

		// write initial who_t struct to log file if advanced

		who_t basic_who = {};
		basic_who.n_clients = 0;
		pwrite(server.log_fd, &basic_who, sizeof(basic_who), 0);
	}


	server_start(&server, server_name, DEFAULT_PERMS); // initialize server member vars

	// create thread for writing to log file

	pthread_t log_thread;
	if(DO_ADVANCED)
	{
		pthread_create(&log_thread, NULL, log_thread_func, NULL);
	}


	while(1)
	{
		// check sources for input
		server_check_sources(&server);

		// if join request is ready, handle it

		if(server_join_ready(&server))
		{
			server_handle_join(&server);
		}

		if(shutdown)
		{
			break;
		}


		// for each client
		// handle data if ready
		for(int i = 0; i < server.n_clients; i++)
		{
			if(server_client_ready(&server, i))
			{
				server_handle_client(&server, i);
			}
		}

		// ping system
		if(DO_ADVANCED)
		{
			alarm(ALARM_INTERVAL); // sends alarm signal every ALARM_INTERVAL
			if(second_passed)
			{
				server_tick(&server); // increase server time
				server_ping_clients(&server);
				server_remove_disconnected(&server, DISCONNECT_SECS); // remove any disconnected clients
				second_passed = 0;
			}
		}

	}

	if(DO_ADVANCED)
	{
		pthread_join(log_thread, NULL); // wait for thread to return
	}

	server_shutdown(&server); // perform shutdown functions

	return 0;
}
