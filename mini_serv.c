#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>

typedef struct t_list
{
	int id;
	char *msg;

} s_list;

/* GLOBALS */
s_list clients[1024];
int server_fd;
/* dont forget to set this one to 0 or add_client will segv */
int g_max_id = 0;
int g_max_fd;
fd_set nfds, writefds, readfds;

void fatal_error()
{
	char *str = "fatal error\n";
	write(2, str, strlen(str));
	exit(1);
}

void args_error()
{
	char *str = "wrong number of args\n";
	write(2, str, strlen(str));
	exit(1);
}

/* sends a message to all users but not the one sending*/
void send_to_all(int ignore_fd, char *str)
{
	for (int fd = 0; fd < g_max_fd + 1; fd++)
	{
		if (FD_ISSET(fd, &writefds) && fd != ignore_fd)
			send(fd, str , strlen(str), 0);
	}
}

void add_client()
{
	struct sockaddr_in cli_addr;
	unsigned int len = sizeof(cli_addr);
	int new_fd;
	char welcome[50];

	new_fd = accept(server_fd, (struct sockaddr *)&cli_addr, &len);
	if (new_fd < 0)
		fatal_error();
	if (new_fd > g_max_fd)
		g_max_fd = new_fd;
	FD_SET(new_fd, &nfds);
	clients[new_fd].id = g_max_id++;
	clients[new_fd].msg = NULL;

	bzero(welcome, 50);
	sprintf(welcome, "client %d just arrived\n", clients[new_fd].id);
	send_to_all(new_fd, welcome);
}

void rm_client(int fd)
{
	char goodbye[50];
	bzero(goodbye, 50);
	sprintf(goodbye, "client %d just left\n", clients[fd].id);
	send_to_all(fd, goodbye);
	FD_CLR(fd, &nfds);
	close(fd);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}


int main(int ac, char**av)
{
	struct sockaddr_in servaddr;
	int port = 8080;
	char buf[1025];
	char *line = NULL;
	int ret_recv;

	if (ac !=  2)
		args_error();
	else
		port = atoi(av[1]);

	// socket create and verification 
	server_fd = socket(AF_INET, SOCK_STREAM, 0); 
	if (server_fd == -1)
		fatal_error();

	// assign IP, PORT 
	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(server_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal_error(); 
	if (listen(server_fd, 10) != 0)
		fatal_error();

	g_max_fd = server_fd;
	/* initializing nfds to contain only server_fd */
	FD_ZERO(&nfds);
	FD_SET(server_fd, &nfds);
	while (1)
	{
		/* updating all fdsets in case of add or remove of a client */
		readfds = writefds = nfds;
		/* select checks if any fd is open for write or read , while it's not , code is blocked here*/
		if (select(g_max_fd + 1, &readfds, &writefds, NULL, NULL) < 0)
			fatal_error();
		for (int fd = 0; fd < g_max_fd + 1; fd++)
		{
			/* fd is ready to be read*/
			if (FD_ISSET(fd, &readfds))
			{
				if (fd == server_fd)
					add_client();
				else
				{
					bzero(buf, sizeof(buf));
					/* recv sizeofbuf -1 so the last element of buf is always '\0'*/
					ret_recv = recv(fd, buf, sizeof(buf) - 1, 0);
					if (ret_recv == 0)
					{
						rm_client(fd);
						break;
					}
					else
						clients[fd].msg = str_join(clients[fd].msg, buf);
				}
			}
			/* fd is done reading and there s a message attached to it , so we parse it and send it to all other clients*/
			else if (FD_ISSET(fd, &nfds) && clients[fd].msg != NULL)
			{
				int line_lenght = 0;
				char *tosend = NULL;
				while(extract_message(&(clients[fd].msg), &line))
				{
					line_lenght = strlen(line);
					/* we malloc + 50 because the string "client $FD_VALUE" will never exceed that lenght"*/
					tosend = malloc(sizeof(char) * (line_lenght + 50));
					if (!tosend)
						fatal_error();
					bzero(tosend, line_lenght + 50);
					sprintf(tosend, "client %d %s", clients[fd].id, line);
					send_to_all(fd, tosend);
					free(tosend);
					tosend = NULL;
				 	free(line);
					line = NULL;
				}
				free(clients[fd].msg);
				clients[fd].msg = NULL;
			}
		}
	}
}
