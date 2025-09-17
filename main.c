#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 80    // Maximum command length

int main(void) {
    pid_t p;
    int should_run = 1;         // flag to determine when to exit program
    int background = 0;         // flag for background process (&)

    char input[MAX_LINE];          // raw input
    char *args[MAX_LINE/2 + 1];    // command lines arguments -- HOLDS POINTERS NOT CHARACTERS

    while(should_run) {
        printf("osh> ");
        fflush(stdout);

        if (fgets(input, MAX_LINE, stdin) == NULL) {    // read input from command line
            perror("Cannot read input\n");
            continue;
        }

        if (input[0] == '\n' || input[0] == ' ' || input[0] == '\t') {
          continue;
        }

        if (strncmp(input, "exit", 4) == 0) {     // QUIT if command = exit
            should_run = 0;
            printf("EXITING!\n");
            break;
        }

        int i = 0;                               // parse the input into args (tokenize)
        char *token = strtok(input, " \t\n");
        while(token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " \t\n");
        }

        // if the last command is &, then remove it from the arguments
        // and the parent will not call wait(), thus triggering a background process
        if (*args[i-1] == '&') {               
            args[i-1] = NULL;
            background = 1;
        }
        else {
            args[i] = NULL;
            background = 0;
        }


        p = fork();                             // command execution begins w/ fork()
        if (p < 0) {                            // fork failed
            fprintf(stderr, "fork failed\n");
            should_run = 0;
            exit(0);
        }
        
        else if (p == 0) {    // child process
            
            execvp(args[0], args);
        }
        else {                 // parent process - wait until child is done executing
            if (background == 1) {
                continue;
            }
            wait(NULL);
        }
    }
    return 0;
}