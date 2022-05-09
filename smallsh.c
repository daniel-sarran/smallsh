
/*
 * Author: Daniel Sarran
 * CS 344
 * Assignment: smallsh
 * Spring 2022
 */

/* Headers. */
#include <err.h>
#include <fcntl.h>
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
#define BACKGROUND_PID_ARRAY_LENGTH 16

/* Global variables. */
const char* CD = "cd";
const char* STATUS = "status";
const char* EXIT = "exit";

/*
 * Shell state structure.
 * Rather than hold various flags and memory blocks and manually passing them
 * to functions, and clearing them at each new command prompt, everything
 * is placed in a struct to pass as a single parameter, or zero out at
 * start of each command prompt.
 */
typedef struct {
    char* arguments[MAX_ARGUMENT_COUNT + 1];
    char commandString[MAX_COMMAND_LINE_LENGTH + 1];
    char expanded[MAX_COMMAND_LINE_LENGTH + 1];
    pid_t backgroundProcesses[BACKGROUND_PID_ARRAY_LENGTH];
    bool isForkActive;
    bool hasBackgroundFlag;
    bool hasForegroundOnlyFlag;
    bool hasInputRedirect;
    bool hasOutputRedirect;
    int inputFileArgIndex;
    int outputFileArgIndex;
    int status;
    pid_t pid;
} ShellState;

/* Function prototypes. */
void reapBackgroundProcesses(ShellState* shellState);
int takeInput(ShellState* shellState);
void parseArgumentTokens(ShellState* shellState);
void expandCommand(ShellState* shellState);
void cleanupProcessesBeforeExit(ShellState* shellState);
void changeDirectory(ShellState* shellState);
void checkStatus(ShellState* shellState);
int executeCommand(ShellState* shellState);

/*
 * Main loop. Creates a small shell. Not large. Not Medium. Smol. Supports
 * file navigation, shell commands using forking, I/O redirection operators as
 * well as * execution of background processes and orphan reaping.
 */
int main(void) {
    ShellState shellState;

    while (shellState.isForkActive == false) {
        /* Reap background processes.  */
        reapBackgroundProcesses(&shellState);

        /* Clear shell state and issue prompt. */
        memset(&shellState, 0, sizeof(shellState));
        shellState.pid = getpid();

        /* Prompt and receive user command. */
        if (takeInput(&shellState) != 0) {
            fprintf(stdout, "fgets() error");
            fflush(stdout);
            continue;
        }
        /* Handle blank lines and comments.  */
        if (shellState.commandString[0] == 0 ||
            shellState.commandString[0] == '#') {
            continue;
        }
        /* Process variable expansion on user input string. */
        expandCommand(&shellState);

        /* Split user input string into arguments array. */
        parseArgumentTokens(&shellState);

        /* Execute command as built-in or otherwise. */
        if (strcmp(shellState.arguments[0], EXIT) == 0) {
            cleanupProcessesBeforeExit(&shellState);
            exit(0);

        } else if (strcmp(shellState.arguments[0], CD) == 0) {
            changeDirectory(&shellState);

        } else if (strcmp(shellState.arguments[0], STATUS) == 0) {
            checkStatus(&shellState);

        } else {
            shellState.isForkActive = true;  // Prevents fork bombs.
            executeCommand(&shellState);     // Will reset isForkActive flag.
        }
    }
}

/*
 *
 */
void reapBackgroundProcesses(ShellState* shellState) {}

/*
 * Prompts user for command and arguments. Takes in a line of input via stdin.
 * Removes trailing newline. Receives shell state struct. Returns 0 if success,
 * 1 otherwise.
 */
int takeInput(ShellState* shellState) {
    /* Command prompt. */
    fprintf(stderr, ": ");
    fflush(stdout);

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
 * Performs variable expansion from `commandString` buffer and copies to
 * `expanded` buffer. Instances of "$$" are replaced by a string of the
 * shell process ID. Receives a shell state struct.
 */
void expandCommand(ShellState* shellState) {
    /* Create string of pid.  */
    char* pidstr;
    {
        int n = snprintf(NULL, 0, "%ld", shellState->pid);
        pidstr = malloc((n + 1) * sizeof *pidstr);
        sprintf(pidstr, "%ld", shellState->pid);
    }
    int pidLength = strlen(pidstr);
    char* cmd = shellState->commandString;
    char* exp = shellState->expanded;

    /* Read command string, replacing "$$" with pid string when writing. */
    int i = 0;  // Read buffer index.
    int j = 0;  // Write buffer index.

    while (cmd[i] != 0) {
        if (cmd[i] == '$' && cmd[i + 1] == '$') {
            /* Two consecutive '$' character, replace with pid string. */
            strcat(exp, pidstr);
            i += 2;
            j += pidLength;

        } else if (cmd[i] == '$' && cmd[i + 1] != '$') {
            /* One '$' character only, copy next two characters to expanded. */
            exp[j] = cmd[i];
            exp[j + 1] = cmd[i + 1];
            i += 2;
            j += 2;

        } else {
            /* Non '$' character, copy to expanded. */
            exp[j] = cmd[i];
            i++;
            j++;
        }
    }
    free(pidstr);
    return;
}

/*
 * Convert expanded user command string into an array of arguments. The
 * arguments in the command string are space-delimited. Identifies redirection
 * operators and background process operators.
 */
void parseArgumentTokens(ShellState* shellState) {
    char* token = strtok(shellState->expanded, " ");
    int i = 0;
    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            /* Input redirect detection and filename caching.  */
            shellState->arguments[i] = NULL;
            shellState->hasInputRedirect = true;
            shellState->inputFileArgIndex = i + 1;

        } else if (strcmp(token, ">") == 0) {
            /* Output redirect detection and filename caching.  */
            shellState->arguments[i] = NULL;
            shellState->hasOutputRedirect = true;
            shellState->outputFileArgIndex = i + 1;

        } else {
            shellState->arguments[i] = token;
        }
        i++;
        token = strtok(NULL, " ");
    }
    shellState->arguments[i] = NULL;

    /* If last character of command is '&', set background flag. */
    if (*shellState->arguments[i - 1] == '&') {
        shellState->hasBackgroundFlag = true;
        shellState->arguments[i - 1] = NULL;
    }
}

/*
 * Terminates any child processes of shell before exiting program. Receives
 * a shell state struct.
 */
void cleanupProcessesBeforeExit(ShellState* shellState) {
    // TODO: kill child and zombie processes.
}

/*
 * Change shell directory. If no path is provided, changes directory to the
 * user's HOME environment variable. Supports both relative and absolute
 * directory changes.
 */
void changeDirectory(ShellState* shellState) {
    char cwd[256];

    if (shellState->arguments[1] == NULL) {  // No path provided.
        if (chdir(getenv("HOME")) != 0) perror("chdir() error()");
    } else {
        if (chdir(shellState->arguments[1]) != 0) perror("chdir() error()");
    }
}

/*
 * Checks last foreground program termination status. Receives shell state
 * struct.
 */
void checkStatus(ShellState* shellState) {
    fprintf(stdout, "exit value %d\n", shellState->status);
    fflush(stdout);
}

/*
 * Executes a non-built-in command with user provided arguments. Handles
 * input/output redirection. Receives shell state struct and returns 0 when
 * successful, 1 otherwise.
 */
int executeCommand(ShellState* shellState) {
    pid_t spawnPid = -5;
    int childExitMethod = -5;
    spawnPid = fork();

    switch (spawnPid) {
        case -1:
            /* Handle forking errors.  */
            fprintf(stdout, "fork() failed.");
            fflush(stdout);
            return 1;

        case 0:
            /* Redirect input, as necessary. */
            if (shellState->hasInputRedirect == true) {
                int sourceFileDescriptor =
                    open(shellState->arguments[shellState->inputFileArgIndex],
                         O_RDONLY);
                if (sourceFileDescriptor == -1) {
                    fprintf(
                        stdout, "cannot open %s for input\n",
                        shellState->arguments[shellState->inputFileArgIndex]);
                    fflush(stdout);                    
                    shellState->isForkActive = false;
                    shellState->status = 1;
                    exit(1);
                }
                if (dup2(sourceFileDescriptor, 0) == -1) {
                    fprintf(stdout, "unable to redirect input\n");
                    fflush(stdout);                    
                    shellState->isForkActive = false;
                    exit(1);
                }
            }
            /* Redirect output, as necessary. */
            if (shellState->hasOutputRedirect == true) {
                int targetFileDescriptor =
                    open(shellState->arguments[shellState->outputFileArgIndex],
                         O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (targetFileDescriptor == -1) {
                    fprintf(
                        stdout, "cannot open %s for output",
                        shellState->arguments[shellState->inputFileArgIndex]);
                    fflush(stdout);
                    shellState->isForkActive = false;
                    shellState->status = 1;
                    exit(1);
                }
                if (dup2(targetFileDescriptor, 1) == -1) {
                    fprintf(stdout, "unable to redirect output");
                    fflush(stdout);
                    shellState->isForkActive = false;
                    exit(1);
                }
            }
            /* Execute command. */
            execvp(*shellState->arguments, shellState->arguments);

            /* Error handling for execvp. */
            fprintf(stdout, "unable to execute command\n");
            fflush(stdout);
            shellState->isForkActive = false;
            exit(1);

        default:
            if (shellState->hasBackgroundFlag == false) {
                /* Foreground processes: child blocks parent.  */
                waitpid(spawnPid, &childExitMethod, 0);

                /* Update exit status of foreground process. */
                if (WIFEXITED(childExitMethod) != 0) {
                    shellState->status = WEXITSTATUS(childExitMethod);
                } else {
                    shellState->status = WTERMSIG(childExitMethod);
                }

            } else {
                /* Background processes: child does not block.  */
                waitpid(spawnPid, &childExitMethod, WNOHANG);
                fprintf(stdout, "background pid is %ld\n", spawnPid);
                fflush(stdout);

                /* Cache pid of current background process into buffer. */
                for (int i = 0; i < BACKGROUND_PID_ARRAY_LENGTH; i++) {
                    if (shellState->backgroundProcesses[i] == 0) {
                        shellState->backgroundProcesses[i] = spawnPid;
                        break;
                    }
                }
            }
            /* Reset fork flag. */
            shellState->isForkActive = false;

            /* Reap background processes prior to next prompt.  */
            pid_t pid;
            while ((pid = waitpid(-1, &childExitMethod, WNOHANG)) > 0)
                    if (WIFEXITED(childExitMethod) != 0) {
                        fprintf(stdout, "background pid %ld is done: exit status %d\n", pid, WEXITSTATUS(childExitMethod));
                    } else {
                        fprintf(stdout, "background pid %ld is done: terminated by signal %d\n", pid, WTERMSIG(childExitMethod));
                    }
                    fflush(stdout);

            return 0;
    }
}
