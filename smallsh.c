#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#define MAX_ARGUMENT_COUNT 512
#define MAX_COMMAND_LINE_LENGTH 2048

int main(int argc, char* argv[]) {
    char command[MAX_COMMAND_LINE_LENGTH + 1];
    char* arguments[MAX_ARGUMENT_COUNT + 1];


    while (1) {
        /* Clear buffers and issue prompt. */
        memset(command, 0, sizeof(command)); 
        memset(arguments, 0, sizeof(arguments)); 
        fprintf(stderr, ": ");

        /* Take input from stdin. */
        if(fgets(command, sizeof(command), stdin) == NULL) {
            err(1, "ERROR: fgets() failed. Please try again.\n");
            continue;
        }

        /* Handle blank lines and comments.  */
        if (command[0] == '\n' || command[0] == '#') {
            continue;
        }

        /* Break line into tokens delimited by space character. */
        char* token = strtok(command, " ");
        int i = 0;
        while(token != NULL) {
            arguments[i] = token;
            i++;
            token = strtok(NULL, " ");
        }

        /* Execute `exit`. */
        if (strcmp(arguments[0], "exit\n") == 0) {
            break;
        }

        /* Execute `cd`. */
        if (strcmp(arguments[0], "cd") == 0 || 
            strcmp(arguments[0], "cd\n") == 0) {
            fprintf(stderr, "We changin direccs.\n");
        }

        /* Execute `status`. */
        if (strcmp(arguments[0], "status\n") == 0) {
            fprintf(stderr, "We checcin' status.\n");
        }

    }
}
