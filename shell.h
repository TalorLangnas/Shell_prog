//shell.h
#ifndef SHELL_H
#define SHELL_H

/*---LIBRARIES---*/
#include <stdio.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>

#define MAX_ARGS 10
#define BUF_SIZE 1024


/******************* GLOBAL VARIABLES ********************/
extern int prev_status;
extern int variable_count;
extern char *current_prompt;
/*****************************************************/

void sigint_handler(int sig);

/* ============= VARIABLES SUBTITUTION ==================== */
#define MAX_VARIABLES 100
#define MAX_VAR_NAME_LEN 50
#define MAX_VAR_VALUE_LEN 100

typedef struct {
    char name[MAX_VAR_NAME_LEN];
    char value[MAX_VAR_VALUE_LEN];
} Variable;

void substitute_variables(Variable variables[], char *command);
char* get_variable(Variable variables[], char *name);
int set_variable(Variable variables[], char *name, char *value);
void print_variables(Variable variables[]);
int read_variable(Variable variables[], char *name);
/* ================================================== */

/* ====================== IF \ ELSE ==================== */
int preprocess_conditions(char *line, Variable *variables);
char* remove_if_prefix(char *str);
int run_conditions(char *if_buff, char *command_buff1, char *command_buff2, Variable *variables);
/* ================================================== */

/* ================= CASE CHECKS ==================== */
typedef enum {
    DEFAULT,
    REDIRECT,
    RETID,
    STATUS,
    REDIRECT_ERROR,
    REDIRECT_APPEND,
    ECHO,
    ECHODOLLAR,
    CD,
    VAR_SUB,
    READ, 
    PIPE
} Case;

Case get_case(char *argv[], int *argc, int *amper);
int check_pipe(char *argv[], int argc);
int check_var_sub(char *argv[], int argc);
int check_read(char *argv[], int argc);
int check_redirect(char *argv[], int argc);
/* ================================================== */

/* ========== INPUT PREPROCESSING FUNCTIONS ============ */
char* trim_whitespace(char *str);
char *read_line(char *prompt);
void set_prompt(const char *new_prompt);
char *read_line_no_prompt(const char *history_file);
char **split_line(char *line, char *tok_delim, int *argc);
char *get_last_command();
/* ====================================================== */

/* ============= COMMAND HANDLING FUNCTIONS ============ */
int launch(char ***commands, int num_of_pipes, int amper, Variable *variables);
int execute(char *command, Variable *variables);
int configure_command(char **argv, int *argc, Case current_case, Variable *variables);
void redirect(char *argv[], int argc);
int echo_dollar(char **argv, int *argc);
int change_dir(char **argv, int argc); // Modified to use int argc
void redirect_error(char *argv[], int argc);
void redirect_append(char *argv[], int argc);
/* ==================================================== */ 

/* ============= PIPE FUNCTIONS ==================== */
int count_pipes(char **argv, int argc);

char ***split_commands(char **argv, int num_pipes, int argc);
void free_commands(char ***commands, int num_commands);
void print_commands(char ***commands, int num_commands);

int **get_pid_array(int num_pipes);
void free_pid_array(int **pid_array, int num_pipes);
void print_pid_array(int **pid_array, int num_pipes);

int **create_pipes(int num_pipes);
void free_pipes(int **pipes, int num_pipes);
void print_pipes(int **pipes, int num_pipes);

void set_fds(int i, int num_pipes, int **pipes);
void close_fds(int num_pipes, int **pipes);
/* ================================================== */


#endif