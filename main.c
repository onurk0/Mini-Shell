#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>   /* for file operations */
#include <termios.h>

#define STACK_SIZE 5 /* used to store last 5 commands */
#define MAX_LINE 80  /* maximum command length */
int history_index = -1;

struct termios org;

/* Stack struct to 5 most recent commands */
typedef struct
{
    char *command[STACK_SIZE];
    int top;
} Stack;

void init(Stack *stack);
int isEmpty(Stack *stack);
int isFull(Stack *stack);
void push(Stack *stack, char *str);
char *pop(Stack *stack);
char *peek(Stack *stack);
void reset_terminal();
void enable_raw_mode();
int key_read();
void reprompt(const char *dir, const char *line);
void arrow_up(Stack *stack, char *line, char *dir);
void arrow_down(Stack *stack, char *line, char *dir);
int read_input(char *buffer, int buffersize, Stack *stack, char *dir);



/* restore original terminal settings */
void reset_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &org);
}

/* enable raw mode for terminal input w/ no echo or line buffering*/
void enable_raw_mode() {

    tcgetattr(STDIN_FILENO, &org);    /* save current terminal setting*/
    atexit(reset_terminal);           /* ensure terminal is reset on exit*/

    struct termios raw = org;         /* load in original setting*/
    raw.c_lflag &= ~(ECHO | ICANON);  /* disable echo and canonical mode*/
    raw.c_cc[VMIN] = 1;               /* minimum number of read characters*/
    raw.c_cc[VTIME] = 0;              /* no timeout*/

    tcsetattr(STDIN_FILENO, TCSANOW, &raw);   /* apply settings*/
}

/* read a single character from STDIN */
/* return character if read successfully, -1 if not*/
int key_read() {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return c;
    } 
    return -1;
}

/* display the prompt with current directory */
void reprompt(const char *dir, const char *line) {
    printf("\r\033[Kosh:%s> %s", dir, line);    /* display a clear line and print*/
    fflush(stdout);
}

/* Handle up arrow key (older commands)*/
void arrow_up(Stack *stack, char *line, char *dir) {
    /* nothing in history */
    if (stack->top == -1) {
        return;
    }
    
    /* move up in history */
    if (history_index < stack->top) {
        history_index++;
    }
    /* load previous command and reprompt */
    strcpy(line, stack->command[stack->top - history_index]);
    reprompt(dir, line);
}

/* Handle down arrow key (recent commands)*/
void arrow_down(Stack *stack, char *line, char *dir) {
    // move down in history
    if (history_index > 0) {
        history_index--;
        strcpy(line, stack->command[stack->top - history_index]);
    }
    else {                 /*no history, clear line*/
        history_index = -1;
        line[0] = '\0';
    }
    reprompt(dir, line);
}

/* read user input; handle special keys and history */
int read_input(char *buffer, int buffersize, Stack *stack, char *dir) {
    int p = 0;
    buffer[0] = '\0';
    history_index = -1;

    while(1) {
        int c = key_read();

        // enter key will execute or reprompt
        if (c == '\n') {
            buffer[p] = '\0';
            printf("\n");

            // handle !!

            if (strcmp(buffer, "!!") == 0) {
                if (isEmpty(stack)) {
                    printf("No recent commands\n");
                    buffer[0] = '\0';
                }
                else {
                    char *last_cmd = peek(stack);
                    strcpy(buffer, last_cmd);
                    printf("%s\n", buffer);
                }
            }
            return p;
        }
        else if (c == 127 || c == '\b') {    // handle backspace and update prompt
            if (p > 0) {
                p--;
                buffer[p] = '\0';
                reprompt(dir, buffer);
            }
        }
        else if (c == 27) {        // handle arrow keys
            char sequence[2];
            if (read(STDIN_FILENO, &sequence[0], 1) != 1) {
                continue;
            }
            if (read(STDIN_FILENO, &sequence[1], 1) != 1) {
                continue;
            }

            if (sequence[0] == '[') {
                if (sequence[1] == 'A') {      // up arrow
                    arrow_up(stack, buffer ,dir);
                    p = strlen(buffer);
                }
                else if (sequence[1] == 'B') {    // down arrow
                    arrow_down(stack, buffer, dir);
                    p = strlen(buffer);
                }
            }
        }
        else if (p < buffersize - 1 && c>= 32 && c <= 126) {
            buffer[p++] = c;         // add printable character then reprompt
            buffer[p] = '\0';
            reprompt(dir, buffer);
        }
    }
}


/* initialize stack */
void init(Stack *stack)
{
    stack->top = -1;
}

/* check if stack is empty */
int isEmpty(Stack *stack)
{
    return stack->top == -1;
}

/* check if stack is full */
int isFull(Stack *stack)
{
    return stack->top == STACK_SIZE - 1;
}

/* push value into stack. If stack is full, the bottom */
/* the bottom element is kicked out and everything above */
/* is moved down and new element is placed on top */
void push(Stack *stack, char *str)
{
    if (isFull(stack))
    {
        free(stack->command[0]);
        for (int i = 1; i <= stack->top; i++)
        {
            stack->command[i - 1] = stack->command[i];
        }
        stack->top--;
    }
    stack->command[++stack->top] = malloc(strlen(str) + 1);
    strcpy(stack->command[stack->top], str);
}

/* remove element on top of stack */
char *pop(Stack *stack)
{
    if (isEmpty(stack))
    {
        return NULL;
    }
    char *popped = stack->command[stack->top--];
    return popped;
}

/* read the value at the top of the stack */
char *peek(Stack *stack)
{
    if (isEmpty(stack))
    {
        return NULL;
    }
    return stack->command[stack->top];
}

int main(void)
{
    enable_raw_mode();
    pid_t p1, p2;
    // flags for loop condition, background process, and pipes
    int should_run = 1, background = 0, seen_pipe = 0, j, k;
    Stack stack;
    init(&stack); // create and initialize stack

    char input[MAX_LINE];         // raw input
    char *args[MAX_LINE / 2 + 1]; // command lines arguments -- HOLDS POINTERS NOT CHARACTERS
    char directory[MAX_LINE];     // store current working directory
    char *infile, *outfile;       // store file names for input/output redirection
    char *left_args[MAX_LINE / 2 + 1], *right_args[MAX_LINE / 2 + 1];

    while (should_run)
    {
        seen_pipe = 0, j = 0, k = 0;
        infile = NULL, outfile = NULL;
        getcwd(directory, sizeof(directory));              // get working directory
        reprompt(strrchr(directory, '/') + 1, "");

        /* read from command line */
        if (read_input(input, MAX_LINE, &stack, strrchr(directory, '/') + 1) == 0) {
            continue;
        }

        /* reprompt if nothing is entered */
        if (input[0] == '\n' || input[0] == ' ' || input[0] == '\t')
        {
            continue;
        }

        /* quit if 'exit' is entered */
        if (strncmp(input, "exit", 4) == 0)
        {
            printf("EXITING!\n");
            break;
        }
        push(&stack, input);

        /* PARSE/TOKENIZE INPUTS */
        int i = 0;
        char *token = strtok(input, " \t\n");
        while (token)
        {

            /* check for pipe and store commands */
            if (strcmp(token, "|") == 0)
            {
                seen_pipe = 1;
            }

            /* check for input redirection */
            else if (strcmp(token, ">") == 0)
            {
                token = strtok(NULL, " \t\n");
                if (token)
                {
                    outfile = token;
                }
            }
            else if (strcmp(token, "<") == 0)
            {
                token = strtok(NULL, " \t\n");
                if (token)
                {
                    infile = token;
                }
            }
            else
            {
                if (!seen_pipe)
                {
                    left_args[j++] = token;
                }
                else
                {
                    right_args[k++] = token;
                }
            }
            token = strtok(NULL, " \t\n");
        }
        left_args[j] = NULL;
        right_args[k] = NULL;

        /* handle cd */
        if (left_args[0] && strcmp(left_args[0], "cd") == 0)
        {
            if (!left_args[1])
            {
                chdir(getenv("HOME"));
            }
            else if (chdir(left_args[1]) != 0)
            {
                printf("%s: No such file or directory\n", left_args[1]);
            }
            continue;
        }

        if (j > 0 && strcmp(left_args[j - 1], "&") == 0)
        {
            left_args[j - 1] = NULL;
            background = 1;
        }
        else if (seen_pipe && k > 0 && strcmp(right_args[k - 1], "&") == 0)
        {
            right_args[k - 1] = NULL;
            background = 1;
        }

        if (!seen_pipe)
        {
            p1 = fork();
            if (p1 == 0)
            {
                if (infile)
                {
                    int fd = open(infile, O_RDONLY);
                    dup2(fd, 0);
                    close(fd);
                }
                if (outfile)
                {
                    int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    dup2(fd, 1);
                    close(fd);
                }
                execvp(left_args[0], left_args);
                printf("%s: command not found\n", *left_args);
                exit(1);
            }
            else
            {
                if (!background)
                {
                    wait(NULL);
                }
            }
        }
        else
        {
            int fd[2];
            pipe(fd);

            p1 = fork();
            if (p1 == 0)
            {
                dup2(fd[1], 1);
                close(fd[0]);
                close(fd[1]);

                if (infile)
                {
                    int f = open(infile, O_RDONLY);
                    dup2(f, 0);
                    close(f);
                }
                execvp(left_args[0], left_args);
                printf("%s command not found\n", *left_args);
                exit(1);
            }

            p2 = fork();
            if (p2 == 0)
            {
                dup2(fd[0], 0);
                close(fd[0]);
                close(fd[1]);

                if (outfile)
                {
                    int f = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    dup2(f, 1);
                    close(f);
                }
                execvp(right_args[0], right_args);
                printf("%s command not found\n", *right_args);
                exit(1);
            }
            close(fd[0]);
            close(fd[1]);
            if (!background)
            {
                wait(NULL);
                wait(NULL);
            }
        }
    }
    return 0;
}