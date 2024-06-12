#include "shell.h"

// Global prompt variable
char *current_prompt;
int prev_status = 0;
int variable_count = 0;

int main() {
    
    int status;
    char *default_prompt = "hello: ";

    // Initialize the global prompt variable
    current_prompt = strdup(default_prompt);
    
    // Set the default prompt
    rl_set_prompt(current_prompt);
    
    char *command;
    Variable variables[MAX_VARIABLES];
    signal(SIGINT, sigint_handler);

    while (1)
    {   
        command = read_line(current_prompt);
        if (command == NULL) {
            continue;
        }
        // Handle if condition
        if (strncmp(command, "if ", 3) == 0) {
            preprocess_conditions(command, variables);
            free(command); // Free the allocated memory for "if ... "
            continue;
        }
        
        substitute_variables(variables, command);
        status = execute(command, variables); 
        prev_status = status;        
    }    
    
    return 0;
}