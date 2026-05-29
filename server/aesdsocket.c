#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

volatile sig_atomic_t signal_received = 0;

void sighandler(int sig_num) {
	signal_received = 1;
}

int ends(char *str, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (str[i] == '\n') {
			return 1;
		}
	}

	return 0;
}

void logerrno(char *msg)
{
	syslog(LOG_USER | LOG_ERR,
	       "%s: %s",
	       msg,
	       strerror(errno));
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sighandler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	hints.ai_flags = AI_PASSIVE;

	struct addrinfo *address;
	int err = getaddrinfo(NULL, "9000", &hints, &address);
	if (err) {
		syslog(LOG_USER | LOG_ERR,
		       "Failed to get address: %s",
		       gai_strerror(err));
		exit(EXIT_FAILURE);
	}

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		logerrno("Failed to open socket");

	int opt = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
		logerrno("Failed to set socket option");

	if (bind(sock, address->ai_addr,address->ai_addrlen))
		logerrno("Failed to bind socket to address");

	freeaddrinfo(address);
	address = NULL;

	if (argc > 1 && strcmp(argv[1], "-d") == 0) {
		pid_t id = fork();
		if (id == -1)
			logerrno("Failed to create daemon");

		if (id)
			_exit(EXIT_SUCCESS);

		if (setsid() == -1)
			logerrno("Failed to create a new session");

		id = fork();
		if (id == -1)
			logerrno("Failed to create daemon");

		if (id)
			_exit(EXIT_SUCCESS);

		/* TODO: Error handling. */
		umask(0);
		if (chdir("/") == -1)
			logerrno("Failed to change directory");

		close(STDIN_FILENO);
		int fd = open("/dev/null", O_RDWR);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		if (fd > 2)
			close(fd);
	}

	int outfile = open(
		"/var/tmp/aesdsocketdata",
		O_RDWR | O_APPEND | O_CREAT,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (outfile == -1)
		logerrno("Failed to open file");


	if (listen(sock, 50))
		logerrno("Failed to listen to incoming connections");

	char *buff = NULL;
	int client;
	struct sockaddr client_addr;
	while (!signal_received) {

		socklen_t client_addr_size = sizeof(client_addr);
		client = accept(sock, &client_addr, &client_addr_size);
		if (client == -1) {
			if (errno == EINTR) {
				continue;
			}

			logerrno("Failed to accept incoming connection");
		}

		struct sockaddr_in *client_in;
		client_in = (struct sockaddr_in *) &client_addr;
		syslog(
			LOG_USER | LOG_INFO,
			"Accepted connection from %s",
			inet_ntoa(client_in->sin_addr));

		size_t buff_len = sizeof(char) * 1024;
		buff = (char *) malloc(buff_len);
		if (!buff)
			logerrno("Failed to allocate buffer memory");
		memset(buff, 0, buff_len);

		ssize_t r;
		while (!signal_received
		       && (r = read(client, buff, buff_len))) {
			if (r == -1)
				logerrno("Failed to read data");
			if (r == 0)
				break;

			if (write(outfile, buff, r) == -1)
				logerrno("Failed to write data");

			sync();

			if (ends(buff, r))
				break;
		}

		free(buff);
		buff = NULL;

		off_t outfile_len = lseek(outfile, 0, SEEK_END);
		if (outfile_len == -1)
			logerrno("Failed to get file size");

		buff = (char *) malloc(sizeof(char) * outfile_len);
		if (!buff)
			logerrno("Failed to allocate buffer memory");
		memset(buff, 0, outfile_len);

		if (lseek(outfile, 0, SEEK_SET) == -1)
			logerrno("Failed to jump in file");

		if (read(outfile, buff, outfile_len) == -1)
			logerrno("Failed to read file");

		if (write(client, buff, outfile_len) == -1)
			logerrno("Failed to send data");

		free(buff);
		buff = NULL;

		if (close(client))
			logerrno("Failed to close socket");

		syslog(
			LOG_USER | LOG_INFO,
			"Closed connection from %s",
			inet_ntoa(client_in->sin_addr));
	}

	if (signal_received)
		syslog(LOG_USER | LOG_INFO, "Caught signal, exiting");

	if (buff)
		free(buff);

	if (unlink("/var/tmp/aesdsocketdata"))
		logerrno("Failed to unlink file");

	if (outfile)
		logerrno("Failed to close file");

	if (close(sock))
		logerrno("Failed to close server socket");

	exit(EXIT_SUCCESS);
}
