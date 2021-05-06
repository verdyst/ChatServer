#include "blather.h"

extern int DO_ADVANCED; // set up to access initialized var from bl_server

client_t *server_get_client(server_t *server, int idx)
{
	if(idx >= server->n_clients)
	{
		return NULL;
	}

	return &server->client[idx];

}
// Gets a pointer to the client_t struct at the given index. If the
// index is beyond n_clients, the behavior of the function is
// unspecified and may cause a program crash.

void server_start(server_t *server, char *server_name, int perms)
{
	log_printf("BEGIN: server_start()\n");
	strncpy(server->server_name, server_name, MAXPATH); // set server name
	// remove(server->server_name); // remove any files prev named server_name
	char server_fifo[MAXPATH];
	snprintf(server_fifo, MAXPATH, "%s.fifo", server_name); // copy to get fifo name
	remove(server_fifo); // remove duplicate instances of server fifo
	mkfifo(server_fifo, S_IRUSR | S_IWUSR);

	int server_fd = open(server_fifo, O_RDWR); // open fifo



	if(DO_ADVANCED)
	{
		char server_sem[MAXNAME];
		snprintf(server_sem, MAXNAME, "/%s.sem", server_name);
		server->log_sem = sem_open(server_sem, O_CREAT, DEFAULT_PERMS, 1);
	}


	server->join_fd = server_fd; // set join_fd to the fifo's fd
	server->time_sec = 0;

	log_printf("END: server_start()\n");
}
// Initializes and starts the server with the given name. A join fifo
// called "server_name.fifo" should be created. Removes any existing
// file of that name prior to creation. Opens the FIFO and stores its
// file descriptor in join_fd.
//
// ADVANCED: create the log file "server_name.log" and write the
// initial empty who_t contents to its beginning. Ensure that the
// log_fd is position for appending to the end of the file. Create the
// POSIX semaphore "/server_name.sem" and initialize it to 1 to
// control access to the who_t portion of the log.
//
// LOG Messages:
// log_printf("BEGIN: server_start()\n");              // at beginning of function
// log_printf("END: server_start()\n");                // at end of function

void server_broadcast(server_t *server, mesg_t *mesg);

void server_shutdown(server_t *server)
{
	log_printf("BEGIN: server_shutdown()\n");
	char server_fifo_name[MAXNAME];
	if(snprintf(server_fifo_name, MAXPATH, "%s.fifo", server->server_name) == -1)
	{
		return; // Error
	}
	close(server->join_fd);
	remove(server_fifo_name); // unlink server

	mesg_t mesg =
	{ .kind = BL_SHUTDOWN, .name = "server" };

	server_broadcast(server, &mesg); // broadcast shutdown message
	for(int i = 0; i < server->n_clients; i++)
	{
			server_remove_client(server, i); // remove clients in iterative order
	}
	if(DO_ADVANCED)
	{
		close(server->log_fd); // close log file, but don't remove
		sem_close(server->log_sem);

		char server_sem[MAXNAME];
		int ret = snprintf(server_sem, MAXNAME, "/%s.sem", server->server_name);

		sem_unlink(server_sem);
	}
	log_printf("END: server_shutdown()\n");
}
// Shut down the server. Close the join FIFO and unlink (remove) it so
// that no further clients can join. Send a BL_SHUTDOWN message to all
// clients and proceed to remove all clients in any order.
//
// ADVANCED: Close the log file. Close the log semaphore and unlink
// it.
//
// LOG Messages:
// log_printf("BEGIN: server_shutdown()\n");           // at beginning of function
// log_printf("END: server_shutdown()\n");             // at end of function

int server_add_client(server_t *server, join_t *join)
{
	log_printf("BEGIN: server_add_client()\n");
	client_t client = {};
	strncpy(client.name, join->name, MAXPATH);


	// open fifo's
	client.to_client_fd = open(join->to_client_fname, O_WRONLY);
	client.to_server_fd = open(join->to_server_fname, O_RDONLY);


	// copy fifo names to client struct
	strncpy(client.to_client_fname, join->to_client_fname, MAXPATH);
	strncpy(client.to_server_fname, join->to_server_fname, MAXPATH);


	// initialize member variables
	client.data_ready = 0;
	client.last_contact_time = server->time_sec;

	if(server->n_clients == MAXCLIENTS)
	{
		return -1;
	}

	// copy the client struct into the server's client array. This avoids
	// needing to malloc memory for client.
	memcpy(&server->client[server->n_clients], &client, sizeof(client_t));
	server->n_clients++;

	log_printf("END: server_add_client()\n");

	return 0;
}
// Adds a client to the server according to the parameter join which
// should have fileds such as name filed in.  The client data is
// copied into the client[] array and file descriptors are opened for
// its to-server and to-client FIFOs. Initializes the data_ready field
// for the client to 0. Returns 0 on success and non-zero if the
// server as no space for clients (n_clients == MAXCLIENTS).
//
// LOG Messages:
// log_printf("BEGIN: server_add_client()\n");         // at beginning of function
// log_printf("END: server_add_client()\n");           // at end of function

int server_remove_client(server_t *server, int idx)
{
	client_t client = server->client[idx];

	// close client fd's
	if((close(client.to_client_fd) || close(client.to_server_fd)) == -1)
	{
		return 1;
	}

	// remove client fifo's
	if((remove(client.to_client_fname) || remove(client.to_server_fname)) == -1)
	{
		return 1;
	}

	// move each client back one in the array to preserve order,
	// starting with the index of the removed client

	server->n_clients--; // decrement number of clients before iterating to
											 // make for loop more seamless

	for(int i = idx; i < server->n_clients; i++)
	{
		server->client[i] = server->client[i+1];
	}


	return 0;
}
// Remove the given client likely due to its having departed or
// disconnected. Close fifos associated with the client and remove
// them.  Shift the remaining clients to lower indices of the client[]
// preserving their order in the array; decreases n_clients. Returns 0
// on success, 1 on failure.

void server_broadcast(server_t *server, mesg_t *mesg)
{
	for(int i = 0; i < server->n_clients; i++)
	{
		write(server->client[i].to_client_fd, mesg, sizeof(mesg_t));
	}
	if(DO_ADVANCED && mesg->kind != BL_PING)
	{
		server_log_message(server, mesg); // log all non-ping messages
	}
}
// Send the given message to all clients connected to the server by
// writing it to the file descriptors associated with them.
//
// ADVANCED: Log the broadcast message unless it is a PING which
// should not be written to the log.

void server_check_sources(server_t *server)
{
	log_printf("BEGIN: server_check_sources()\n");
	struct pollfd input_pfds[server->n_clients+1]; // set up polls array to check I/O
																								 // add one for server's join fifo
	for(int i = 0; i < server->n_clients; i++)
	{
		input_pfds[i].fd = server->client[i].to_server_fd; // source of client input
		input_pfds[i].events = POLLIN; // poll for input
	}

	input_pfds[server->n_clients].fd = server->join_fd; // to check for join input
	input_pfds[server->n_clients].events = POLLIN;

	log_printf("poll()'ing to check %d input sources\n", server->n_clients+1);

	int result = poll(input_pfds, server->n_clients+1, -1);

	log_printf("poll() completed with return value %d\n", result);

	if(result == -1)
	{
		log_printf("poll() interrupted by a signal\n");
		log_printf("END: server_check_sources()\n");
		return;
	}

	if(input_pfds[server->n_clients].revents & POLLIN) // check for join input
	{
		server->join_ready = 1;
	}
	log_printf("join_ready = %d\n", server->join_ready);


	for(int i = 0; i < server->n_clients; i++)
	{
		if(input_pfds[i].revents & POLLIN) // indicates input in file
		{
			server->client[i].data_ready = 1;
		}
		log_printf("client %d '%s' data_ready = %d\n", i, server->client[i].name, server->client[i].data_ready);
	}

	log_printf("END: server_check_sources()\n");
}
// Checks all sources of data for the server to determine if any are
// ready for reading. Sets the servers join_ready flag and the
// data_ready flags of each of client if data is ready for them.
// Makes use of the poll() system call to efficiently determine which
// sources are ready.
//
// NOTE: the poll() system call will return -1 if it is interrupted by
// the process receiving a signal. This is expected to initiate server
// shutdown and is handled by returning immediagely from this function.
//
// LOG Messages:
// log_printf("BEGIN: server_check_sources()\n");             // at beginning of function
// log_printf("poll()'ing to check %d input sources\n",...);  // prior to poll() call
// log_printf("poll() completed with return value %d\n",...); // after poll() call
// log_printf("poll() interrupted by a signal\n");            // if poll interrupted by a signal
// log_printf("join_ready = %d\n",...);                       // whether join queue has data
// log_printf("client %d '%s' data_ready = %d\n",...)         // whether client has data ready
// log_printf("END: server_check_sources()\n");               // at end of function

int server_join_ready(server_t *server)
{
	return server->join_ready;
}
// Return the join_ready flag from the server which indicates whether
// a call to server_handle_join() is safe.

void server_handle_join(server_t *server)
{
	log_printf("BEGIN: server_handle_join()\n");
	if(!server_join_ready) // if join_ready is 0
	{
		return;
	}

	join_t new_client = {};

	read(server->join_fd, &new_client, sizeof(join_t)); // read join struct from
																											// server join fifo

	log_printf("join request for new client '%s'\n", new_client.name);

	server_add_client(server, &new_client);

	server->join_ready = 0;

	// send join message to all connected clients

	mesg_t join_msg = {};
	join_msg.kind = BL_JOINED;
	strncpy(join_msg.name, new_client.name, MAXNAME);
	server_broadcast(server, &join_msg);

	log_printf("END: server_handle_join()\n");
}
// Call this function only if server_join_ready() returns true. Read a
// join request and add the new client to the server. After finishing,
// set the servers join_ready flag to 0.
//
// LOG Messages:
// log_printf("BEGIN: server_handle_join()\n");               // at beginnning of function
// log_printf("join request for new client '%s'\n",...);      // reports name of new client
// log_printf("END: server_handle_join()\n");                 // at end of function

int server_client_ready(server_t *server, int idx)
{
	return server->client[idx].data_ready;
}
// Return the data_ready field of the given client which indicates
// whether the client has data ready to be read from it.

void server_handle_client(server_t *server, int idx)
{
	log_printf("BEGIN: server_handle_client()\n");

	if(!server_client_ready(server, idx))
	{
		return;
	}

	mesg_t msg = {};

	read(server->client[idx].to_server_fd, &msg, sizeof(mesg_t)); // read into msg

	mesg_kind_t msg_kind = msg.kind;

	if(msg_kind == BL_MESG)
	{
		log_printf("client %d '%s' MESSAGE '%s'\n", idx, msg.name, msg.body);
		server_broadcast(server, &msg);
	}
	else if(msg_kind == BL_JOINED || msg.kind == BL_SHUTDOWN)
	{
		server_broadcast(server, &msg);
	}
	else if(msg_kind == BL_DEPARTED)
	{
		log_printf("client %d '%s' DEPARTED\n", idx, msg.name);
		server_remove_client(server, idx); // remove client after departure
		server_broadcast(server, &msg);
	}

	// update contact time whenever server receives a message from a client
	server->client[idx].last_contact_time = server->time_sec;

	server->client[idx].data_ready = 0; // client's data has been handled

	log_printf("END: server_handle_client()\n");
}
// Process a message from the specified client. This function should
// only be called if server_client_ready() returns true. Read a
// message from to_server_fd and analyze the message kind. Departure
// and Message types should be broadcast to all other clients.  Ping
// responses should only change the last_contact_time below. Behavior
// for other message types is not specified. Clear the client's
// data_ready flag so it has value 0.
//
// ADVANCED: Update the last_contact_time of the client to the current
// server time_sec.
//
// LOG Messages:
// log_printf("BEGIN: server_handle_client()\n");           // at beginning of function
// log_printf("client %d '%s' DEPARTED\n",                  // indicates client departed
// log_printf("client %d '%s' MESSAGE '%s'\n",              // indicates client message
// log_printf("END: server_handle_client()\n");             // at end of function

void server_tick(server_t *server)
{
	server->time_sec++;
}
// ADVANCED: Increment the time for the server

void server_ping_clients(server_t *server)
{
	mesg_t ping = {};
	ping.kind = BL_PING;
	server_broadcast(server, &ping);
}
// ADVANCED: Ping all clients in the server by broadcasting a ping.

void server_remove_disconnected(server_t *server, int disconnect_secs)
{
	for(int i = 0; i < server->n_clients; i++)
	{

		if((server->time_sec - server->client[i].last_contact_time) >= disconnect_secs) // if true, difference in time
		{																																								// indicates that client has disconnected
			mesg_t disconnect_msg = {};
			disconnect_msg.kind = BL_DISCONNECTED;
			strncpy(disconnect_msg.name, server->client[i].name, MAXNAME);
			if(server_remove_client(server, i) == 0) // if client removal was successful
			{
				// clients will be shifted over since a client was removed
				server_broadcast(server, &disconnect_msg);
				i--; // need to decrement i to account for client list changes
			}
		}
	}
}
// ADVANCED: Check all clients to see if they have contacted the
// server recently. Any client with a last_contact_time field equal to
// or greater than the parameter disconnect_secs should be
// removed. Broadcast that the client was disconnected to remaining
// clients.  Process clients from lowest to highest and take care of
// loop indexing as clients may be removed during the loop
// necessitating index adjustments.

void server_write_who(server_t *server)
{
	sem_wait(server->log_sem);

	// opening a separate fd for the log file is necessary as there is a bug with
	// Linux wherein pwrite does not work on fd's opened with O_APPEND.
	// A workaround for this is to just open a separate fd each time so that
	// the fd position is always at 0.

	char log_copy[MAXPATH];
	int ret = snprintf(log_copy, MAXPATH, "%s.log", server->server_name);
	int log_copy_fd = open(log_copy, O_WRONLY, DEFAULT_PERMS);
	who_t who = {};

	who.n_clients = server->n_clients;

	for(int i = 0; i < server->n_clients; i++)
	{
		strncpy(who.names[i], server->client[i].name, MAXNAME);
	}

	ret = write(log_copy_fd, &who, sizeof(who));

	sem_post(server->log_sem);
	close(log_copy_fd);
}
// ADVANCED: Write the current set of clients logged into the server
// to the BEGINNING the log_fd. Ensure that the write is protected by
// locking the semaphore associated with the log file. Since it may
// take some time to complete this operation (acquire semaphore then
// write) it should likely be done in its own thread to preven the
// main server operations from stalling.  For threaded I/O, consider
// using the pwrite() function to write to a specific location in an
// open file descriptor which will not alter the position of log_fd so
// that appends continue to write to the end of the file.

void server_log_message(server_t *server, mesg_t *mesg)
{
	int nbytes = write(server->log_fd, mesg, sizeof(mesg_t));
}
// ADVANCED: Write the given message to the end of log file associated
// with the server.
