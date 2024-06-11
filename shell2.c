#include "shell.h"

int prev_status = 0;
int variable_count = 0;

int main() {
    
    int status;
    // Define the name of the history file
    const char *history_file = ".my_history";
    
    char *command;
    Variable variables[MAX_VARIABLES];
    signal(SIGINT, sigint_handler);

    while (1)
    {   
        
        command = read_line("hello: ", history_file);
        if (command == NULL) {
            printf("write something!! the input is empty\n");
        }
        // Handle if condition
        if (strncmp(command, "if ", 3) == 0) {
            preprocess_conditions(command, variables);
            free(command); // Free the allocated memory for "if ... "
            continue;
        }
        
        substitute_variables(variables, command);
        status = execute(command, variables);
        print_variables(variables);       
        prev_status = status;        
    }    
    
    return 0;
}