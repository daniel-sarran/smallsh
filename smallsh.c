/* Headers. */
#include <err.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

/* Constants. */
#define MAX_ARGUMENT_COUNT 512
#define MAX_COMMAND_LINE_LENGTH 2048

/* Global variables. */
const char* CD = "cd";
const char* STATUS = "status";
const char* EXIT = "exit";

/* Structures. */
typedef struct {
    char* arguments[MAX_ARGUMENT_COUNT + 1];
    char command[MAX_COMMAND_LINE_LENGTH + 1];
    bool isForkActive;
    int status;
    pid_t pid;
} ShellState;

/* Function prototypes. */
int takeInput(ShellState* shellState);
void parseArgumentTokens(ShellState* shellState);
void cleanupProcesses(ShellState* shellState);
void changeDirectory(ShellState* shellState);
void checkStatus(ShellState* shellState);
int nonBuiltInCommand(ShellState* shellState);

/* 
 * Creates a small shell.
 */
int main(void) {
    /* Initialize shellState.command line structure. */
    ShellState shellState;
    shellState.isForkActive = false;
        
    while (shellState.isForkActive == false) {
        /* Clear state and issue prompt. */
        memset(&shellState, 0, sizeof(shellState)); 
        shellState.pid = getpid();

        /* Command prompt. */
        fprintf(stderr, ": ");

        /* Receive user command. */
        if (takeInput(&shellState) != 0) {
            err(1, "ERROR: fgets() failed. Please try again.\n");
            continue;
        }
        /* Handle blank lines and comments.  */
        if (shellState.command[0] == '\n' || shellState.command[0] == '#') {
            continue;
        }
        /* Split user input into arguments. */
        parseArgumentTokens(&shellState);

        /* Execute `exit` command. */
        if (strcmp(shellState.arguments[0], EXIT) == 0) {
            cleanupProcesses(&shellState);
            continue;
        }
        /* Execute `cd` command. */
        if (strcmp(shellState.arguments[0], CD) == 0) {
            changeDirectory(&shellState);
            continue;
        }
        /* Execute `status` command. */
        if (strcmp(shellState.arguments[0], STATUS) == 0) {
            checkStatus(&shellState);
            continue;
        }
        /* Execute non-built-in command. */
        shellState.isForkActive = true;
        if (nonBuiltInCommand(&shellState) != 0)
            fprintf(stderr,
                    "ERROR: unable to execute '%s'.\n",
                    *shellState.arguments);
    }
}

/*
 * TODO: docstring
 */
int takeInput(ShellState* shellState) {
        /* Take input from stdin. */
        if(fgets(shellState->command, sizeof(shellState->command), stdin) == NULL) {
            return 1;
        }
        /* Remove fgets trailing newline. */
        if (shellState->command[strlen(shellState->command) - 1] == '\n') {
            shellState->command[strlen(shellState->command) - 1] = 0; 
        }
        return 0;
}

/*
 * TODO: docstring
 */
void parseArgumentTokens(ShellState* shellState) {
    /* Break line into tokens delimited by space character. */
    char* token = strtok(shellState->command, " ");
    int i = 0;
    while(token != NULL) {
        shellState->arguments[i] = token;

        /* Handle variable expansion.
         *
         * loop through token characters
         * copy to arguments[i] 
         * strcat the pid thing
         */
        if (strcmp(token, "$$") == 0) {
            char variableExpansion[50];
            sprintf(variableExpansion, "%ld", (long)shellState->pid);
            shellState->arguments[i] = variableExpansion;

        }
        i++;
        token = strtok(NULL, " ");
    }
    shellState->arguments[i] = NULL;
}

/*
 * TODO: docstring
 */
void cleanupProcesses(ShellState* shellState) {
    // TODO: kill child and zombie processes.
}

/*
 *
 */
void changeDirectory(ShellState* shellState) {
    char cwd[256];

    /* Change directory, no path provided. */
    if (shellState->arguments[1] == NULL) {
        chdir(getenv("HOME"));

        if (getcwd(cwd, sizeof(cwd)) == NULL)
            perror("getcwd() error");
        return;
    }

    /* Change directory, path provided. */
    if (chdir(shellState->arguments[1]) != 0)
        perror("chdir() error()");

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        perror("getcwd() error");
}

/*
 * TODO: docstring
 */
void checkStatus(ShellState* shellState) {
    // TODO: check status.
    printf("Most recent foreground status: %d.\n", shellState->status);
}

/*
 * TODO: docstring
 */
int nonBuiltInCommand(ShellState* shellState) {
    pid_t spawnPid = -5;
    int childExitMethod = -5;
    spawnPid = fork();
    if (spawnPid == -1) {
        perror("ERROR: fork() failed.");
        return 1;
    } else if (spawnPid == 0) { // Child process

        // TODO: Redirect stdin and stdout as necessary
        // TODO: Clean up args array

        // Execute command with arguments.
        execvp(*shellState->arguments, shellState->arguments);
        return 1;

    } // Parent process

    // IF: Command is in the foreground:
    waitpid(spawnPid, &childExitMethod, 0);

    /* Update exit status. */
    if (WIFEXITED(childExitMethod) != 0) {
        shellState->status = WEXITSTATUS(childExitMethod);
    } else {
        shellState->status = WTERMSIG(childExitMethod);
    }

    // TODO: ELSE: Don't (also if foreground-only mode flag is true)
    
    /* Reset fork flag. */
    shellState->isForkActive = false;
    return 0;
}
