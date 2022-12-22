#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct s_client {
	int 	        id;
	int 	        fd;
	int		        new_msg;
	struct s_client *next;
} t_client;

int         max_fd = 0;
int         id = 0;
fd_set      all_sock, read_sock, write_sock;
t_client    *client_list = NULL;

void error(char *msg) {
    if (msg)
        write(2, msg, strlen(msg));
    else
        write(2, "Fatal error\n", 12);
    exit(1);
}

void    add_client_to_list(int new_connection) {
	t_client    *new_client = (t_client *)calloc(1, sizeof(t_client));

	new_client->fd = new_connection;
	new_client->id = id;
	new_client->next = NULL;
	new_client->new_msg = 1;

    t_client    *last = client_list;
	if (client_list == NULL)
		client_list = new_client;
	else {
		while (last->next)
			last = last->next;
		last->next = new_client;
	}
}


void broadcast(int sender_fd, char *raw_msg, int id) {
	char        msg[200];
	t_client    *client = client_list;
    
	sprintf(msg, raw_msg, id);
	while (client) {
		if (FD_ISSET(client->fd, &write_sock) && client->fd != sender_fd)
			send(client->fd, msg, strlen(msg), 0);
		client = client->next;
	}
}

void accept_connection(int serv_sock) {
	int new_connection = accept(serv_sock, NULL, NULL);

	FD_SET(new_connection, &all_sock);
    max_fd = new_connection > max_fd ? new_connection : max_fd;
	add_client_to_list(new_connection);
	broadcast(new_connection,"server: client %d just arrived\n", id);
	id++;
}

void    remove_client(t_client *client) 
{
	FD_CLR(client->fd, &all_sock);
	close(client->fd);
	t_client    *tmp = client_list;

	if (client_list == client) {
		client_list = client->next;
		free(client); // ballec
	}
	else {
		while (tmp && tmp->next != client)
			tmp = tmp->next;
		tmp->next = client->next;
		free(client);
	}
}

void handle_msgs() {
    t_client    *client = client_list;

    while (client) {
        if (FD_ISSET(client->fd, &read_sock)) {
			char    c;
			int     ret = recv(client->fd, &c, 1, 0);

			//client disconnected
			if (ret <= 0) {
				broadcast(client->fd, "server: client %d just left\n", client->id);
				remove_client(client);
			}
			else {
				if (client->new_msg) {
					broadcast(client->fd, "client %d: ", client->id);
					client->new_msg = 0;
				}
				if (c == '\n')
					client->new_msg = 1;
				broadcast(client->fd, &c, -1);
			}
		}
        client = client->next;
    }	
}

void run_server(int serv_sock) {
	FD_ZERO(&all_sock);
	FD_SET(serv_sock, &all_sock);
	max_fd = serv_sock;

	while (1) {
		read_sock = write_sock = all_sock;

		select(max_fd + 1, &read_sock, &write_sock, NULL, NULL);
		if (FD_ISSET(serv_sock, &read_sock))
			accept_connection(serv_sock);
		else
			handle_msgs();
	}
}

int main(int argc, char **argv) {
	if (argc < 2)
        error("Wrong number of arguments\n");

	int                 sockfd;
	struct sockaddr_in  servaddr; 

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1)
		error(NULL);	
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1])); //ATOI here

	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		error(NULL);	
	if (listen(sockfd, 10) != 0) 
		error(NULL);

	run_server(sockfd);
	return (0);
}