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

void sigint_handler(int sig) {
    printf("\nYou typed Control-C!\nhello: ");
    fflush(stdout);
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
                strcat(substituted_command, token);
            }
        } else {
            strcat(substituted_command, token);
        }
        strcat(substituted_command, " ");
        token = strtok(NULL, " ");
    }
    
    if (strlen(substituted_command) > 0) {
        substituted_command[strlen(substituted_command) - 1] = '\0';
    }
    strcpy(command, substituted_command);
}

int execute_command(char *command) {
    int status;
    pid = fork();
    if (pid == 0) {
        char *argv[10];
        int i = 0;
        char *token = strtok(command, " ");
        while (token != NULL) {
            argv[i++] = token;
            token = strtok(NULL, " ");
        }
        argv[i] = NULL;
        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    } else {
        wait(&status);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
}

int main() {
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

    signal(SIGINT, sigint_handler);

    while (1) {
        command = readline(prompt);
        if (command == NULL) {
            break;
        }
        
        if (strlen(command) == 0) {
            free(command);
            continue;
        }

        add_history(command);
        
        strcpy(last_command, command);

        // if (strncmp(command, "if", 2) == 0) {
        if (strncmp(command, "if ", 3) == 0){
            printf("enter to if condition\n");
            int inside_else = 0, inside_else_if = 0;
            char condition[MAX_COMMAND_LENGTH];
            char then_command[MAX_COMMAND_LENGTH];
            char else_command[MAX_COMMAND_LENGTH];
            char else_if_condition[MAX_COMMAND_LENGTH];
            char else_if_command[MAX_COMMAND_LENGTH];

            sscanf(command, "if %[^\n]s", condition);
            printf("> : ");
            fgets(command, MAX_COMMAND_LENGTH, stdin);

            if (strncmp(command, "then\n", 5) != 0) {
                printf("Syntax error: expected \"then\" command\n");
                free(command);
                continue;
            }
            fgets(then_command, MAX_COMMAND_LENGTH, stdin);
            then_command[strcspn(then_command, "\n")] = 0;
            printf("> : ");
            fgets(command, MAX_COMMAND_LENGTH, stdin);

            if (strncmp(command, "else if ", 8) == 0) {
                inside_else_if = 1;
                sscanf(command, "else if %[^\n]s", else_if_condition);
                printf("> : ");
                fgets(command, MAX_COMMAND_LENGTH, stdin);
                if (strncmp(command, "then\n", 5) != 0) {
                    printf("Syntax error: expected \"then\" command for \"else if\"\n");
                    free(command);
                    continue;
                }
                fgets(else_if_command, MAX_COMMAND_LENGTH, stdin);
                else_if_command[strcspn(else_if_command, "\n")] = 0;
                printf("> : ");
                fgets(command, MAX_COMMAND_LENGTH, stdin);
            }

            if (strncmp(command, "else\n", 5) == 0) {
                fgets(else_command, MAX_COMMAND_LENGTH, stdin);
                else_command[strcspn(else_command, "\n")] = 0;
                inside_else = 1;
                printf("> : ");
                fgets(command, MAX_COMMAND_LENGTH, stdin);
            }

            if (strncmp(command, "fi\n", 3) != 0) {
                printf("Syntax error: expected \"fi\" at the end\n");
                free(command);
                continue;
            }

            condition[strcspn(condition, "\n")] = 0;

            if (execute_command(condition)) {
                execute_command(then_command);
            } else if (inside_else_if && execute_command(else_if_condition)) {
                execute_command(else_if_command);
            } else if (inside_else) {
                execute_command(else_command);
            }

            free(command);
            continue;
        }

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

        i = 0;
        token = strtok(command, " ");
        while (token != NULL) {
            argv[i] = token;
            token = strtok(NULL, " ");
            i++;
        }
        argv[i] = NULL;

        if (argv[0] == NULL) {
            free(command_copy);
            free(command);
            continue;
        }

        if (!strcmp(argv[i - 1], "&")) {
            amper = 1;
            argv[i - 1] = NULL;
        } else {
            amper = 0;
        }

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
            redirect = 0;
            redirect_error = 0;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        } else {
            redirect = 0;
            redirect_error = 0;
            redirect_append = 0;
        }

        pipe_mode = 0;
        for (int j = 0; j < i; j++) {
            if (strcmp(argv[j], "|") == 0) {
                pipe_mode = 1;
                break;
            }
        }

        if (pipe_mode) {
            int num_cmds = 0;
            char *cmds[10];
            char *cmd_token = strtok(command_copy, "|");
            while (cmd_token != NULL) {
                cmds[num_cmds++] = cmd_token;
                cmd_token = strtok(NULL, "|");
            }
            
            int **pipefds = malloc((num_cmds - 1) * sizeof(int *));
            for (int i = 0; i < num_cmds - 1; i++) {
                pipefds[i] = malloc(2 * sizeof(int));
                pipe(pipefds[i]);
            }
            
            for (int i = 0; i < num_cmds; i++) {
                pid = fork();
                if (pid == 0) {
                    if (i > 0) {
                        dup2(pipefds[i - 1][0], 0);
                    }
                    if (i < num_cmds - 1) {
                        dup2(pipefds[i][1], 1);
                    }
                    for (int j = 0; j < num_cmds - 1; j++) {
                        close(pipefds[j][0]);
                        close(pipefds[j][1]);
                    }
                    char *cmd_argv[10];
                    int cmd_argc = 0;
                    char *cmd_token = strtok(cmds[i], " ");
                    while (cmd_token != NULL) {
                        cmd_argv[cmd_argc++] = cmd_token;
                        cmd_token = strtok(NULL, " ");
                    }
                    cmd_argv[cmd_argc] = NULL;
                    execvp(cmd_argv[0], cmd_argv);
                    perror("execvp");
                    exit(1);
                }
            }
            for (int i = 0; i < num_cmds - 1; i++) {
                close(pipefds[i][0]);
                close(pipefds[i][1]);
            }
            for (int i = 0; i < num_cmds; i++) {
                wait(NULL);
            }
            free_pipefds(pipefds, num_cmds - 1);
        } else {
            if (execute_command(command_copy) != 0) {
                exit_status = 0;
            } else {
                exit_status = 1;
            }
        }
        free(command_copy);
        free(command);
    }
    return 0;
}
