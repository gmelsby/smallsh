# smallsh
A toy shell that implements some features of popular shells. Uses `fork` and `exec` to create child processes.

## Features
### Input template
`command [args...] [< input.txt] [> output.txt] [&]`
### Background Processes
If background processes are enabled (See SIGSTP in [Signals](#signals) section), commands ending in `&` will run in the background.

### Variable Expansion
Instances of `$$` in shell input are transformed into the PID of the smallsh process.

### Included Commands
- `cd` changes the working directory.
- `status` prints info about the last foregrounded process ran.
- `exit` kills all child processes and exits the shell.

### Signals
Ctrl-C (SIGINT) terminates foregrounded child processes. It does not terminate smallsh.

Ctrl-Z (SIGSTP) toggles whether commands can be ran in the background using `&`.

### Input and Output Redirection
Input redirection is done with the `<` character and output redirection is done with the character `>`. \
Output redirection truncates the specified file if it currently exists and creates it if it does not. \
Appending to the output file e.g. `>>` is not supported.


## How to compile smallsh using gcc
The name of the .c file should be smallsh.c. \
If it has been changed, replace "smallsh.c" with its current name in the following instructions. \
In the directory containing smallsh.c, run the bash command:

`gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -o smallsh smallsh.c`

This should output an executable file named smallsh in the current directory.