#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_HISTORY 20

#define MAX_VARIABLES 100
#define MAX_VAR_NAME_LEN 50
#define MAX_VAR_VALUE_LEN 100

volatile sig_atomic_t ctrl_c_pressed = 0;
int pid;

char history[MAX_HISTORY][MAX_COMMAND_LENGTH];
int history_count = 0;
int history_index = -1;

typedef struct {
    char name[MAX_VAR_NAME_LEN];
    char value[MAX_VAR_VALUE_LEN];
} Variable;

Variable variables[MAX_VARIABLES];
int variable_count = 0;

// Define the signal handler function
void sigint_handler(int sig) {
    printf("\nYou typed Control-C!\nhello: ");
    fflush(stdout);
    // exit from the child process
    if (pid != 0) {
        kill(pid, SIGKILL);
    }
}

void free_pipefds(int **pipefds, int pipe_count) {
    for (int i = 0; i < pipe_count; i++) {
        free(pipefds[i]);
    }
    free(pipefds);
}

void set_variable(char *name, char *value) {
    for (int i = 0; i < variable_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            strncpy(variables[i].value, value, MAX_VAR_VALUE_LEN);
            return;
        }
    }
    if (variable_count < MAX_VARIABLES) {
        strncpy(variables[variable_count].name, name, MAX_VAR_NAME_LEN);
        strncpy(variables[variable_count].value, value, MAX_VAR_VALUE_LEN);
        variable_count++;
    }
}

char* get_variable(char *name) {
    for (int i = 0; i < variable_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return variables[i].value;
        }
    }
    return NULL;
}

void substitute_variables(char *command) {
    char temp_command[1024];
    strcpy(temp_command, command);
    char substituted_command[1024] = "";
    char *token = strtok(temp_command, " ");
    
    while (token != NULL) {
        if (token[0] == '$') {
            char *value = get_variable(token + 1);
            if (value != NULL) {
                strcat(substituted_command, value);
            } else {
                strcat(substituted_command, token);  // Keep the original token if variable not found
            }
        } else {
            strcat(substituted_command, token);
        }
        strcat(substituted_command, " ");
        token = strtok(NULL, " ");
    }
    
    // Remove the trailing space
    if (strlen(substituted_command) > 0) {
        substituted_command[strlen(substituted_command) - 1] = '\0';
    }
    strcpy(command, substituted_command);
}

int main() {
    // Initialize the history library
    using_history();
    char *command;
    char last_command[1024] = {0};
    char prompt[1024] = "hello: ";
    char *token;
    char *outfile;
    int i, fd, amper, redirect, retid, status, redirect_error, redirect_append;
    int exit_status = 0;
    int infinite_process = 0;
    int pipe_mode = 0;
    char *argv[10];
    int iter_counter = 0;

    // Register the signal handler for SIGINT (Ctrl+C)
    signal(SIGINT, sigint_handler);

    while (1) {
        command = readline(prompt);
        if (command == NULL) {
            break; // Exit on EOF
        }
        
        if (strlen(command) == 0) {
            free(command);
            continue; // Ignore empty commands
        }

        // Add command to history
        add_history(command);
        
        strcpy(last_command, command);

        // Handle pipes
        if (strstr(command, "|") != NULL) {
            pipe_mode = 1;
            // Count the number of pipes
            int pipe_count = 0;
            for (int i = 0; i < strlen(command); i++) {
                if (command[i] == '|') {
                    pipe_count++;
                }
            }

            // Create pipes
            int **pipefds = (int **)malloc(pipe_count * sizeof(int *));
            for (int i = 0; i < pipe_count; i++) {
                pipefds[i] = (int *)malloc(2 * sizeof(int));
                if (pipefds[i] == NULL) {
                    printf("Memory allocation failed.\n");
                    free_pipefds(pipefds, pipe_count);
                    free(command);
                    return 1;
                }
                if (pipe(pipefds[i]) == -1) {
                    perror("pipe");
                    free_pipefds(pipefds, pipe_count);
                    free(command);
                    return 1;
                }
            }

            // Allocate memory for commands
            char **commands = malloc((pipe_count + 1) * sizeof(char *));
            if (commands == NULL) {
                printf("Memory allocation failed.\n");
                free_pipefds(pipefds, pipe_count);
                free(command);
                return 1;
            }

            // Duplicate the command
            char *command_copy_str = strdup(command);
            if (command_copy_str == NULL) {
                printf("Memory allocation failed.\n");
                free(commands);
                free_pipefds(pipefds, pipe_count);
                free(command);
                return 1;
            }

            // Split the command by "|"
            for (int i = 0; i < pipe_count + 1; i++) {
                commands[i] = strsep(&command_copy_str, "|");
            }

            // Execute the commands
            for (int i = 0; i < pipe_count + 1; i++) {
                pid = fork();
                if (pid == 0) {
                    // Redirect input from previous pipe if not the first command
                    if (i > 0) {
                        dup2(pipefds[i - 1][0], 0);
                    }
                    // Redirect output to next pipe if not the last command
                    if (i < pipe_count) {
                        dup2(pipefds[i][1], 1);
                    }
                    // Close all pipe file descriptors
                    for (int j = 0; j < pipe_count; j++) {
                        close(pipefds[j][0]);
                        close(pipefds[j][1]);
                    }
                    // Tokenize the command
                    int j = 0;
                    token = strtok(commands[i], " ");
                    while (token != NULL) {
                        argv[j] = token;
                        token = strtok(NULL, " ");
                        j++;
                    }
                    argv[j] = NULL;
                    execvp(argv[0], argv);
                    perror("execvp");
                    exit(1);
                }
            }

            // Close all pipe file descriptors in the parent
            for (int i = 0; i < pipe_count; i++) {
                close(pipefds[i][0]);
                close(pipefds[i][1]);
            }

            // Wait for all child processes to finish
            for (int i = 0; i < pipe_count + 1; i++) {
                wait(&status);
            }

            free(command_copy_str);
            free(commands);
            free_pipefds(pipefds, pipe_count);
            free(command);
            pipe_mode = 0;
            continue;
        }

        // Handle history (!!)
        if (!(strcmp(command, "!!"))) {
            if (strlen(last_command) == 0) {
                printf("No commands in history.\n");
                free(command);
                continue;
            }
            strcpy(command, last_command);
        } else {
            strcpy(last_command, command);
        }

        // Handle variable assignment
        if (strchr(command, '=') != NULL && strstr(command, "echo") == NULL) {
            token = strtok(command, " =");
            char *var_name = token;
            var_name++;
            token = strtok(NULL, " =");
            char *var_value = token;
            set_variable(var_name, var_value);
            free(command);
            continue;
        }

        substitute_variables(command);

        char *command_copy = strdup(command);
        if (!(strcmp(command, "quit"))) {
            free(command);
            exit(0);
        }

        // Parse command line
        i = 0;
        token = strtok(command, " ");
        while (token != NULL) {
            argv[i] = token;
            token = strtok(NULL, " ");
            i++;
        }
        argv[i] = NULL;

        // Check if command is empty
        if (argv[0] == NULL) {
            free(command_copy);
            free(command);
            continue;
        }

        // Check if command ends with &
        if (!strcmp(argv[i - 1], "&")) {
            amper = 1;
            argv[i - 1] = NULL;
        } else {
            amper = 0;
        }

        // Handle built-in commands
        if (!(strcmp(argv[0], "cd"))) {
            if (argv[1] != NULL) {
                if (chdir(argv[1]) != 0) {
                    perror("cd");
                }
            } else {
                if (chdir(getenv("HOME")) != 0) {
                    perror("cd");
                }
            }
            free(command_copy);
            free(command);
            continue;
        } else if (i > 2 && !strcmp(argv[i - 2], ">")) {
            redirect = 1;
            redirect_error = 0;
            redirect_append = 0;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        } else if (i > 2 && !strcmp(argv[i - 2], "2>")) {
            redirect_error = 1;
            redirect = 0;
            redirect_append = 0;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        } else if (i > 2 && !strcmp(argv[i - 2], ">>")) {
            redirect_append = 1;
            redirect_error = 0;
            redirect = 0;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        } else if (i > 2 && !strcmp(argv[0], "prompt") && !strcmp(argv[1], "=")) {
            strcpy(prompt, argv[2]);
            free(command_copy);
            free(command);
            continue;
        } else if (!(strcmp(argv[0], "echo"))) {
            if (!(strcmp(argv[1], "$?"))) {
                printf("%d\n", exit_status);
                fflush(stdout);  // Ensure output is flushed
                free(command_copy);
                free(command);
                continue;
            } else {
                for (int j = 1; argv[j] != NULL; j++) {
                    if (argv[j][0] == '$') {
                        char *value = get_variable(argv[j] + 1);
                        if (value != NULL) {
                            printf("%s ", value);
                        } else {
                            printf("%s ", argv[j]);
                        }
                    } else {
                        printf("%s ", argv[j]);
                    }
                }
                printf("\n");
                fflush(stdout);
                free(command_copy);
                free(command);
                continue;
            }
        } else if (!(strcmp(argv[0], "read"))) {
                if (argv[1] != NULL) {
                    char input[1024];
                    fflush(stdout);
                    fgets(input, 1024, stdin);
                    input[strlen(input) - 1] = '\0';
                    set_variable(argv[1], input);
                } else {
                    printf("Usage: read <variable>\n");
                }
                free(command_copy);
                free(command);
                continue;
            

        } else {
            redirect = 0;
            redirect_error = 0;
            redirect_append = 0;
        }

        // Handle external commands
        pid = fork();
        if (pid == 0) {
            if (redirect) {
                fd = creat(outfile, 0660);
                close(STDOUT_FILENO);
                dup(fd);
                close(fd);
            }
            if (redirect_error) {
                fd = creat(outfile, 0660);
                close(STDERR_FILENO);
                dup(fd);
                close(fd);
            }
            if (redirect_append) {
                fd = open(outfile, O_WRONLY | O_APPEND, 0660);
                close(STDOUT_FILENO);
                dup(fd);
                close(fd);
            }
            execvp(argv[0], argv);
            perror("execvp");
            exit(1);
        }

        // Parent process
        if (amper == 0) {
            retid = wait(&status);
            if (retid != -1) {
                if (WIFEXITED(status)) {
                    exit_status = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    exit_status = 128 + WTERMSIG(status);
                }
            }
        }
        free(command_copy);
        free(command);
    }

    return 0;
}
