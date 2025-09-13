#include <stdio.h>
#include <unistd.h>

#define MAX_LINE 80    // Maximum command length

int main(int argc, char** argv){

    char *args[MAX_LINE/2 + 1];    //command lines arguments
    int should_run = 1;   // flag to determine when to exit program

    while(should_run) {
        printf("osh>");
        fflush(stdout);

        
    }

    return 0;
}