#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define STACK_SIZE 5   // # of recent commands stored
#define MAX_LINE 80    // Maximum command length

typedef struct {
    char *command[STACK_SIZE];
    int top;
} Stack;

void init(Stack *stack){
    // initialize top to -1 (empty stack)
    stack->top = -1;
}
int isEmpty(Stack *stack){
    return stack->top == 0;
}
int isFull (Stack *stack){
    return stack->top == STACK_SIZE - 1;
}

void push(Stack *stack, char *str) {
    if (isFull(stack)) {
        return;
    }
    stack->command[++stack->top] = malloc(strlen(str) + 1);
    strcpy(stack->command[stack->top], str);
}
char* pop(Stack *stack){
    if (isEmpty(stack)) {
        return NULL;
    }
    char *popped = stack->command[stack->top--];
    return popped;    // caller should free
}
char* peek(Stack *stack){
    if (isEmpty(stack)) {
        return NULL;
    }
    return stack->command[stack->top];
}


int main(void) {
    pid_t p;
    int should_run = 1;         // flag to determine when to exit program
    int background = 0;         // flag for background process (&)
    Stack stack;
    init(&stack);

    char input[MAX_LINE];          // raw input
    char *args[MAX_LINE/2 + 1];    // command lines arguments -- HOLDS POINTERS NOT CHARACTERS
    char directory[PATH_MAX];     // used to store directory info

    while(should_run) {
        getcwd(directory, sizeof(directory));
        printf("osh:%s> ", (strrchr(directory, '/') + 1));
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
        char *copy = strdup(input);
        push(&stack, copy);

        // if "cd" is entered, then return to the base directory if valid,
        // else, continue

        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                chdir(getenv("HOME"));
            }
            else {
                if(chdir(args[1]) != 0) {
                    printf("%s: %s: No such file or directory\n", args[0], args[1]);
                }
                chdir(args[1]);
            }
            continue;
        }
        else {
            p = fork();
        }

        if (strcmp(args[0], "!!") == 0) {
            if (isEmpty(&stack) == 1) {
                printf("No recent commands!\n");
            }
            else {
                continue; // IMPLEMENT!
            }
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

         // command execution begins w/ fork()
        if (p < 0) {                            // fork failed
            fprintf(stderr, "fork failed\n");
            should_run = 0;
            exit(0);
        }
        
        else if (p == 0) {    // child process
            
            execvp(args[0], args);
        }
        else {
            // parent process - will wait until child is done executing
            // unless '&' is entered
            if (background == 1) {
                continue;
            }
            wait(NULL);
        }
    }
    return 0;
}