#include "shell.h"

// Signal handler for SIGINT
void sigint_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nYou typed Control-C!\n");
        // Re-display the prompt
        printf("%s", current_prompt);
        fflush(stdout);
    }
}

/* ========== INPUT PREPROCESSING FUNCTIONS ============ */

// Function to trim leading and trailing whitespace from a string
char* trim_whitespace(char *str) {
    // Trim leading whitespace
    while (*str == ' ' || *str == '\t' || *str == '\n')
        str++;

    // Trim trailing whitespace
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n'))
        end--;

    // Null-terminate the trimmed string
    *(end + 1) = '\0';

    return str;
}

// Function for reading a line from the user, returns the line
char *read_line(char *prompt) {

    char *input = readline(prompt);
    
    // Trim leading and trailing whitespace
    char *line = trim_whitespace(input);

    if (strlen(line) == 0) { // Handle empty input
        return NULL;
    }

    // Handle "!!" to get the last command
    if (strcmp(line, "!!") == 0) {
        free(line); // Free the allocated memory for "!!"
        line = get_last_command();
        if (line == NULL) {
            printf("No commands in history.\n");
            return NULL;
        }
        printf("%s\n", line); // Print the retrieved command
    }

    // Handle "quit" to exit the shell
    if (strcmp(line, "quit") == 0) {
        free(line); // Free the allocated memory for "quit"
        printf("quit is received, Exiting shell...\n");
        if (current_prompt) {
            free(current_prompt); // Free the old prompt memory
        }
        exit(0);
    }

     // Handle "prompt = newprompt" to change the prompt
    if (strncmp(line, "prompt = ", 9) == 0) {
        char *new_prompt = line + 9;
        set_prompt(new_prompt);
        // If the line is not empty, add it to the history
        if (line && *line) {
            add_history(line);
        }
        free(line); // Free the allocated memory for the old line
        return NULL; // Continue the loop without further processing
    }

    // If the line is not empty, add it to the history
    if (line && *line) {
        add_history(line);
    }

    return line;
}

// Function to set the readline prompt
void set_prompt(const char *new_prompt) {
    if (current_prompt) {
        free(current_prompt); // Free the old prompt memory
    }
    current_prompt = strdup(new_prompt); // Update the global prompt variable
    
    // Calculate the length of the new prompt
    size_t len = strlen(current_prompt);
    
    // Reallocate memory for the new prompt plus one extra character for the space
    current_prompt = (char *)realloc(current_prompt, (len + 2) * sizeof(char));

    // Check if reallocation was successful
    if (current_prompt == NULL) {
        perror("realloc failed in set_prompt\n");
        return;
    }

    // Add a space at the end of the new prompt
    current_prompt[len] = ' ';
    current_prompt[len + 1] = '\0'; // Null-terminate the string

    rl_set_prompt(current_prompt); // Set the new prompt
    rl_redisplay(); // Refresh the display
}

// Help Function for read_variable func, returns the line with no prompt
char *read_line_no_prompt(const char *history_file) {
    char *line = readline("");
    if (line == NULL) {
        return NULL;
    }

    // If the line is not empty, add it to the history
    if (line && *line) {
        add_history(line);
        // Save the history to the file
        write_history(history_file);
    }

    return line;
}

/*
 * split_line - split a string into multiple strings
 * @line: string to be splited
 * Return: pointer that points to the new array
 */

char **split_line(char *line, char *tok_delim, int *argc){
    int bufsize = 64;
    int i = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token;

    if (!tokens)
    {
        fprintf(stderr, "allocation error in split_line: tokens\n");
        exit(EXIT_FAILURE);
    }
    token = strtok(line, tok_delim);
    while (token != NULL)
    {
        /* handle comments */
        if (token[0] == '#')
        {
            break;
        }
        tokens[i] = token;
        i++;
        if (i >= bufsize)
        {
            bufsize += bufsize;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens)
            {
                fprintf(stderr, "reallocation error in split_line: tokens");
                exit(EXIT_FAILURE);
            }
  }
  token = strtok(NULL, tok_delim);
 }
 tokens[i] = NULL;
    *argc = i;
   
 return (tokens);
}

// Function to retrieve the last command from history
char *get_last_command() {
    HIST_ENTRY *last_entry = previous_history();
    if (last_entry == NULL) {
        return NULL;
    }
    return strdup(last_entry->line);
}

/* ====================================================== */
/* ============= COMMAND HANDLING FUNCTIONS ============ */

int launch(char ***commands, int num_of_pipes, int amper, Variable *variables)
{
    int return_status;
    int num_commands = num_of_pipes + 1;
    int **pids = get_pid_array(num_of_pipes); // pid[i][0] = pid, pid[i][1] = wpid, pid[i][2] = status
    int **pipes = create_pipes(num_of_pipes);
    
    for(int i = 0; i < num_commands; i++){
        pids[i][0] = fork();
        if (pids[i][0] < 0) { // create function to handle errors
            // Error forking
            perror("error in forking");
        } 
        else if (pids[i][0] == 0) {
            set_fds(i, num_of_pipes, pipes);
            signal(SIGINT, SIG_DFL);
            // Count number of arguments in commands[i]
            int argc = 0;
            while(commands[i][argc] != NULL){
                argc++;
            }
            Case current_case = get_case(commands[i], &argc, &amper);
            int status = configure_command(commands[i], &argc, current_case, variables);
            if(status != 0){
                exit(status);
            }
            
            if (execvp(commands[i][0], commands[i]) == -1) {
                perror("error in execvp");
            }
            exit(EXIT_FAILURE);
        } else {
            continue;
        }
    }
    close_fds(num_of_pipes, pipes);

            // Parent process
            if (amper == 0){
                    // wait for all child processes
                    for(int i = 0; i < num_commands; i++){ 
                        do {
                            pids[i][1] = waitpid(pids[i][0], &pids[i][2], WUNTRACED);
                        }   while (!WIFEXITED(pids[i][2]) && !WIFSIGNALED(pids[i][2]));
                }
            }

        // check how does it work if amper == 1
    return_status = pids[num_commands - 1][2]; // return the status of the last command
    free_pid_array(pids, num_of_pipes);
    free_pipes(pipes, num_of_pipes);
    
    return return_status; 
}

int execute(char *command, Variable *variables){

    int curr_status;
    int num_of_pipes = 0;
    int amper = 0;
    char **argv;
    int argc;
    argv = split_line(command, " ", &argc);
    Case command_case = get_case(argv, &argc, &amper);

    if (argv[0] == NULL) {
        // An empty command was entered.
        return 1;
    }
    if(command_case == VAR_SUB){
        return set_variable(variables, argv[0] + 1, argv[2]);
    }
    if(command_case == READ){
        return read_variable(variables, argv[1]);
    }
    if (command_case == CD) {
        return change_dir(argv, argc);
    }
    if(command_case == PIPE){
        num_of_pipes = count_pipes(argv, argc);
    }
    char ***commands = split_commands(argv, num_of_pipes, argc);
    curr_status = launch(commands, num_of_pipes, amper, variables);
    free_commands(commands, num_of_pipes + 1);
    return curr_status;
}

int configure_command(char **argv, int *argc, Case current_case, Variable *variables){
    int status = 0;
    switch (current_case) {
        case DEFAULT:
            break;
        case REDIRECT:
            redirect(argv, *argc);
            break;
        case REDIRECT_ERROR:
            redirect_error(argv, *argc);
            break;
        case REDIRECT_APPEND:
            redirect_append(argv, *argc);
            break;
        case PIPE:
            break;
        case ECHO:
            break;
        case ECHODOLLAR:
            echo_dollar(argv, argc);
            break;
        case CD:
            break;
        case VAR_SUB:
            break;
        default:
            break;
    }
    return status;
}
// Function to handle with variabels of redirection case the redirection
void redirect(char *argv[], int argc) {
     int fd;
    
    if (argc < 3) {
        fprintf(stderr, "Invalid command syntax\n");
        exit(1);
    }

    if(!strcmp(argv[argc - 2], ">")){
        char *outfile;
        argv[argc - 2] = NULL;
        outfile = argv[argc - 1];
        fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            perror("open");
            exit(1);
        }
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }
        if (close(fd) == -1) {
            perror("close");
            exit(1);
        }
    }
    else if(!strcmp(argv[argc - 2], "<")){
        char *infile = argv[argc - 1];
        argv[argc - 2] = NULL; // Terminate the arguments list before the '<'
        
        
        fd = open(infile, O_RDONLY);
        if (fd == -1) {
            perror("open");
            exit(1);
        }
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }
        if (close(fd) == -1) {
            perror("close");
            exit(1);
        }
    }
     else {
        printf("enter to else\n"); // debug
        fprintf(stderr, "Invalid redirection operator\n");
        exit(1);
    }
}


void redirect_error(char *argv[], int argc) {
    // printf("enter to redirect_error\n"); // debug
    char *outfile;
    int fd;
    argv[argc - 2] = NULL;
    outfile = argv[argc - 1];
    // *fd_out = argv[argc - 1];
    fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    // *fd_in = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    // if (*fd_in == -1) {
    if (fd == -1) {
        perror("open");
        exit(1);
    }
    // if (dup2(*fd_in, STDOUT_FILENO) == -1) {
    if (dup2(fd, STDERR_FILENO) == -1) {
        perror("dup2");
        exit(1);
    }
    // if (close(*fd_in) == -1) {
    if (close(fd) == -1) {
        perror("close");
        exit(1);
    }
}

void redirect_append(char *argv[], int argc) {
    char *outfile;
    int fd;
    argv[argc - 2] = NULL;
    outfile = argv[argc - 1];
    // *fd_out = argv[argc - 1];
    fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
    // *fd_in = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    // if (*fd_in == -1) {
    if (fd == -1) {
        perror("open");
        exit(1);
    }
    // if (dup2(*fd_in, STDOUT_FILENO) == -1) {
    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("dup2");
        exit(1);
    }
    // if (close(*fd_in) == -1) {
    if (close(fd) == -1) {
        perror("close");
        exit(1);
    }
}


int echo_dollar(char **argv, int *argc) {
    printf("%d\n", prev_status); // debug
    exit(0);
    // return prev_status;
}

int change_dir(char **argv, int argc) {
    // printf("enter to change_dir\n"); // debug
    if (argc == 1) {
        // Change to home directory
        if (chdir(getenv("HOME")) == 0) {
            return 0;
        } else {
            perror("chdir");
            return 1;
        }
    } else {
        if (chdir(argv[1]) == 0) {
            return 0;
        } else {
            perror("chdir");
            return 1;
        }
    }
}
/* ==================================================== */ 





/* ================= CASE CHECKS ==================== */

Case get_case(char *argv[], int *argc, int *amper) {
    if (!strcmp(argv[*argc - 1], "&")) {
        argv[*argc - 1] = NULL;
        *argc = *argc - 1;
        *amper = 1;
    } if (check_redirect(argv, *argc)) {
        return REDIRECT;
    } else if (*argc > 1 && !strcmp(argv[*argc - 2], "2>")) {
        return REDIRECT_ERROR;
    } else if (*argc > 1 && !strcmp(argv[*argc - 2], ">>")) {
        return REDIRECT_APPEND;
    } else if (*argc > 1 && !strcmp(argv[0], "echo") && !strcmp(argv[1], "$?")) {
        return ECHODOLLAR;
    } else if (*argc > 1 && !strcmp(argv[0], "echo") && strcmp(argv[1], "$?")) {
        return ECHO;
    } else if (*argc >= 1 && !strcmp(argv[0], "cd")) {
        return CD;    
    } else if (check_pipe(argv, *argc)) {
        return PIPE;
    } else if (check_var_sub(argv, *argc)) {
        return VAR_SUB;
    } else if(check_read(argv, *argc)){
        return READ;
    } else {
        return DEFAULT;
    } 
}

int check_pipe(char *argv[], int argc) {
    // printf("enter to check_pipe\n"); // debug
    int i;
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "|") == 0) {
            // printf("pipe found at index: %d\n", i); // debug
            return 1;
        }
    }
    // printf("no pipe found\n"); // debug
    return 0;
}

int check_var_sub(char *argv[], int argc) {
    // printf("enter to check_var_sub\n"); // debug
    // printf("argc in var_sub: %d\n", argc); // debug
    if (argc < 3) {
        return 0; // Insufficient arguments
    }

    if (argv[0][0] == '$' && strcmp(argv[1], "=") == 0 && argv[2] != NULL) {
        // printf("Variable substitution syntax detected\n"); // debug
        return 1; // Variable substitution syntax detected
    }

    return 0; // Not variable substitution syntax
}

int check_read(char *argv[], int argc) {
    if (argc < 2) {
        return 0; // Insufficient arguments
    }

    if ((!strcmp(argv[0], "read")) && (argv[1] != NULL)) {
        printf("Variable substitution READ syntax detected\n"); // debug
        return 1; // Variable substitution syntax detected
    }

    return 0; // Not variable substitution syntax
}

int check_redirect(char *argv[], int argc){
    if((argc > 1 && !strcmp(argv[argc - 2], ">")) || (argc > 1 && !strcmp(argv[argc - 2], "<"))) {
        return 1;
    } else {
        return 0;
    }
}
/* ================================================== */



/* ============= PIPE FUNCTIONS ==================== */

int count_pipes(char **argv, int argc){
    int i;
    int num_of_pipes = 0;
    for(i = 0; i < argc; i++){
        if(!strcmp(argv[i], "|")){
            num_of_pipes++;
        }
    }
    return num_of_pipes;
}

// Function to split argv based on pipes and return an array of command segments
char ***split_commands(char **argv, int num_pipes, int argc) {
    // Number of commands is number of pipes + 1
    int num_commands = num_pipes + 1;

    // Allocate memory for the array of command segments
    char ***commands = malloc(num_commands * sizeof(char **));
    if (commands == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    int cmd_index = 0; // Index for commands array
    int arg_index = 0; // Index for arguments within a command
    commands[cmd_index] = malloc((argc + 1) * sizeof(char *));
    if (commands[cmd_index] == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "|") == 0) {
            // Null-terminate the current command
            commands[cmd_index][arg_index] = NULL;
            cmd_index++;
            arg_index = 0;
            // Allocate memory for the next command segment
            commands[cmd_index] = malloc((argc + 1) * sizeof(char *));
            if (commands[cmd_index] == NULL) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }
        } else {
            commands[cmd_index][arg_index] = argv[i];
            arg_index++;
        }
    }
    // Null-terminate the last command
    commands[cmd_index][arg_index] = NULL;

    return commands;
}

// Function to free the allocated memory for commands
void free_commands(char ***commands, int num_commands) {
    for (int i = 0; i < num_commands; i++) {
        free(commands[i]);
    }
    free(commands);
}
void print_commands(char ***commands, int num_commands){
    // Print the commands for verification
    for (int i = 0; i < num_commands; i++) {
        printf("Command %d: ", i);
        for (int j = 0; commands[i][j] != NULL; j++) {
            printf("%s ", commands[i][j]);
        }
        printf("\n");
    }
}

// Function to allocate memory for an array to store pid, wpid, and status
int **get_pid_array(int num_pipes) {
    int num_commands = num_pipes + 1;
    
    // Allocate memory for the outer array
    int **pid_array = malloc(num_commands * sizeof(int *));
    if (pid_array == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for each inner array and initialize them
    for (int i = 0; i < num_commands; i++) {
        pid_array[i] = malloc(3 * sizeof(int));
        if (pid_array[i] == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        pid_array[i][0] = 0; // pid
        pid_array[i][1] = 0; // wpid
        pid_array[i][2] = 0; // status
    }

    return pid_array;
}

// Function to free the allocated memory for the pid array
void free_pid_array(int **pid_array, int num_pipes) {
    int num_commands = num_pipes + 1;
    for (int i = 0; i < num_commands; i++) {
        free(pid_array[i]);
    }
    free(pid_array);
}
// Print function for the pid array
void print_pid_array(int **pid_array, int num_pipes) {
    for (int i = 0; i < num_pipes + 1; i++) {
        printf("Command %d: pid = %d, wpid = %d, status = %d\n", i, pid_array[i][0], pid_array[i][1], pid_array[i][2]);
    }
}

// Function to create an array of pipes
int **create_pipes(int num_pipes) {
    // Allocate memory for the outer array
    int **pipes = malloc(num_pipes * sizeof(int *));
    if (pipes == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for each inner array and create pipes
    for (int i = 0; i < num_pipes; i++) {
        pipes[i] = malloc(2 * sizeof(int));
        if (pipes[i] == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        // Create the pipe
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    return pipes;
}

// Function to free the allocated memory for pipes
void free_pipes(int **pipes, int num_pipes) {
    for (int i = 0; i < num_pipes; i++) {
        free(pipes[i]);
    }
    free(pipes);
}

void print_pipes(int **pipes, int num_pipes) {
    for (int i = 0; i < num_pipes; i++) {
        printf("Pipe %d: read = %d, write = %d\n", i, pipes[i][0], pipes[i][1]);
    }
}

// Function to set the file descriptors for a command in a pipeline
void set_fds(int i, int num_pipes, int **pipes) {
    // If it's not the first command, set stdin to the read end of the previous pipe
    if (i > 0) {
        if (dup2(pipes[i-1][0], STDIN_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
    }

    // If it's not the last command, set stdout to the write end of the current pipe
    if (i < num_pipes) {
        if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
    }

    // Close all pipe file descriptors that are no longer needed
    for (int j = 0; j < num_pipes; j++) {
        // Close the read end of the current pipe
        if (j != i-1 && (i > 0)) { // Keep stdin of the current command open
            close(pipes[j][0]);
        }
        // Close the write end of the current pipe
        if (j != i) { // Keep stdout of the current command open
            close(pipes[j][1]);
        }
    }
}

// Function to close all pipe file descriptors in the parent process
void close_fds(int num_pipes, int **pipes) {
    for (int i = 0; i < num_pipes; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

/* ================================================== */

/* ====================== IF \ ELSE ==================== */

int run_conditions(char *if_buff, char *command_buff1, char *command_buff2, Variable *variables) {
    int status;

    // Evaluate the if condition
    substitute_variables(variables, if_buff);
    status = execute(if_buff, variables);
    if (status == 0) { // If the condition is true
        substitute_variables(variables, command_buff1);
        return execute(command_buff1, variables);
    } else { // If the condition is false
        substitute_variables(variables, command_buff2);
        return execute(command_buff2, variables);
    }
}


int preprocess_conditions(char *line, Variable *variables){
    char *if_buff;
    char *then_buff_input;
    char *command_buff1_input;
    char *else_buff_input;
    char *command_buff2_input;
    char *fi_buff_input;

    if_buff = remove_if_prefix(line);
    if (if_buff == NULL) {
        perror("Error in if input\n");
        return 1;
    }

    then_buff_input = readline("> ");
    char *then_buff = trim_whitespace(then_buff_input); // Trim leading and trailing whitespace
    if (then_buff && *then_buff) {
        add_history(then_buff);
    }
    // printf("then_buff: %s\n", then_buff); // debug  
    if (then_buff == NULL || strcmp(then_buff, "then") != 0) {
        perror("Error in then input\n");
        free(then_buff);
        return 1;
    }

    command_buff1_input = readline("> ");
    char *command_buff1 = trim_whitespace(command_buff1_input); // Trim leading and trailing whitespace
    if (command_buff1 && *command_buff1) {
        add_history(command_buff1);
    }
    // printf("command_buff1: %s\n", command_buff1); // debug
    if (command_buff1 == NULL) {
        perror("Error in command_buff1 input\n");
        free(then_buff);
        return 1;
    }

    else_buff_input = readline("> ");
    char *else_buff = trim_whitespace(else_buff_input); // Trim leading and trailing whitespace
    if (else_buff && *else_buff) {
        add_history(else_buff);
    }
    // printf("else_buff: %s\n", else_buff); // debug
    if (else_buff == NULL || strcmp(else_buff, "else") != 0) {
        perror("Error in else input\n");
        free(then_buff);
        free(command_buff1);
        return 1;
    }

    command_buff2_input = readline("> ");
    char *command_buff2 = trim_whitespace(command_buff2_input); // Trim leading and trailing whitespace
    if (command_buff2 && *command_buff2) {
        add_history(command_buff2);
    }
    // printf("command_buff2: %s\n", command_buff2); // debug
    if (command_buff2 == NULL) {
        perror("Error in command_buff2 input\n");
        free(then_buff);
        free(command_buff1);
        free(else_buff);
        return 1;
    }

    fi_buff_input = readline("> ");
    char *fi_buff = trim_whitespace(fi_buff_input); // Trim leading and trailing whitespace
    if (fi_buff && *fi_buff) {
        add_history(fi_buff);
    }
    // printf("fi_buff: %s\n", fi_buff); // debug
    if (fi_buff == NULL || strcmp(fi_buff, "fi") != 0) {
        perror("Error in fi input\n");
        free(then_buff);
        free(command_buff1);
        free(else_buff);
        free(command_buff2);
        return 1;
    }

    // Run all the commands
    int result = run_conditions(if_buff, command_buff1, command_buff2, variables);
    
    // Free all buffers
    free(then_buff_input);
    free(command_buff1_input);
    free(else_buff_input);
    free(command_buff2_input);
    free(fi_buff_input);

    return result;
}

char* remove_if_prefix(char *str) {
    if (strncmp(str, "if ", 3) == 0) {
        return str + 3;
    } else {
        return str;
    }
}

/* ================================================== */

/* ============= VARIABLES SUBTITUTION ==================== */

int read_variable(Variable variables[], char *name) {
    printf("enter to read_variable\n"); // debug
    char *value;
    value = read_line_no_prompt(".my_history");
    return set_variable(variables, name, value);
}

int set_variable(Variable variables[], char *name, char *value) {
    for (int i = 0; i < variable_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            strncpy(variables[i].value, value, MAX_VAR_VALUE_LEN);
            return 0;  // success
        }
    }
    if (variable_count < MAX_VARIABLES) {
        strncpy(variables[variable_count].name, name, MAX_VAR_NAME_LEN);
        strncpy(variables[variable_count].value, value, MAX_VAR_VALUE_LEN);
        variable_count++;
        return 0;  // success
    }
    else {
        printf("Variable limit reached. Cannot add more variables.\n");
        return 1;  // failure
}
}

// char* get_variable(Variable variables[], int var_count, char *name) {
char* get_variable(Variable variables[], char *name) {
    for (int i = 0; i < variable_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return variables[i].value;
        }
    }
    return NULL;
}

void substitute_variables(Variable variables[], char *command){
    // printf("enter to substitute_variables\n"); // debug
    char temp_command[1024];
    strcpy(temp_command, command);
    char substituted_command[1024] = "";
    char *token = strtok(temp_command, " ");
    
    while (token != NULL) {
        
        if (token[0] == '$') {
            char *value = get_variable(variables, token + 1);
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

void print_variables(Variable variables[]){
    for (int i = 0; i < variable_count; i++) {
        printf("Variable %d: name = %s, value = %s\n", i, variables[i].name, variables[i].value);
    }
}
/* ================================================== */