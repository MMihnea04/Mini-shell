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

	// daca nu avem dir,mergem la HOME
	if (dir == NULL) {
		dir_path = getenv("HOME");
		if (dir_path == NULL)
			return 1;
		return chdir(dir_path) != 0 ? 1 : 0;
	}

	// obt calea dir din word si verif daca e valida
	dir_path = get_word(dir);
	if (dir_path == NULL)
		return 1;
	// inc sa schimbam dir si verif rez
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
	// buff pt dir crt si obt calea dir de lucru
	char crt_dir[1024];

	if (getcwd(crt_dir, sizeof(crt_dir)) == NULL)
		return 1;
	// afisam dir crt
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
	// obt numele fis si set flag-uri pt open
	char *filename = get_word(file);
	int flags = O_WRONLY | O_CREAT | ((io_flag & append_bit) ? O_APPEND : O_TRUNC);
	// deschiem fis si verif daca a mers
	int file_descr = open(filename, flags, 0644);

	free(filename);
	if (file_descr < 0)
		return -1;
	// salvam fd orig pt restaurare mai tarziu
	int saved_fd = dup(target_fd);

	// redirec fd target spre fis si inchidem fis descr
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
	// verif daca e cmd cd si facem redirec daca treb
	if (strcmp(cmd_name, "cd") == 0) {
		int org_stdout = do_redirect(simple_cmd->out, simple_cmd->io_flags, IO_OUT_APPEND, STDOUT_FILENO);
		int saved_err = do_redirect(simple_cmd->err, simple_cmd->io_flags, IO_ERR_APPEND, STDERR_FILENO);
		// exec cd si salvam rez
		int rez = shell_cd(simple_cmd->params);

		// restauram stdout daca a fost redirec
		if (org_stdout >= 0) {
			dup2(org_stdout, STDOUT_FILENO);
			close(org_stdout);
		}
		// restauram stderr daca a fost redirec
		if (saved_err >= 0) {
			dup2(saved_err, STDERR_FILENO);
			close(saved_err);
		}
		free(cmd_name);
		return rez;
	}

	// verif daca e cmd pwd
	if (strcmp(cmd_name, "pwd") == 0) {
		// redirec stdout daca treb si exec pwd
		int saved = do_redirect(simple_cmd->out, simple_cmd->io_flags, IO_OUT_APPEND, STDOUT_FILENO);
		int rez = shell_pwd();

		// restauram stdout daca a fost redirec
		if (saved >= 0) {
			dup2(saved, STDOUT_FILENO);
			close(saved);
		}
		free(cmd_name);
		return rez;
	}

	// verif daca e exit sau quit si returnam cod special
	if (strcmp(cmd_name, "exit") == 0 || strcmp(cmd_name, "quit") == 0) {
		free(cmd_name);
		return shell_exit();
	}

	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */
	// verif daca e asig de var(cont =)
	if (strchr(cmd_name, '=') != NULL) {
		// sep numele var de valoare la =
		*strchr(cmd_name, '=') = '\0';
		char *var_value = strchr(cmd_name, '\0') + 1;

		// daca avem params,val var vine din params
		if (simple_cmd->params != NULL) {
			char *args_value = get_word(simple_cmd->params);

			if (args_value != NULL) {
				// set var env cu val din params
				setenv(cmd_name, args_value, 1);
				free(args_value);
			} else {
				// folosim val dupa =
				setenv(cmd_name, var_value, 1);
			}
		} else {
			// nu avem params,folosim val dupa =
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
	// creem proc copil pt exec cmd extern
	pid_t process_id = fork();

	if (process_id < 0) {
		free(cmd_name);
		return 1;
	}
	// codul din proc copil
	if (process_id == 0) {
		int file_descr;

		// redirec stdin daca treb
		if (simple_cmd->in != NULL) {
			char *inp_file = get_word(simple_cmd->in);

			// deschidem fis pt citire si redirec stdin
			file_descr = open(inp_file, O_RDONLY);
			free(inp_file);
			if (file_descr < 0)
				exit(EXIT_FAILURE);
			dup2(file_descr, STDIN_FILENO);
			close(file_descr);
		}
		// redirec stdout daca treb
		if (simple_cmd->out != NULL) {
			char *outp_file = get_word(simple_cmd->out);
			// set flag-uri pt append sau trunc
			int flags = O_WRONLY | O_CREAT | ((simple_cmd->io_flags & IO_OUT_APPEND) ? O_APPEND : O_TRUNC);

			// deschidem fis si redirec stdout
			file_descr = open(outp_file, flags, 0644);
			if (file_descr < 0) {
				free(outp_file);
				exit(EXIT_FAILURE);
			}
			dup2(file_descr, STDOUT_FILENO);
			// verif daca stderr merge la acelasi fis
			if (simple_cmd->err != NULL) {
				char *err_file = get_word(simple_cmd->err);

				if (strcmp(outp_file, err_file) == 0)
					dup2(file_descr, STDERR_FILENO);
				free(err_file);
			}
			close(file_descr);
			free(outp_file);
		}
		// redirec stderr daca treb si nu e deja redirec la acelasi fis ca stdout
		if (simple_cmd->err != NULL && (simple_cmd->out == NULL ||
				strcmp(get_word(simple_cmd->out), get_word(simple_cmd->err)) != 0)) {
			char *err_file = get_word(simple_cmd->err);
			int flags = O_WRONLY | O_CREAT | ((simple_cmd->io_flags & IO_ERR_APPEND) ? O_APPEND : O_TRUNC);

			// deschidem fis si redirec stderr
			file_descr = open(err_file, flags, 0644);
			free(err_file);
			if (file_descr < 0)
				exit(EXIT_FAILURE);
			dup2(file_descr, STDERR_FILENO);
			close(file_descr);
		}
		// obt argv si exec cmd
		int argc;
		char **argv = get_argv(simple_cmd, &argc);

		execvp(cmd_name, argv);
		// daca exec esueaza,afisam err si exit
		fprintf(stderr, "Execution failed for '%s'\n", cmd_name);
		for (int i = 0; i < argc; i++)
			free(argv[i]);
		free(argv);
		exit(EXIT_FAILURE);
	}
	// proc parinte ast proc copil si ret sts exit
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
	// creem proc pt primul cmd
	pid_t pid1 = fork();

	if (pid1 < 0)
		return 1;
	// copilul exec cmd1
	if (pid1 == 0)
		exit(parse_command(cmd1, depth + 1, parent_cmd));

	// creem proc pt al 2-lea cmd
	pid_t pid2 = fork();
	int sts;

	if (pid2 < 0) {
		// daca fork esueaza,ast primul proc
		waitpid(pid1, &sts, 0);
		return 1;
	}
	// copilul exec cmd2
	if (pid2 == 0)
		exit(parse_command(cmd2, depth + 1, parent_cmd));

	// parintele ast ambele proc si ret sts ultimului
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
	// cream pipe pt comun intre proc
	int pipe_descr[2];

	if (pipe(pipe_descr) < 0)
		return 1;

	// creem proc pt cmd1(scrie in pipe)
	pid_t pid1 = fork();

	if (pid1 < 0) {
		close(pipe_descr[READ]);
		close(pipe_descr[WRITE]);
		return 1;
	}
	if (pid1 == 0) {
		// inchidem cap de citire si redirec stdout la cap de scriere
		close(pipe_descr[READ]);
		dup2(pipe_descr[WRITE], STDOUT_FILENO);
		close(pipe_descr[WRITE]);
		exit(parse_command(cmd1, depth + 1, parent_cmd));
	}
	// creem proc pt cmd2(citeste din pipe)
	pid_t pid2 = fork();
	int sts;

	if (pid2 < 0) {
		close(pipe_descr[READ]);
		close(pipe_descr[WRITE]);
		waitpid(pid1, &sts, 0);
		return 1;
	}
	if (pid2 == 0) {
		// inchidem cap de scriere si reirec stdin la cap de citire
		close(pipe_descr[WRITE]);
		dup2(pipe_descr[READ], STDIN_FILENO);
		close(pipe_descr[READ]);
		exit(parse_command(cmd2, depth + 1, parent_cmd));
	}
	// parintele inchide ambele cap si ast proc copii
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
	// verif daca cmd e valid
	if (crt_cmd == NULL)
		return 0;
	// daca nu are op,e cmd simplu
	if (crt_cmd->op == OP_NONE)
		/* TODO: Execute a simple command. */
		return parse_simple(crt_cmd->scmd, depth, parent_cmd);

	int rez1;

	// proc op in functie de tip
	switch (crt_cmd->op) {
	case OP_SEQUENTIAL:
		/* TODO: Execute the commands one after the other. */
		// exec cmd-uri unul dupa altul
		rez1 = parse_command(crt_cmd->cmd1, depth + 1, crt_cmd);
		return (rez1 == SHELL_EXIT) ? SHELL_EXIT : parse_command(crt_cmd->cmd2, depth + 1, crt_cmd);
	case OP_PARALLEL:
		/* TODO: Execute the commands simultaneously. */
		// exec cmd-uri in paralel
		return run_in_parallel(crt_cmd->cmd1, crt_cmd->cmd2, depth, crt_cmd);
	case OP_CONDITIONAL_NZERO:
		/* TODO: Execute the second command only if the first one
		 * returns non zero.
		 */
		// exec cmd2 doar daca cmd1 eseaza
		rez1 = parse_command(crt_cmd->cmd1, depth + 1, crt_cmd);
		if (rez1 == SHELL_EXIT)
			return SHELL_EXIT;
		return (rez1 != 0) ? parse_command(crt_cmd->cmd2, depth + 1, crt_cmd) : rez1;
	case OP_CONDITIONAL_ZERO:
		/* TODO: Execute the second command only if the first one
		 * returns zero.
		 */
		// exec cmd2 doar daca cmd1 reuseste
		rez1 = parse_command(crt_cmd->cmd1, depth + 1, crt_cmd);
		if (rez1 == SHELL_EXIT)
			return SHELL_EXIT;
		return (rez1 == 0) ? parse_command(crt_cmd->cmd2, depth + 1, crt_cmd) : rez1;
	case OP_PIPE:
		/* TODO: Redirect the output of the first command to the
		 * input of the second.
		 */
		// conectam output cmd1 la input cmd2
		return run_on_pipe(crt_cmd->cmd1, crt_cmd->cmd2, depth, crt_cmd);
	default:
		return SHELL_EXIT;
	}
}
