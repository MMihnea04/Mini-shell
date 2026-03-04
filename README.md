Mini-Shell

This project implements a simplified Unix shell in C. It is designed as an educational exercise focused on understanding process creation, inter-process communication, and implementing a command-line interface. The shell executes built-in commands, external programs, and supports piping and redirection.

The emphasis of the project is correctness, simplicity, and faithful behavior according to POSIX shell semantics rather than performance optimization.

Overview

A shell provides a command-line interface for interacting with the operating system. Users can enter commands, execute programs, and manage processes.

In this project, the mini-shell reads user input, parses it into commands and arguments, and executes them in child processes. The shell supports multiple commands separated by pipes, input/output redirection, background execution, and signal handling.

The shell also implements basic built-in commands such as cd, exit, and pwd.

Architecture

The shell follows a read-eval-execute loop:

Read: The shell reads a line of input from the user or a script file

Parse: The input is parsed into commands, arguments, and control operators (pipes, redirection, background)

Execute: Built-in commands are executed in the shell process. External commands are executed in child processes using fork and exec.

Wait: The shell waits for foreground processes to finish, while background processes run asynchronously

Signal handling ensures that Ctrl-C interrupts foreground commands but does not terminate the shell itself.

Command Parsing

The shell parses user input into a structured format:

Splits commands by spaces while respecting quoted strings

Identifies pipes (|) and sets up inter-process communication using pipes

Detects input (<) and output (>, >>) redirection

Handles background execution with the & operator

The parser produces a command table representing the sequence of commands, arguments, redirections, and execution mode.

Execution

Built-in Commands: cd, pwd, exit are executed directly in the shell process

External Commands: Spawned using fork() and executed with execvp()

Pipes: The shell creates pipes between processes to allow output of one command to become input of the next

Redirection: Standard input/output can be redirected to files

Background Execution: Commands followed by & are executed without blocking the shell

The shell ensures proper cleanup of file descriptors and child processes to avoid resource leaks.

Signal Handling

The shell implements signal handling to support:

SIGINT (Ctrl-C) to terminate foreground processes without exiting the shell

SIGCHLD to reap background child processes and prevent zombie processes

Signals are handled in a way that does not disrupt the main read-eval-execute loop.

Features

Execution of built-in and external commands

Command piping (|)

Input and output redirection (<, >, >>)

Background execution with &

Signal handling for Ctrl-C and child process termination

Command prompt and error handling

Building and Running

The shell can be built using make from the src directory:

cd src
make

This produces the mini-shell binary.

Run the shell with:

./mini-shell

The shell will display a prompt and accept user commands. To exit, use the exit command or press Ctrl-D.

Testing and Validation

Automated and manual tests are used to validate:

Built-in commands behavior (cd, pwd, exit)

Execution of external programs

Piping and redirection functionality

Background execution

Signal handling and process cleanup

Test scripts compare the output of the mini-shell against a reference shell to ensure correctness.

Limitations

The shell does not implement job control commands like fg or bg

Advanced shell features such as variable expansion, command substitution, or scripting are not supported

The shell is designed for educational purposes and experimentation with process management concepts
