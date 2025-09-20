#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>     // for file 

#define STACK_SIZE 5   // # of recent commands stored
#define MAX_LINE 80    // # Maximum command length

/* Stack struct to 5 most recent commands */
typedef struct {
    char *command[STACK_SIZE];
    int top;
} Stack;

/* initialize stack */
void init(Stack *stack){
    stack->top = -1;
}

/* check if stack is empty */
int isEmpty(Stack *stack){
    return stack->top == -1;
}

/* check if stack is full */
int isFull (Stack *stack){
    return stack->top == STACK_SIZE - 1;
}

/* push value into stack. If stack is full, the bottom */
/* the bottom element is kicked out and everything above */
/* is moved down and new element is placed on top */
void push(Stack *stack, char *str) {
    if (isFull(stack)) {
        free(stack->command[0]);
        for (int i = 1; i <= stack->top; i++) {
            stack->command[i - 1] = stack->command[i];
        }
        stack->top--;
    }
    stack->command[++stack->top] = malloc(strlen(str) + 1);
    strcpy(stack->command[stack->top], str);
}

/* remove element on top of stack */
char* pop(Stack *stack){
    if (isEmpty(stack)) {
        return NULL;
    }
    char *popped = stack->command[stack->top--];
    return popped; 
}

/* read the value at the top of the stack */
char* peek(Stack *stack){
    if (isEmpty(stack)) {
        return NULL;
    }
    return stack->command[stack->top];
}


int main(void) {
    pid_t p;
    int should_run = 1;            // flag to determine when to exit program
    int background = 0;            // flag for background process (&)
    Stack stack;                
    init(&stack);                  // create and initialize stack

    char input[MAX_LINE];          // raw input
    char *args[MAX_LINE/2 + 1];    // command lines arguments -- HOLDS POINTERS NOT CHARACTERS
    char directory[MAX_LINE];      // store current working directory
    char *infile, *outfile;        // store file names for input/output redirection

    while(should_run) {

        getcwd(directory, sizeof(directory));             // get working directory
        printf("osh:%s> ", (strrchr(directory, '/') + 1));  // print last directory, not entire source
        fflush(stdout);

        /* read from command line */
        if (fgets(input, MAX_LINE, stdin) == NULL) {
            perror("Cannot read input\n");
            continue;
        }
        
        /* reprompt if nothing is entered */
        if (input[0] == '\n' || input[0] == ' ' || input[0] == '\t') {
          continue;
        }

        /* quit if 'exit' is entered */
        if (strncmp(input, "exit", 4) == 0) {
            should_run = 0;
            printf("EXITING!\n");
            break;
        }

        /* if !!, check stack and repeat command if not empty */
        if (strcmp(input, "!!\n") == 0) {
            if (isEmpty(&stack)) {
                printf("No recent commands\n");
                continue;
            }
            else {
                char *last_cmd = peek(&stack);
                strcpy(input, last_cmd);
            }
        }

        /* PARSE/TOKENIZE INPUTS */
        int i = 0;
        infile = NULL;
        outfile = NULL;
        char *token = strtok(input, " \t\n");
        while(token != NULL) {
            if (strcmp(token, ">") == 0) {
                token = strtok(NULL, " \t\n");
                if (token != NULL) {
                    outfile = token;
                }
            }
            else if (strcmp(token, "<") == 0) {
                token = strtok(NULL, " \t\n");
                if (token != NULL) {
                    infile = token;
                }
            }
            else {
                args[i++] = token;
            }
            token = strtok(NULL, " \t\n");
        }
        args[i] = NULL;
        char *copy = strdup(input);
        push(&stack, copy);

        /* if 'cd', then return to base directory*/
        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                chdir(getenv("HOME"));
            }
            else {
                if(chdir(args[1]) != 0) {
                    printf("%s: %s: No such file or directory\n", args[0], args[1]);
                }
            }
            continue;
        }

        /* if &, parent process will not wait while child runs in background*/
        if (strcmp(args[i - 1], "&") == 0) {               
            args[i-1] = NULL;
            background = 1;
        }
        else {
            args[i] = NULL;
            background = 0;
        }

        p = fork();

        /* exit is fork failed */
        if (p < 0) {                            
            fprintf(stderr, "fork failed\n");
            should_run = 0;
            exit(0);
        }
        
        /* Child process */
        else if (p == 0) { 
            if (outfile) {
                int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("HELP ITS NOT WORKING");
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            if (infile) {
                int fd = open(infile, O_RDONLY);
                if (fd < 0){
                    perror("input file error\n");
                    exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            execvp(args[0], args);
        }

        /* Parent process will wait for child to finish unless background */
        /* process is requested */
        else {
            if (!background) {
              wait(NULL);
            }
        }
    }
    return 0;
}