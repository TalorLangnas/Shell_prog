#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>
#include <signal.h>

volatile sig_atomic_t ctrl_c_pressed = 0;
int pid;
// Define the signal handler function
void sigint_handler(int sig) {
    printf("\nYou typed Control-C!\nhello: ");
    fflush(stdout);
    // exit from the child process
    if(pid != 0){
        kill(pid, SIGKILL);
    }
}

void free_pipefds(int **pipefds, int pipe_count){
    for (int i = 0; i < pipe_count; i++) {
        free(pipefds[i]);
    }
    free(pipefds);
}

// create function that zeroing all the counters and flags

int main() {
int command_index = 0;
int num_of_commands = 1;
char command[1024];
char last_command[1024] = {"0"};
char prompt[1024] = "hello:";
char *token;
char *command1;
char *outfile;
int i, fd, amper, redirect, retid, status, redirect_error, redirect_append;
int exit_status = -1;
int infinte_proccess = 0;
char *argv[10];
// char **argv[command_index][10];




// Register the signal handler for SIGINT (Ctrl+C)
signal(SIGINT, sigint_handler);
// infinte_proccess = 1;   
while (1)
{
    num_of_commands = 1;
    command_index = 0;
    printf("%s ", prompt);
    fgets(command, 1024, stdin);
    command[strlen(command) - 1] = '\0';
    printf("Command: %s\n", command); // debug
    // count how many pipes are in the command
    for(int i = 0; i < strlen(command); i++){
        if(command[i] == '|'){
            num_of_commands++;
        }
    }
    printf("Number of commandsssssss: %d\n", num_of_commands); // debug
    char *copy_command = strdup(command);
    if(copy_command == NULL){
        printf("Memory allocation failed.\n");
        return 1;
    }
    char **commandns = malloc(num_of_commands * sizeof(char *)); 
    command1 = strtok (copy_command,"|");
    // count number of commands (pipe "|" separated between commands)
    // int command_index = 1;
    while (command1 != NULL)
    {
        // argv[i] = token;
        commandns[command_index] = command1;
        printf("commandns[%d]: %s\n", command_index, commandns[command_index]); // debug
        command1 = strtok (NULL, "|");
        command_index++;
        // printf("token[%d]: %s\n", i, token);
    }

    // define pipes has number of commands - 1
    int **pipefds = (int **)malloc((num_of_commands - 1) * sizeof(int *));
    for (int i = 0; i < (num_of_commands - 1); i++) {
        pipefds[i] = (int *)malloc(2 * sizeof(int));
        if (pipefds[i] == NULL) {
            printf("Memory allocation failed.\n");
            free_pipefds(pipefds, (num_of_commands - 1));
            return 1;
        }
        pipe(pipefds[i]);
        if(pipefds[i] == -1){
            perror("pipe");
            free_pipefds(pipefds, (num_of_commands - 1));
            return 1;
        }
    }
    // activate the pipes
    for(int i = 0; i < num_of_commands - 1; i++){
        if(pipe(pipefds[i]) == -1){
            perror("pipe");
            free_pipefds(pipefds, (num_of_commands - 1));
            return 1;
        }
    }
    if(!(strcmp(command, "!!"))) {
        if(strlen(last_command) == 0) {
            printf("No commands in history.\n");
            continue;
        }
        strcpy(command, last_command);
    }
    else {
        strcpy(last_command, command);
    }
    char *command_copy = strdup(command);
    if(!(strcmp(command, "quit"))) {
        exit(0);
    }
    for(int index = 0; index < num_of_commands; index++){
        printf("enter to for loop\n"); // debug

    /* parse command line */
    i = 0;
    // token = strtok (command," ");
    token = strtok (commandns[index]," ");
    while (token != NULL)
    {
        argv[i] = token;
        // argv[command_index][i] = token;
        printf("token[%d]: %s\n", i, token); // debug
        token = strtok (NULL, " ");
        i++;
        // printf("token[%d]: %s\n", i, token);
    }
    
    argv[i] = NULL;
    /* Is command empty */
    if (argv[0] == NULL){ // old code
    // if (argv[command_index][0] == NULL){
        // call to zeroing function
        continue;
    }
        

    /* Does command line end with & */ 
    if (! strcmp(argv[i - 1], "&")) { // old code
    // if (! strcmp(argv[command_index][i - 1], "&")) {
        amper = 1;
        argv[i - 1] = NULL; // old code
        // argv[command_index][i - 1] = NULL;
    }
    else 
        amper = 0; 
    if(!(strcmp(argv[0], "cd"))){ // old code
    // if(!(strcmp(argv[command_index][0], "cd"))){
        if(argv[1] != NULL){ // old code
        // if(argv[command_index][1] != NULL){
            if (chdir(argv[1]) != 0) { // old code
            // if (chdir(argv[command_index][1]) != 0) {
            perror("cd");
        }
        }
        else{
            if(chdir(getenv("HOME")) != 0){
                perror("cd");
            }
        }
        free(command_copy);
        redirect_append = 0;
        redirect_error = 0;
        redirect = 0;
        continue;
    } 
    else if (! strcmp(argv[i - 2], ">")) { // old code
    // else if (! strcmp(argv[command_index][i - 2], ">")) {
        redirect = 1;
        redirect_error = 0;
        redirect_append = 0; 
        argv[i - 2] = NULL;  // old code
        // argv[command_index][i - 2] = NULL;
        outfile = argv[i - 1]; // old code
        // outfile = argv[command_index][i - 1];
        }
        
    else if (! strcmp(argv[i - 2], "2>")) { // old code
    // else if (! strcmp(argv[command_index][i - 2], "2>")) {
        redirect_error = 1;
        redirect = 0;
        redirect_append = 0;
        argv[i - 2] = NULL; // old code
        // argv[command_index][i - 2] = NULL;
        outfile = argv[i - 1];
        // outfile = argv[command_index][i - 1];        
        }
    else if (! strcmp(argv[i - 2], ">>")) { // old code
    // else if (! strcmp(argv[command_index][i - 2], ">>")) {
        redirect_append = 1;
        redirect_error = 0;
        redirect = 0;
        argv[i - 2] = NULL; // old code
        // argv[command_index][i - 2] = NULL;
        outfile = argv[i - 1]; // old code
        // outfile = argv[command_index][i - 1];
        }
    else if (! (strcmp(argv[0], "prompt") && strcmp(argv[1], "="))) {    // old code
    // else if (! (strcmp(argv[command_index][0], "prompt") && strcmp(argv[command_index][1], "="))) {
        redirect_append = 0;
        redirect_error = 0;
        redirect = 0;
        strcpy(prompt, argv[2]); // old code
        // strcpy(prompt, argv[command_index][2]);
        }
    else if (!(strcmp(argv[0], "echo"))) { // old code
    // else if (!(strcmp(argv[command_index][0], "echo"))) {
        // assume that argv[1] is never null 
        if(!(strcmp(argv[1], "$?"))) { // old code
        // if(!(strcmp(argv[command_index][1], "$?"))) {
            printf("%d\n", exit_status); // Print the stored exit status
            redirect_append = 0;
            redirect_error = 0;
            redirect = 0;
            free(command_copy);
                continue;
        } else{
            int length = strlen(command_copy) - 5;
        char *substring = malloc(length + 1); // Allocate memory for the substring (+1 for null terminator)

        if (substring == NULL) {
            printf("Memory allocation failed.\n");
            return 1;
        }
        strncpy(substring, &command_copy[5], length); // Copy the substring
        substring[length] = '\0'; // Null-terminate the substring
        
        redirect_append = 0;
        redirect_error = 0;
        redirect = 0;
        // printf("%s\n", substring);
        free(command_copy);
        free(substring); // Free the allocated memory
        }
    }
    else {
        redirect = 0;
        redirect_error = 0; 
        redirect_append = 0;
        } 

    /* for commands not part of the shell command language */ 

    // if (fork() == 0) {
    pid = fork();
    if (pid == 0) {
        // for the first command we need to redirect the read output to the next pipe
        if(index > 0)
        {
            dup2(pipefds[index - 1][0], 0);
        }
        if(index < num_of_commands - 1)
        {
            dup2(pipefds[index][1], 1);
        }
        // close all the pipes
        for (int i = 0; i < num_of_commands - 1; i++) {
            close(pipefds[i][0]);
            close(pipefds[i][1]);
        }
        // if it is the first command we need to redirect the output to the pipe 
        if(infinte_proccess){
            // infinte_proccess = 0;
            while(1){
                printf("Infinite Proccess\n");
                sleep(2);
            }
        }
        /* redirection of IO ? */
        if (redirect) {
            fd = creat(outfile, 0660); 
            close (STDOUT_FILENO) ; 
            dup(fd); 
            close(fd); 
            /* stdout is now redirected */
        }
        if (redirect_error) {
            fd = creat(outfile, 0660); 
            close (STDERR_FILENO) ; 
            dup(fd); 
            close(fd); 
            /* stdout is now redirected */
        }
        if (redirect_append) {
            // open fd for appending
            fd = open(outfile, O_WRONLY | O_APPEND, 0660);
            // fd = creat(outfile, 0660); 
            close (STDOUT_FILENO) ; 
            dup(fd); 
            close(fd); 
            /* stdout is now redirected */
        }
        printf("argv[0]: %s\n", argv[0]); // debug  
        execvp(argv[0], argv);
    }

/*
The WIFEXITED macro is used to check if a child process terminated normally (i.e., by calling exit or 
returning from the main function). 
It takes an integer status code returned by wait or waitpid as its argument and returns 
a non-zero value if the child process terminated normally.
*/
  /* parent continues here */
  // close all the pipes
        for (int i = 0; i < num_of_commands - 1; i++) {
            close(pipefds[i][0]);
            close(pipefds[i][1]);
        }
        for(int i = 0; i < num_of_commands; i++){
            wait(NULL);
        }
        if (amper == 0) {
            
                    retid = wait(&status);
                    if (retid != -1) {
                        if (WIFEXITED(status)) {
                            exit_status = WEXITSTATUS(status); // Store the exit status
                        } else if (WIFSIGNALED(status)) {
                            exit_status = 128 + WTERMSIG(status); // Store the signal number
                        }
                    }
        }
                // free(command_copy);
            }
}
}
