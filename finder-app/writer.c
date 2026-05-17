#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	if (argc < 3) {
		syslog(LOG_USER | LOG_ERR, "Too few arguments provided");
		exit(EXIT_FAILURE);
	}

	char* writefile = argv[1];
	int fd = open(
		writefile,
		O_WRONLY | O_CREAT | O_TRUNC,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd == -1) {
		syslog(
			LOG_USER | LOG_ERR,
			"Failed to open file: %s",
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	char *writestr = argv[2];
	size_t len = strlen(writestr);
	ssize_t buff = 0;

	while (len > 0) {
		buff = len;
		if (len > SSIZE_MAX)
			buff = SSIZE_MAX;

		if (write(fd, writestr, buff) == -1) {
			syslog(
				LOG_USER | LOG_ERR,
				"Failed to write to file: %s",
				strerror(errno));
			if (close(fd) == -1) {
				syslog(
					LOG_USER | LOG_ERR,
					"Failed to close file: %s",
					strerror(errno));
			}
			exit(EXIT_FAILURE);
		}
		syslog(
			LOG_USER | LOG_DEBUG,
			"Writing %s to %s",
			writestr, writefile);
		len -= buff;
	}

	if (close(fd) == -1) {
		syslog(
			LOG_USER | LOG_DEBUG,
			"Failed to close file: %s",
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
