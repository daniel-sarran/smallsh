/* Headers. */
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
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
    char commandString[MAX_COMMAND_LINE_LENGTH + 1];
    char expanded[MAX_COMMAND_LINE_LENGTH + 1];
    bool isForkActive;
    int status;
    pid_t pid;
} ShellState;

/* Function prototypes. */
int takeInput(ShellState* shellState);
void parseArgumentTokens(ShellState* shellState);
void expandCommand(ShellState* shellState);
void cleanupProcessesBeforeExit(ShellState* shellState);
void changeDirectory(ShellState* shellState);
void checkStatus(ShellState* shellState);
int executeCommand(ShellState* shellState);

/*
 * Creates a small shell.
 */
int main(void) {
    /* Initialize shellState.command line structure. */
    ShellState shellState;

    while (shellState.isForkActive == false) {
        /* Clear state and issue prompt. */
        memset(&shellState, 0, sizeof(shellState));
        shellState.pid = getpid();

        /* Receive user command. */
        if (takeInput(&shellState) != 0) {
            err(1, "ERROR: fgets() failed. Please try again.\n");
            continue;
        }
        /* Handle blank lines and comments.  */
        if (shellState.commandString[0] == 0 ||
            shellState.commandString[0] == '#') {
            continue;
        }
        /* Process variable expansion. */
        expandCommand(&shellState);

        /* Split user input into arguments. */
        parseArgumentTokens(&shellState);

        /* Execute command as built-in or otherwise. */
        if (strcmp(shellState.arguments[0], EXIT) == 0) {
            /* Execute `exit` command. */
            cleanupProcessesBeforeExit(&shellState);
            exit(0);
        } else if (strcmp(shellState.arguments[0], CD) == 0) {
            /* Execute `cd` command. */
            changeDirectory(&shellState);
        } else if (strcmp(shellState.arguments[0], STATUS) == 0) {
            /* Execute `status` command. */
            checkStatus(&shellState);
        } else {
            /* Execute non-built-in command. */
            shellState.isForkActive = true;
            executeCommand(&shellState);  // isForkActive flag resets.
        }
    }
}

/*
 * Prompts user for command and arguments.
 */
int takeInput(ShellState* shellState) {
    /* Command prompt. */
    fprintf(stderr, ": ");

    /* Take input from stdin. */
    if (fgets(shellState->commandString, sizeof(shellState->commandString),
              stdin) == NULL) {
        return 1;
    }
    /* Remove fgets trailing newline. */
    int cmdLength = strlen(shellState->commandString);
    if (shellState->commandString[cmdLength - 1] == '\n') {
        shellState->commandString[cmdLength - 1] = 0;
    }
    return 0;
}

/*
 * Performs variable expansion from `commandString` and copies to `expanded`.
 */
void expandCommand(ShellState* shellState) {
    char* pidstr;
    {
        int n = snprintf(NULL, 0, "%ld", shellState->pid);
        pidstr = malloc((n + 1) * sizeof *pidstr);
        sprintf(pidstr, "%ld", shellState->pid);
    }
    int pidLength = strlen(pidstr);
    char* cmd = shellState->commandString;
    char* exp = shellState->expanded;

    /* Read in character by character, replace "$$" with pid. */
    int i = 0;
    int j = 0;
    while (cmd[i] != 0) {
        if (cmd[i] != '$') {
            /* Non '$' character, copy to expanded. */
            exp[j] = cmd[i];
            i++;
            j++;
        } else if (cmd[i] == '$' && cmd[i + 1] == '$') {
            /* Two consecutive '$' character, replace with pid string. */
            strcat(exp, pidstr);
            i += 2;
            j += pidLength;
        } else {
            /* One '$' character only, copy next two characters to expanded. */
            exp[j] = cmd[i];
            exp[j + 1] = cmd[i + 1];
            i += 2;
            j += 2;
        }
    }
    free(pidstr);
    return;
}

/*
 * Convert user command string into an array of arguments.
 */
void parseArgumentTokens(ShellState* shellState) {
    /* Break line into tokens delimited by space character. */
    char* token = strtok(shellState->expanded, " ");
    int i = 0;
    while (token != NULL) {
        shellState->arguments[i] = token;
        i++;
        token = strtok(NULL, " ");
    }
    shellState->arguments[i] = NULL;
}

/*
 * Terminates any child processes of shell before exiting program.
 */
void cleanupProcessesBeforeExit(ShellState* shellState) {
    // TODO: kill child and zombie processes.
}

/*
 * Change shell directory.
 */
void changeDirectory(ShellState* shellState) {
    char cwd[256];

    /* Change directory, no path provided. */
    if (shellState->arguments[1] == NULL) {
        if (chdir(getenv("HOME")) != 0) perror("chdir() error()");
    } else {
        /* Change directory, path provided. */
        if (chdir(shellState->arguments[1]) != 0) perror("chdir() error()");
    }
}

/*
 * Checks last foreground program termination status.
 */
void checkStatus(ShellState* shellState) {
    // TODO: check status.
    printf("exit value %d\n", shellState->status);
}

/*
 * Executes a non-built-in command from $PATH.
 */
int executeCommand(ShellState* shellState) {
    pid_t spawnPid = -5;
    int childExitMethod = -5;
    spawnPid = fork();
    if (spawnPid == -1) {
        perror("fork() failed.");
        return 1;
    } else if (spawnPid == 0) {  // Child process

        // TODO: Redirect stdin and stdout as necessary
        // TODO: Clean up args array

        // Execute command with arguments.
        execvp(*shellState->arguments, shellState->arguments);
        perror("  ERROR: execvp() failed");
        return 1;

    }  // Parent process

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
