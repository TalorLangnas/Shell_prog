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

int main() {
char command[1024];
char last_command[1024] = {"0"};
char prompt[1024] = "hello:";
char *token;
char *outfile;
int i, fd, amper, redirect, retid, status, redirect_error, redirect_append;
int exit_status = -1;
int infinte_proccess = 0;
int pipe_mode = 0;
char *argv[10];
int iter_counter = 0;


// Register the signal handler for SIGINT (Ctrl+C)
signal(SIGINT, sigint_handler);
// infinte_proccess = 1;   
while (1)
{
    printf("%s ", prompt);
    fgets(command, 1024, stdin);
    command[strlen(command) - 1] = '\0';
    // check if command contains "|"
    if(strstr(command, "|") != NULL){
        pipe_mode = 1;
        // count how many pipes are in the command
        int pipe_count = 0;
        for(int i = 0; i < strlen(command); i++){
            if(command[i] == '|'){
                pipe_count++;
            }
        }

        // Create a 2D array with pipe_count rows and 2 columns
        int **pipefds = (int **)malloc(pipe_count * sizeof(int *));
        for (int i = 0; i < pipe_count; i++) {
            pipefds[i] = (int *)malloc(2 * sizeof(int));
            if (pipefds[i] == NULL) {
                printf("Memory allocation failed.\n");
                free_pipefds(pipefds, pipe_count);
                return 1;
            }
            pipe(pipefds[i]);
            if(pipefds[i] == -1){
                perror("pipe");
                free_pipefds(pipefds, pipe_count);
                return 1;
            }
        }
        
        // num_of_commands = pipe_count + 1;
        // alolocate memory for the commands
        char **commands = malloc((pipe_count + 1) * sizeof(char *));
        if(commands == NULL){
            printf("Memory allocation failed.\n");
            return 1;
        }
        // duplicate the command
        char *command_copy_str = strdup(command);
        if(command_copy_str == NULL){
            printf("Memory allocation failed.\n");
            return 1;
        }
        // split the command by "|"
        for(int i = 0; i < pipe_count + 1; i++){
            token = strtok(command_copy_str, "|");
            commands[i] = token;
            command_copy_str = NULL;
        }
        // print the commands
        for(int i = 0; i < pipe_count + 1; i++){
            printf("Command %d: %s\n", i, commands[i]);
        }
        free(command_copy_str);
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
    /* parse command line */
    i = 0;
    // token = strtok (command," ");
    token = strtok (command," ");
    while (token != NULL)
    {
        argv[i] = token;
        token = strtok (NULL, " ");
        i++;
    }
    argv[i] = NULL;

    /* Is command empty */
    if (argv[0] == NULL)
        continue;

    /* Does command line end with & */ 
    if (! strcmp(argv[i - 1], "&")) {
        amper = 1;
        argv[i - 1] = NULL;
    }
    else 
        amper = 0; 
    if(!(strcmp(argv[0], "cd"))){
        if(argv[1] != NULL){
            if (chdir(argv[1]) != 0) {
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
    else if (! strcmp(argv[i - 2], ">")) {
        redirect = 1;
        redirect_error = 0;
        redirect_append = 0; 
        argv[i - 2] = NULL;
        outfile = argv[i - 1];
        }
        
    else if (! strcmp(argv[i - 2], "2>")) {
        redirect_error = 1;
        redirect = 0;
        redirect_append = 0;
        argv[i - 2] = NULL;
        outfile = argv[i - 1];
        }
    else if (! strcmp(argv[i - 2], ">>")) {
        redirect_append = 1;
        redirect_error = 0;
        redirect = 0;
        argv[i - 2] = NULL;
        outfile = argv[i - 1];
        }
    else if (! (strcmp(argv[0], "prompt") && strcmp(argv[1], "="))) {
        redirect_append = 0;
        redirect_error = 0;
        redirect = 0;
        strcpy(prompt, argv[2]);
        }
    else if (!(strcmp(argv[0], "echo"))) {
        // assume that argv[1] is never null 
        if(!(strcmp(argv[1], "$?"))) {
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
        if(infinte_proccess){
            // infinte_proccess = 0;
            while(1){
                printf("Infinite Proccess\n");
                sleep(2);
            }
        }
        if(pipe_mode){
            
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
        execvp(argv[0], argv);
    }
/*
The WIFEXITED macro is used to check if a child process terminated normally (i.e., by calling exit or 
returning from the main function). 
It takes an integer status code returned by wait or waitpid as its argument and returns 
a non-zero value if the child process terminated normally.
*/
  /* parent continues here */
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
