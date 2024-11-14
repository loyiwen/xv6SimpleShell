#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Read a line of characters from stdin.
int getcmd(char *buf, int nbuf) {
	printf(">>> "); // Display prompt
	memset(buf, 0, nbuf);
	gets(buf, nbuf);

	if (buf[0] == 0) { // EOF
		return -1;
	}

	return 0;
}

// A recursive function which parses the command at *buf and executes it.
__attribute__((noreturn)) void run_command(char *buf, int nbuf, int *pcp) {
	// Useful data structures and flags.
	char *arguments[10];
	int numargs = 0;
	int ws = 1; // Word start flag
	int we = 0; // Word end flag

	int redirection_flag = 0;
	int redirection_left = 0, redirection_right = 0;
	char *file_name_l = 0, *file_name_r = 0;

	int p[2];
	int pipe_cmd = 0;
	int sequence_cmd = 0;

	// Parse the command character by character.
	int i = 0;
	for (; i < nbuf; i++) {
		// Parse the current character and set-up various flags: sequence_cmd, redirection, pipe_cmd and similar.
		// ##### Place your code here.

		// Handle spaces, newlines, tabs, and end of string
		if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' || buf[i] == '\0') {
			if (we)	{
				buf[i] = '\0'; // Terminate the current word
				we = 0; // Set flag to indicate that buf[i] is no longer inside a word
			}
			ws = 1; // The next non-space character is the start of a new word
			continue;
		}


		// Detect pipe command '|'
		if (buf[i] == '|') {
			pipe_cmd = 1;
			buf[i] = '\0';
			i++;
			break;
		}


		// Detect sequence command ';'
		if (buf[i] == ';') {
			sequence_cmd = 1;
			buf[i] = '\0';
			i++;
			break;
		}


		// Detect redirection commands '<' and '>'
		if (buf[i] == '<' && !redirection_flag)	{
			buf[i] = '\0'; // Terminate previous argument
			redirection_left = 1;
			redirection_flag = 1;
			i++;
		} else if (buf[i] == '>' && !redirection_flag) {
			buf[i] = '\0'; // Terminate previous argument
			redirection_right = 1;
			redirection_flag = 1;
			i++;
		}

		if (!(redirection_left || redirection_right)) {
			// No redirection, continue parsing command.
			// #### Place your code here.
		} else {
			/* Redirection command. Capture the file names. */
			// ##### Place your code here.
			while (buf[i] == ' ' || buf[i] == '\t') i++; // Skip spaces after '<' or '>'

			if (redirection_left) {
				file_name_l = &buf[i];
			} else {
				file_name_r = &buf[i];
			}
			while (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\n' && buf[i] != '\0') i++;
    		buf[i] = '\0'; // Terminate the filename
    		continue;
		}


		// Detect start of new word
		if (ws) {
			arguments[numargs++] = &buf[i]; // Store start of new word
			ws = 0;
			we = 1;
		}
	}
	arguments[numargs] = 0; // Terminate the argument list
	redirection_flag = 0;


	// Sequence command. Continue this command in a new process. Wait for it to complete and execute the command following ';'.
	if (sequence_cmd) {		
		sequence_cmd = 0;
		
		// Fork a child process to execute the current command
		if (fork() != 0) {
			wait(0);
			// ##### Place your code here.
			// Recursively execute the remaining commands after ';'
			// printf("[DEBUG] Executing next command after ';'\n");

			// Recursively handle the next command after the ';'
			// printf("[DEBUG] Handling next command after ';'\n");

			// Skip any extra spaces after the ';' character
			while (buf[i] == ' ' || buf[i] == '\t') {
				i++;
			}
			if (buf[i] != '\0') {
				run_command(&buf[i], nbuf - i, pcp);
			}
			exit(0);
		} else {
			exec(arguments[0], arguments);
			fprintf(2, "exec %s failed\n", arguments[0]);
			exit(1);
		}
	}


	// If this is a redirection command, tie the specified files to std in/out.
	if (redirection_left) {
		// ##### Place your code here.
		if (file_name_l) {
			int fd_in = open(file_name_l, O_RDONLY);
			if (fd_in < 0) {
				fprintf(2, "Error: cannot open file %s for reading\n", file_name_l);
				exit(1);
			}

			close(0); // Close stdin
			if (dup(fd_in) != 0) {
				fprintf(2, "Error: duplication failed for input redirection\n");
				close(fd_in);
				exit(1);
			}
			close(fd_in);
		} else {
			fprintf(2, "Error: missing filename after '<'\n");
        	exit(1);
		}
	}
	
	if (redirection_right) {
		// ##### Place your code here.
		if (file_name_r) {
			int fd_out = open(file_name_r, O_WRONLY | O_CREATE | O_TRUNC);
			if (fd_out < 0) {
				fprintf(2, "Error: cannot open file %s for writing\n", file_name_r);
				exit(1);
			}

			close(1); // Close stdout
			if (dup(fd_out) != 1) {
				fprintf(2, "Error: duplication failed for output redirection\n");
				close(fd_out);
				exit(1);
			}
			close(fd_out);
		} else {
			fprintf(2, "Error: missing filename after '<'\n");
        	exit(1);
		}
	}


	// Parsing done. Execute the command.


	// If this command is 'cd', write the arguments to the pcp pipe and exit with '2' to tell the parent process about this.
	if (strcmp(arguments[0], "cd") == 0) {
		// ##### Place your code here.
		if (numargs < 2) {
			printf("cd: missing argument\n");
		} else if (chdir(arguments[1]) < 0) {
			printf("cd: cannot change directory to %s\n", arguments[1]);
		}
		write(pcp[1], arguments[1], strlen(arguments[1]) + 1); // Notify the parent process
		exit(2); // Exit with status 2 to indicate a cd command
	} else {
		// Pipe command: fork twice. Execute the left hand side directly. Call run_command recursion for the right side of the pipe.
		if (pipe_cmd) {
			// Create a pipe
			if (pipe(p) < 0) {
				fprintf(2, "pipe failed\n");
				exit(1);
			}

			printf("[DEBUG] Pipe detected, splitting commands...\n");

			// Fork the first child to execute left-hand side command
			int left_pid = fork();
			if (left_pid < 0) {
				fprintf(2, "fork for left command failed\n");
				exit(1);
			}

			if (left_pid == 0) { // First child process
				// printf("[DEBUG] Executing left-hand side command: %s\n", arguments[0]);

				// Redirect stdout to the write end of the pipe
				close(1);			// Close current stdout
				dup(p[1]); 			// Duplicate write end of the pipe to stdout
				close(p[0]); 		// Close unused read end
				close(p[1]); 		// Close write end after duplication
				exec(arguments[0], arguments); // Execute left-hand side command
				fprintf(2, "exec %s failed\n", arguments[0]);
				exit(1);
			}

			// Parent process: close the write end after forking the first child
			close(p[1]);

			// Fork the second child to execute right-hand side command
			int right_pid = fork();
			if (right_pid < 0) {
				fprintf(2, "fork for right command failed\n");
				exit(1);
			}

			if (right_pid == 0) {
				// printf("[DEBUG] Executing right command: %s\n", &buf[i + 1]);

				// Redirect stdin to the read end of the pipe
				close(0);			// Close current stdin
				dup(p[0]); 			// Duplicate the read end of the pipe to stdin
				close(p[0]); 		// Close read end after duplication
				run_command(&buf[i + 1], nbuf - (i + 1), pcp);
				// fprintf(2, "[DEBUG] Failed to execute right-hand side command\n");
    			exit(0);
			}

			// Parent process: close read end of pipe
			close(p[0]);

			// Wait for both child processes to complete
			int status;
			wait(&status); // Wait for the first child (left command)
			wait(&status); // Wait for the second child (right command)
			
			// printf("[DEBUG] All commands completed, returning to prompt...\n");
			exit(0);
		} else {
			// ##### Place your code here.
			// Handle regular commands without pipes
			int main_pid = fork();
			if (main_pid < 0)	{
				fprintf(2, "fork failed\n");
				exit(1);
			}

			if (main_pid == 0) { // Child process
				exec(arguments[0], arguments);
				fprintf(2, "exec %s failed\n", arguments[0]);
				exit(1);
			} else { // Parent process
				wait(0); // Wait for the child process to finish
			}
		}
	}

	// If no command entered, return
	if (numargs == 0) {
		exit(0);
	}
	exit(0);
}


int main(void) {
	static char buf[100];
	int pcp[2];
	pipe(pcp);

	// Read and run input commands.
	while (getcmd(buf, sizeof(buf)) >= 0) {
		if (fork() == 0) {
			run_command(buf, 100, pcp);
		}
		// Check if run_command found this is a 'cd' command and run it if required.
		int child_status;
		// ##### Place your code here
		if (wait(&child_status) >= 0) {
			if (child_status == 2) { // If exit status is 2, it's a 'cd' command
                char new_dir[100];
                read(pcp[0], new_dir, sizeof(new_dir)); // Read the directory from the pipe
                if (chdir(new_dir) < 0) {
                    printf("cd: cannot change directory to %s\n", new_dir);
                }
            }
		}
	}
	exit(0);
}
