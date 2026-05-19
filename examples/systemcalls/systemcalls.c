#include "systemcalls.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * DONE  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
	int ret = system(cmd);
	if (!WIFEXITED(ret)) {
		switch(ret) {
		case -1:
			syslog(
				LOG_USER | LOG_ERR,
				"Failed executing command '%s': %s",
				cmd,
				strerror(errno));
			break;
		case 0:
			syslog(
				LOG_USER | LOG_DEBUG,
				"Command is NULL, but shell is available");
			break;
		default:
			syslog(
				LOG_USER | LOG_DEBUG,
				"Command is NULL, and shell is not available");
			break;
		}

		return false;
	}

	if (WEXITSTATUS(ret)) {
		switch(WEXITSTATUS(ret)) {
		case 127:
			syslog(
				LOG_USER | LOG_ERR,
				"Could not execute shell in the child process");
			break;
		default:
			syslog(
				LOG_USER | LOG_ERR,
				"The child process exited with error status %d",
				WEXITSTATUS(ret));
			break;
		}

		return false;
	}

    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
	command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

/*
 * DONE:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    pid_t pid = fork();
    if (pid == -1) {
	    syslog(LOG_USER | LOG_ERR,
		   "Failed to launch child process: %s",
		   strerror(errno));

	    return false;
    }

    if (!pid) {
	    int ret = execv(command[0], command);
	    if (ret == -1) {
		    syslog(LOG_USER | LOG_ERR,
			   "Failed to launch command '%s': %s",
			   command[0],
			   strerror(errno));

		    exit(EXIT_FAILURE);
	    }
    }

    int wstatus = 0;
    if (wait(&wstatus) == -1) {
	    syslog(
		    LOG_USER | LOG_ERR,
		    "Failed to wait for child process: %s",
		    strerror(errno));

	    return false;
    }

    if (!WIFEXITED(wstatus)) {
	    syslog(LOG_USER | LOG_ERR,
		   "Child did not exit normally");

	    return false;
    }

    if (WEXITSTATUS(wstatus)) {
	    syslog(LOG_USER | LOG_ERR,
		   "Child exited with error status: %d",
		   WEXITSTATUS(wstatus));

	    return false;
    }

    syslog(
	    LOG_USER | LOG_DEBUG,
	    "Child done with exit status: %d",
	    WEXITSTATUS(wstatus));

    va_end(args);

    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
	command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed


/*
 * DONE
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    int fd = open(outputfile,
		  O_WRONLY|O_TRUNC|O_CREAT,
		  0644);
    if (fd < 0) {
	    perror("open");
	    return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
	    syslog(LOG_USER | LOG_ERR,
		   "Failed to launch child process: %s",
		   strerror(errno));

	    return false;
    }

    if (!pid) {
	    if (dup2(fd, 1) < 0) {
		    syslog(LOG_USER | LOG_ERR,
			   "Failed to duplicate to stdout: %s",
			   strerror(errno));
		    exit(EXIT_FAILURE);
	    }
	    close(fd);

	    int ret = execv(command[0], command);
	    if (ret == -1) {
		    syslog(LOG_USER | LOG_ERR,
			   "Failed to launch command '%s': %s",
			   command[0],
			   strerror(errno));

		    exit(EXIT_FAILURE);
	    }
    }

    int wstatus = 0;
    if (wait(&wstatus) == -1) {
	    syslog(
		    LOG_USER | LOG_ERR,
		    "Failed waiting for child process: %s",
		    strerror(errno));

	    return false;
    }

    if (!WIFEXITED(wstatus)) {
	    syslog(LOG_USER | LOG_ERR,
		   "Child did not exit normally");
	    close(fd);

	    return false;
    }

    if (WEXITSTATUS(wstatus)) {
	    syslog(LOG_USER | LOG_ERR,
		   "Child exited with error status: %d",
		   WEXITSTATUS(wstatus));

	    return false;
    }

    syslog(
	    LOG_USER | LOG_DEBUG,
	    "Child done with exit status: %d",
	    WEXITSTATUS(wstatus));

    close(fd);

    va_end(args);

    return true;
}
