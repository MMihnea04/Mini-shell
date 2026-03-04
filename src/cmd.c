// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cmd.h"
#include "utils.h"

#define READ        0
#define WRITE       1

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	/* TODO: Execute cd. */
	char *dir_path;

	// if we don't have dir, go to HOME
	if (dir == NULL) {
		dir_path = getenv("HOME");
		if (dir_path == NULL)
			return 1;
		return chdir(dir_path) != 0 ? 1 : 0;
	}

	// obtain dir path from word and verify if it's valid
	dir_path = get_word(dir);
	if (dir_path == NULL)
		return 1;
	// attempt to change dir and verify result
	int rez = chdir(dir_path);

	free(dir_path);
	if (rez != 0)
		return 1;
	return 0;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* TODO: Execute exit/quit. */
	return SHELL_EXIT;
}

/**
 * Internal pwd command.
 */
static int shell_pwd(void)
{
	// buffer for current dir and obtain working dir path
	char crt_dir[1024];

	if (getcwd(crt_dir, sizeof(crt_dir)) == NULL)
		return 1;
	// display current dir
	printf("%s\n", crt_dir);
	return 0;
}

/**
 * Helper function to handle redirections for builtin commands.
 */
static int do_redirect(word_t *file, int io_flag, int append_bit, int target_fd)
{
	if (file == NULL)
		return -1;
	// obtain file name and set flags for open
	char *filename = get_word(file);
	int flags = O_WRONLY | O_CREAT | ((io_flag & append_bit) ? O_APPEND : O_TRUNC);
	// open file and verify if it worked
	int file_descr = open(filename, flags, 0644);

	free(filename);
	if (file_descr < 0)
		return -1;
	// save original fd for restoration later
	int saved_fd = dup(target_fd);

	// redirect target fd to file and close file descriptor
	dup2(file_descr, target_fd);
	close(file_descr);
	return saved_fd;
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *simple_cmd, int depth, command_t *parent_cmd)
{
	/* TODO: Sanity checks. */
	if (simple_cmd == NULL || simple_cmd->verb == NULL)
		return 0;
	char *cmd_name = get_word(simple_cmd->verb);

	if (cmd_name == NULL)
		return 0;

	/* TODO: If builtin command, execute the command. */
	// verify if it's cd cmd and do redirect if needed
	if (strcmp(cmd_name, "cd") == 0) {
		int org_stdout = do_redirect(simple_cmd->out, simple_cmd->io_flags, IO_OUT_APPEND, STDOUT_FILENO);
		int saved_err = do_redirect(simple_cmd->err, simple_cmd->io_flags, IO_ERR_APPEND, STDERR_FILENO);
		// execute cd and save result
		int rez = shell_cd(simple_cmd->params);

		// restore stdout if it was redirected
		if (org_stdout >= 0) {
			dup2(org_stdout, STDOUT_FILENO);
			close(org_stdout);
		}
		// restore stderr if it was redirected
		if (saved_err >= 0) {
			dup2(saved_err, STDERR_FILENO);
			close(saved_err);
		}
		free(cmd_name);
		return rez;
	}

	// verify if it's pwd cmd
	if (strcmp(cmd_name, "pwd") == 0) {
		// redirect stdout if needed and execute pwd
		int saved = do_redirect(simple_cmd->out, simple_cmd->io_flags, IO_OUT_APPEND, STDOUT_FILENO);
		int rez = shell_pwd();

		// restore stdout if it was redirected
		if (saved >= 0) {
			dup2(saved, STDOUT_FILENO);
			close(saved);
		}
		free(cmd_name);
		return rez;
	}

	// verify if it's exit or quit and return special code
	if (strcmp(cmd_name, "exit") == 0 || strcmp(cmd_name, "quit") == 0) {
		free(cmd_name);
		return shell_exit();
	}

	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */
	// verify if it's var assignment (contains =)
	if (strchr(cmd_name, '=') != NULL) {
		// separate var name from value at =
		*strchr(cmd_name, '=') = '\0';
		char *var_value = strchr(cmd_name, '\0') + 1;

		// if we have params, var value comes from params
		if (simple_cmd->params != NULL) {
			char *args_value = get_word(simple_cmd->params);

			if (args_value != NULL) {
				// set env var with value from params
				setenv(cmd_name, args_value, 1);
				free(args_value);
			} else {
				// use value after =
				setenv(cmd_name, var_value, 1);
			}
		} else {
			// we don't have params, use value after =
			setenv(cmd_name, var_value, 1);
		}
		free(cmd_name);
		return 0;
	}

	/* TODO: If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */
	// create child process to execute external cmd
	pid_t process_id = fork();

	if (process_id < 0) {
		free(cmd_name);
		return 1;
	}
	// code in child process
	if (process_id == 0) {
		int file_descr;

		// redirect stdin if needed
		if (simple_cmd->in != NULL) {
			char *inp_file = get_word(simple_cmd->in);

			// open file for reading and redirect stdin
			file_descr = open(inp_file, O_RDONLY);
			free(inp_file);
			if (file_descr < 0)
				exit(EXIT_FAILURE);
			dup2(file_descr, STDIN_FILENO);
			close(file_descr);
		}
		// redirect stdout if needed
		if (simple_cmd->out != NULL) {
			char *outp_file = get_word(simple_cmd->out);
			// set flags for append or trunc
			int flags = O_WRONLY | O_CREAT | ((simple_cmd->io_flags & IO_OUT_APPEND) ? O_APPEND : O_TRUNC);

			// open file and redirect stdout
			file_descr = open(outp_file, flags, 0644);
			if (file_descr < 0) {
				free(outp_file);
				exit(EXIT_FAILURE);
			}
			dup2(file_descr, STDOUT_FILENO);
			// verify if stderr goes to same file
			if (simple_cmd->err != NULL) {
				char *err_file = get_word(simple_cmd->err);

				if (strcmp(outp_file, err_file) == 0)
					dup2(file_descr, STDERR_FILENO);
				free(err_file);
			}
			close(file_descr);
			free(outp_file);
		}
		// redirect stderr if needed and not already redirected to same file as stdout
		if (simple_cmd->err != NULL && (simple_cmd->out == NULL ||
				strcmp(get_word(simple_cmd->out), get_word(simple_cmd->err)) != 0)) {
			char *err_file = get_word(simple_cmd->err);
			int flags = O_WRONLY | O_CREAT | ((simple_cmd->io_flags & IO_ERR_APPEND) ? O_APPEND : O_TRUNC);

			// open file and redirect stderr
			file_descr = open(err_file, flags, 0644);
			free(err_file);
			if (file_descr < 0)
				exit(EXIT_FAILURE);
			dup2(file_descr, STDERR_FILENO);
			close(file_descr);
		}
		// obtain argv and execute cmd
		int argc;
		char **argv = get_argv(simple_cmd, &argc);

		execvp(cmd_name, argv);
		// if exec fails, display error and exit
		fprintf(stderr, "Execution failed for '%s'\n", cmd_name);
		for (int i = 0; i < argc; i++)
			free(argv[i]);
		free(argv);
		exit(EXIT_FAILURE);
	}
	// parent process waits for child process and returns exit status
	int sts;

	waitpid(process_id, &sts, 0);
	free(cmd_name);
	return WIFEXITED(sts) ? WEXITSTATUS(sts) : 1;
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int depth,
		command_t *parent_cmd)
{
	/* TODO: Execute cmd1 and cmd2 simultaneously. */
	// create process for first cmd
		return 1;
	// child executes cmd1
	if (pid1 == 0)
		exit(parse_command(cmd1, depth + 1, parent_cmd));

	// create process for second cmd
	pid_t pid2 = fork();
	int sts;

	if (pid2 < 0) {
		// if fork fails, wait for first process
		waitpid(pid1, &sts, 0);
		return 1;
	}
	// child executes cmd2
	if (pid2 == 0)
		exit(parse_command(cmd2, depth + 1, parent_cmd));

	// parent waits for both processes and returns status of last
	waitpid(pid1, &sts, 0);
	waitpid(pid2, &sts, 0);
	return WIFEXITED(sts) ? WEXITSTATUS(sts) : 1;
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int depth,
		command_t *parent_cmd)
{
	/* TODO: Redirect the output of cmd1 to the input of cmd2. */
	// create pipe for communication between processes
		return 1;

	// create process for cmd1 (writes to pipe)
	pid_t pid1 = fork();

	if (pid1 < 0) {
		close(pipe_descr[READ]);
		close(pipe_descr[WRITE]);
		return 1;
	}
	if (pid1 == 0) {
		// close read end and redirect stdout to write end
		close(pipe_descr[READ]);
		dup2(pipe_descr[WRITE], STDOUT_FILENO);
		close(pipe_descr[WRITE]);
		exit(parse_command(cmd1, depth + 1, parent_cmd));
	}
	// create process for cmd2 (reads from pipe)
	pid_t pid2 = fork();
	int sts;

	if (pid2 < 0) {
		close(pipe_descr[READ]);
		close(pipe_descr[WRITE]);
		waitpid(pid1, &sts, 0);
		return 1;
	}
	if (pid2 == 0) {
		// close write end and redirect stdin to read end
		close(pipe_descr[WRITE]);
		dup2(pipe_descr[READ], STDIN_FILENO);
		close(pipe_descr[READ]);
		exit(parse_command(cmd2, depth + 1, parent_cmd));
	}
	// parent closes both ends and waits for child processes
	close(pipe_descr[READ]);
	close(pipe_descr[WRITE]);
	waitpid(pid1, &sts, 0);
	waitpid(pid2, &sts, 0);
	return WIFEXITED(sts) ? WEXITSTATUS(sts) : 1;
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *crt_cmd, int depth, command_t *parent_cmd)
{
	/* TODO: sanity checks */
	// verify if cmd is valid
	if (crt_cmd == NULL)
		return 0;
	// if it has no op, it's a simple cmd
	if (crt_cmd->op == OP_NONE)
		/* TODO: Execute a simple command. */
		return parse_simple(crt_cmd->scmd, depth, parent_cmd);

	int rez1;

	// process op based on type
	switch (crt_cmd->op) {
	case OP_SEQUENTIAL:
		/* TODO: Execute the commands one after the other. */
		// execute commands one after another
		rez1 = parse_command(crt_cmd->cmd1, depth + 1, crt_cmd);
		return (rez1 == SHELL_EXIT) ? SHELL_EXIT : parse_command(crt_cmd->cmd2, depth + 1, crt_cmd);
	case OP_PARALLEL:
		/* TODO: Execute the commands simultaneously. */
		// execute commands in parallel
		return run_in_parallel(crt_cmd->cmd1, crt_cmd->cmd2, depth, crt_cmd);
	case OP_CONDITIONAL_NZERO:
		/* TODO: Execute the second command only if the first one
		 * returns non zero.
		 */
		// execute cmd2 only if cmd1 fails
		rez1 = parse_command(crt_cmd->cmd1, depth + 1, crt_cmd);
		if (rez1 == SHELL_EXIT)
			return SHELL_EXIT;
		return (rez1 != 0) ? parse_command(crt_cmd->cmd2, depth + 1, crt_cmd) : rez1;
	case OP_CONDITIONAL_ZERO:
		/* TODO: Execute the second command only if the first one
		 * returns zero.
		 */
		// execute cmd2 only if cmd1 succeeds
		rez1 = parse_command(crt_cmd->cmd1, depth + 1, crt_cmd);
		if (rez1 == SHELL_EXIT)
			return SHELL_EXIT;
		return (rez1 == 0) ? parse_command(crt_cmd->cmd2, depth + 1, crt_cmd) : rez1;
	case OP_PIPE:
		/* TODO: Redirect the output of the first command to the
		 * input of the second.
		 */
		// connect output of cmd1 to input of cmd2
		return run_on_pipe(crt_cmd->cmd1, crt_cmd->cmd2, depth, crt_cmd);
	default:
		return SHELL_EXIT;
	}
}
