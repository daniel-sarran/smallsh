# Mini Shell

### Goal
Mini Shell is a stripped down shell program for the purpose of exploring operating system concepts such as system calls, low-level I/O, and signal handling.

#### Non-Goals
A notable non-goal is to create a fully operational shell. This is an exploratory MVP.

### Overview
How is a shell built? 
What are its core functionalities? 
How does it interact with the operating system?

These are the questions to be answered in the development of this program.

#### Core functionalities include:
1) User prompt for commands
  - Input validation handles spaces, comments (`\#`)
2) Supports execution of commands
  - Built-in commands `cd`, `status`, `exit`
  - External commands from `exec` family of functions (e.g. `ls`) using forking and child processes
3) Supports command line operators
 - Execution of input (`<`) and output (`>`) redirection
 - Execution of background processes (`&`)
 - Variable expansion (`$$` exchanged for process ID of shell)
7) Supports signal interruption (`CTRL+Z`, `CTRL+C`)

### General syntax of command line
`command [arg1 arg2 ...] [< input_file] [> output_file] [&]`
