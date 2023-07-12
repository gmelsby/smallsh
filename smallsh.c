// Name: Gregory Melsby
// OSU Email: melsbyg@oregonstate.edu
// Course: CS344 - Operating Systems / Section 400
// Assignment: Assignment 3: smallsh
// Due Date: 05/09/2022
// Description: Shell that can run basic commands

#define _POSIX_C_SOURCE 200809L // makes signal handling work when compiled std=c99
#include <stdlib.h> // malloc, realloc, free, exit, etc
#include <stdint.h> // intmax_t
#include <fcntl.h> // open, close
#include <stdio.h> // printf, fprintf, perror, etc
#include <sys/types.h> // pid_t
#include <unistd.h> // getpid, fork
#include <string.h> // size_t, strlen, etc
#include <sys/wait.h> // waitpid
#include <signal.h> // sigset_t, kill, signal processing
#include <errno.h> // perror


// maximum number of characters the command line will read
const int MAX_LINE_LEN = 2048;

// struct to store pointers to each part of the input
struct inputparts {
  // stores how many arguments were passed in
  int arg_count;
  // stores pointer to string representing path of input stream
  char *instream;
  // stores pointer to string representing path of output stream
  char * outstream;
  // whether the command should run in the background
  _Bool background;
 // stores array of pointers to arguments
  char* args[512];
  };

// struct to store information about currently running background processes
struct backgroundprocesses {
  int count;
  pid_t processes[30];
};

// tracks whether '&' at the end will cause the command to run in the background
// can be modified in signal processors
volatile sig_atomic_t background_capacity = 1;

// prototype for background_on so background_off recognizes function when compling
void background_on(int signal);

/*
 * Signal handler to toggle off background process capacity with '&'
 * @param signal: int that lets handler know what signal is being processed.
 *                necessary for function declaration but not useful.
 * @returns modifies background_capacity global variable
 */
void background_off(int signal) {
  char* output = "\nEntering foreground-only mode (& is now ignored)\n:";
  background_capacity = 0;
  // write loop to ensure output
  for (size_t bytes_written = 0; bytes_written < sizeof *output * 51;) {
    size_t current_bytes_written = write(STDOUT_FILENO, output, 51 * sizeof *output - bytes_written);
    if (current_bytes_written == 0) {
      break;
    }
    bytes_written += current_bytes_written;
  }
  // does something to signal parameter to avoid warning when compling of unused param
  signal++;

  // modifies signal handler for SIGSTP to call background_off next time
  struct sigaction sigtstp_action;
  // pretty sure this call to memset is reentrant but couldn't find much about it--should only modify sigstp_action memory
  memset(&sigtstp_action, 0, sizeof sigtstp_action);
  sigtstp_action.sa_handler = background_on;
  sigfillset(&(sigtstp_action.sa_mask));
  // SA_RESTART lets certain functions continue if system call was deliverd in the middle of execution
  sigtstp_action.sa_flags = SA_RESTART;

  sigaction(SIGTSTP, &sigtstp_action, NULL);

}

/*
 * Signal handler to toggle off background process capacity with '&'
 * Differs from background_off by not printing a colon after message
 * To be used when SIGSTP is received while a foreground process is being executed
 * @param signal: int that lets handler know what signal is being processed.
 *                necessary for function declaration but not useful.
 * @returns modifies background_capacity global variable
 */
void background_off_nocol(int signal) {
  char* output = "\nEntering foreground-only mode (& is now ignored)\n";
  background_capacity = 0;
  // write loop to ensure output
  for (size_t bytes_written = 0; bytes_written < sizeof *output * 50;) {
    size_t current_bytes_written = write(STDOUT_FILENO, output, 50 * sizeof *output - bytes_written);
    if (current_bytes_written == 0) {
      break;
    }
    bytes_written += current_bytes_written;
  }
  // does something to signal parameter to avoid warning when compling of unused param
  signal++;

  // no need to make new sigaction--taken care of in execcommand function based on value of background_capacity
}

/*
 * Signal handler to toggle on background process capacity with '&'
 * @param signal: int that lets handler know what signal is being processed.
 *                necessary for function declaration but not useful.
 * @returns modifies background_capacity global variable
 */ 
void background_on(int signal) {
  char* output = "\nExiting foreground-only mode\n:";
  background_capacity = 1;
  // write loop to ensure output
  for (size_t bytes_written = 0; bytes_written < sizeof *output * 31;) {
    size_t current_bytes_written = write(STDOUT_FILENO, output, 31 * sizeof *output - bytes_written);
    if (current_bytes_written == 0) {
      break;
    }
    bytes_written += current_bytes_written;
  }
  // does something to signal parameter to avoid warning when compling of unused param
  signal++;

  // modifies signal handler for SIGSTP to call background_on next time
  struct sigaction sigtstp_action;
  // pretty sure this call to memset is reentrant but couldn't find much about it--should only modify sigstp_action memory
  memset(&sigtstp_action, 0, sizeof sigtstp_action);
  sigtstp_action.sa_handler = background_off;
  sigfillset(&(sigtstp_action.sa_mask));
  // SA_RESTART lets certain functions continue if system call was deliverd in the middle of execution
  sigtstp_action.sa_flags = SA_RESTART;

  sigaction(SIGTSTP, &sigtstp_action, NULL);
}

/*
 * Signal handler to toggle on background process capacity with '&'
 * Differs from background_on by not printing a colon after message
 * To be used when SIGSTP is received while a foreground process is being executed
 * @param signal: int that lets handler know what signal is being processed.
 *                necessary for function declaration but not useful.
 * @returns modifies background_capacity global variable
 */ 
void background_on_nocol(int signal) {
  char* output = "\nExiting foreground-only mode\n";
  background_capacity = 1;
  // write loop to ensure output
  for (size_t bytes_written = 0; bytes_written < sizeof *output * 30;) {
    size_t current_bytes_written = write(STDOUT_FILENO, output, 30 * sizeof *output - bytes_written);
    if (current_bytes_written == 0) {
      break;
    }
    bytes_written += current_bytes_written;
  }
  // does something to signal parameter to avoid warning when compling of unused param
  signal++;

  // no need to make new sigaction--taken care of in execcommand function based on value of background_capacity
}

/*
 * Sets up and deploys signal handler for SIGTSTP based on value of background_capacity
 */
void set_sigtstp_handler() {
  // sets up signal handler for SIGTSTP
  struct sigaction sigtstp_action;
  memset(&sigtstp_action, 0, sizeof sigtstp_action);
  // determines whether to call background_off or background_on based on background_capacity
  // sets up to turn off background_capacity if it is currently on
  if (background_capacity) {
    sigtstp_action.sa_handler = background_off;
  }
  // sets up to turn on background_capacity if it is currently off
  else {
    sigtstp_action.sa_handler = background_on;
  }

  // doesn't allow other signals while signal handler is being executed
  sigfillset(&(sigtstp_action.sa_mask));
  // SA_RESTART allows some functions to continue after being interrupted by a signal
  sigtstp_action.sa_flags = SA_RESTART;
  // activates signal handler
  sigaction(SIGTSTP, &sigtstp_action, NULL);
}

/*
 * Loops through background processes, checking if any have ended
 * If so, cleans up process and outputs info about exit or termination
 * @param bp: pointer to backgroundprocesses struct that keeps an array and count of background processes
 * @returns modified bp if any background processes have ended
 */
void monitorbps(struct backgroundprocesses *bp) {
    // checks the current background processes to see if they have terminated
    for (int i = 0; i < bp->count;) {
      pid_t child_pid = bp->processes[i];
      int background_status;
      // case where a background process has finished
      if (waitpid(child_pid, &background_status, WNOHANG)) {
        // case where exited normally--prints out exit value
        if (WIFEXITED(background_status)) {
          printf("background pid %jd is done: exit value %d\n", (intmax_t) child_pid, WEXITSTATUS(background_status));
          fflush(stdout);
        }
        // case where terminated by a signal--prints out value of signal
        if (WIFSIGNALED(background_status)) {
          printf("background pid %jd is done: terminated by signal %d\n", (intmax_t) child_pid, WTERMSIG(background_status));
          fflush(stdout);
        }
        // moves rest of backgound pids left 1 index to replace finished process
        for (int j = i; j < bp->count - 1; j++) {
          bp->processes[j] = bp->processes[j+1];
        }
        bp->processes[bp->count - 1] = 0;
        // decrements count of bpackground processes
        bp->count--;
      }
      // case where bp->processes[i] has not yet terminated--continues through array
      else {
        i++;
      }
    }
}

/*
 * Input parsing functionality
 * @param instring, pointer to the first character in the string to be parsed
 * @param in, pointer to a struct inputparts that will have its data membered filled with parsed strings
 * @returns 1 if input is empty or a comment, otherwise returns 0
 * @returns modified instring with null terminators where spaces were due to strtok
 * @returns modified struct inputparts *in with parsed arguments and input/output redirection info
 */
int parseargs(char* instring, struct inputparts *in) {
  // if the input is fully empty we loop again
  if (strlen(instring) == 0) {
    printf("\n");
    return 1;
  }

  // checks if the last character is a newline
  // if so replaces it with a null terminator
  if (instring[strlen(instring) - 1] == '\n') {
    instring[strlen(instring) - 1] = '\0';

    // if the input was just a new line we loop again
    if (strlen(instring) == 0) {
      return 1;
    }
  }

  // checks if the last character is '&'
  // if so sets the command to run in background
  // we then move the null terminator one char earlier to replace '&'
  if (strlen(instring) > 1 && instring[strlen(instring) - 1] == '&' && instring[strlen(instring) - 2] == ' ') {
    // sets command to run in background if background_capacity is enabled
    if (background_capacity) {
      in->background = 1;
    }
    // removes the '& even if background_capacity is not on
    instring[strlen(instring) - 1] = '\0';
  }

  // parses input with strtok
  // strips out spaces
  in->args[0] = strtok(instring, " ");

  // if the line is empty or the first non-whitespace character is '#' we don't do anything else with the input
  if (in->args[0] == NULL || *in->args[0] == '#') {
    return 1;
  }

  // setup for parsing while loop
  char *nextstr = strtok(NULL, " ");
  in->arg_count += 1;
  
  // parses the input string with strtok until it returns null
  while (nextstr != NULL) {
    // checks if the next part of input is just '<', if so puts pointer to string after that one in in->instream
    if (*nextstr =='<' && nextstr[1] == '\0') {
      in->instream = strtok(NULL, " ");
    }

    // checks if the next part of input is just '>', if so puts pointer to string after that one in in->instream
    else if (*nextstr == '>' && nextstr[1] == '\0') {
      in->outstream = strtok(NULL, " ");
    }

    else {
      // loop exits if there are more arguments than can be stored
      if (in->arg_count == 512 + 1) {
        fprintf(stderr, "Too many arguments passed in. Some will not be parsed.\n");
        fflush(stderr);
        break;
      }

      // otherwise we add the argument string to the array in struct in and increment in->arg_count
      in->args[in->arg_count] = nextstr;
      in->arg_count++;
    }

    // gets a pointer to the next string
    nextstr = strtok(NULL, " ");
  }

  // null terminates the args array
  in->args[in->arg_count] = (char*) NULL;

  return 0;
}



/*
 * Expands "$$" if present in any of the arguments 
 * Even if no arguments are expanded, allocates new memory where the arguments will be stored
 * @param *in pointer to the inputparts struct containing information about the arguments
 * @returns modified struct inparts *in with pointers to newly allocated memory with expanded args
 */
void expandargs(struct inputparts *in) {
  pid_t pid = getpid();
  char* pidstr;
  // write to null to get the length of the string
  int pidstr_len = snprintf(NULL, 0, "%jd", (intmax_t) pid);
  // to do
  // allocates approrpriate memory
  pidstr = malloc(pidstr_len + 1);
  memset(pidstr, '\0', pidstr_len + 1);
  // fills pidstr with pid in string form
  sprintf(pidstr, "%jd", (intmax_t) pid);

  // goes through each argument and changes the pointer to newly allocated mem
  // if "$$" in in the input string it is changed to the current process pid
  for (int arg_idx = 0; arg_idx < in->arg_count;) {
    // allocate new memory for the resulting string
    char *tempstr = malloc(sizeof (*tempstr) * (strlen(in->args[arg_idx]) + 1));
    size_t tempstr_len = strlen(in->args[arg_idx] + 1);
    memset(tempstr, '\0', tempstr_len);

    // iterate through the inputted string copying each character
    unsigned int arg_str_idx = 0;
    int tempstr_idx = 0;
    char next_char;
    for (arg_str_idx = 0; arg_str_idx <= strlen(in->args[arg_idx] + 1);) {
      next_char = in->args[arg_idx][arg_str_idx];
      arg_str_idx++;
      // if the character is '$' check the next character
      if (next_char == '$') {
        char after_next_char = in->args[arg_idx][arg_str_idx];
        arg_str_idx++;

        // if the character after is '$' copy the current pid as a string into result
        if (after_next_char == '$') {
          tempstr = realloc(tempstr, tempstr_len + pidstr_len);
          tempstr_len += pidstr_len;
          strcpy(&tempstr[tempstr_idx], pidstr);
          tempstr_idx += pidstr_len;
        }

        // otherwise just copy both characters as they are
        else {
          tempstr[tempstr_idx] = next_char;
          tempstr[tempstr_idx + 1] = after_next_char;
          tempstr_idx += 2;
        }
      }

      // no '$', just standard copy
      else {
        tempstr[tempstr_idx] = next_char;
        tempstr_idx += 1;
      }
    }

    // null-terminator for newly-created string
    tempstr[tempstr_idx] = '\0';

    // assign pointer to new string to the struct data member args[arg_idx]
    in->args[arg_idx] = tempstr;
    arg_idx += 1;
  }

  // freeds pidstr when done with it
  free(pidstr);
}


/*
 * Forks and executes commands passed in through inputparts struct in the child process
 * Also handles input and output redirection if necessary
 * Updates exitstatus and exitednormally for status functionality in main function
 * Updates backgroundprocesses struct to keep track of newly formed background processes
 * @param in: pointer to inputparts struct that contains information about the inputted command and redirection
 * @param bp: pointer to backgroundprocesses struct that keeps an array and count of background processes
 * @param exitstatus: pointer to int that stores exit status or term signal of last foreground command
 * @param exitednormally: pointer to _Bool that stores whether last foreground command exited normally
 * @returns modifies exitstatus and exitednormally if foreground command is run
 */
void execcommand(struct inputparts *in, struct backgroundprocesses *bp, int *exitstatus, _Bool *exitednormally) {
  // checks that there will not be more than 30 background processes
  if (in->background && bp->count >= 30) {
    fprintf(stderr, "Too many background processes already! Try again when one has terminated.\n");
    return;
  }

  // forks to create a child process
  pid_t fork_pid = fork(); 
  int child_status;
  // if fork failed
  if (fork_pid < 0) {
    perror("Issue making fork");
    fflush(stderr);
    return;
  }

  // case where we are in the child process
  else if (fork_pid == 0) {
    // sets up ignore for SIGTSTP in child process
    struct sigaction new_sigtstp_action;
    memset(&new_sigtstp_action, 0, sizeof new_sigtstp_action);
    // SIG_IGN sets ignore behavior
    new_sigtstp_action.sa_handler = SIG_IGN;
    sigfillset(&(new_sigtstp_action.sa_mask));
    // SA_RESTART lets certain functions continue if system call was deliverd in the middle of execution
    new_sigtstp_action.sa_flags = SA_RESTART;

    // enables new signal behavior
    sigaction(SIGTSTP, &new_sigtstp_action, NULL);

    // if command is to be run in foreground, sets up default behavior for SIGINT
    if (!in->background) {
      struct sigaction new_sigint_action;
      memset(&new_sigint_action, 0, sizeof new_sigint_action);
      // SIG_DFL sets default behavior
      new_sigint_action.sa_handler = SIG_DFL;
      sigfillset(&(new_sigint_action.sa_mask));
      // SA_RESTART lets certain functions continue if system call was deliverd in the middle of execution
      new_sigint_action.sa_flags = SA_RESTART;

      // enables new signal behavior
      sigaction(SIGINT, &new_sigint_action, NULL);
    }
    
    // redirects stderr if command is to be executed in background
    else {
      int new_erroutput = open("/dev/null", O_WRONLY);
      dup2(new_erroutput, STDERR_FILENO);
    }

    // handle redirection if necessary
    int new_input;
    int new_output;
      
    // handles input redirection
    if (in->instream != NULL || in->background) {
      // case where input redirection is passed in
      if (in->instream != NULL) {
        new_input = open(in->instream, O_RDONLY);
      }

      // if the command is to be executed in background and does not have input redirection already
      // gets file descriptor for dev/null
      else {
        new_input = open("/dev/null", O_RDONLY);
      }
      // makes sure open worked, if not prints error
      if (new_input < 0) {
        fprintf(stderr, "Could not open %s for input\n", in->instream);
        exit(1);
      }
      // redirects input
      dup2(new_input, STDIN_FILENO);
    }

    // handles ouptut redirection
    if (in->outstream != NULL || in->background) {
      // case where output redirection is passed in
      if (in->outstream != NULL) {
      // creates new file if the output file does not exist, otherwise overwrites existing file
      new_output = open(in->outstream, O_WRONLY | O_CREAT | O_TRUNC, 0777);
      }
      // if the command is to be executed in background and does not have output redirection already
      // gets file descriptor for dev/null
      else {
        new_output = open("/dev/null", O_WRONLY);
      }
      // makes sure open worked, if not prints error
      if (new_output < 0) {
        fprintf(stderr, "Could not open %s for output\n", in->outstream);
        exit(1);
      }
      // redirects output
      dup2(new_output, STDOUT_FILENO);
    }
    
    // exec function to run the command with arguments
    // checks if error has occured and prints it
    if (execvp(in->args[0], in->args) == -1) {
      perror("Issue executing command");
      fflush(stderr);
      exit(1);
    }
    
    // if we are still in the child process an error has occured
    fprintf(stderr, "Issue exiting child process automatically. Manually exiting.\n");
    fflush(stderr);
    exit(1);
  }
  
  // case where we are in the parent process
  else {
    // waits if command is not to be executed in background
    if (!in->background) {
      // blocks signal of SIGTSTP until foreground process is complete
      sigset_t sigtstpset, priorset;
      // creates sigset_t containing just SIGSTP
      sigemptyset(&sigtstpset);
      sigaddset(&sigtstpset, SIGTSTP);
      // replaces current signal mask with sigstpset, stores current mask to be restored later
      sigprocmask(SIG_BLOCK, &sigtstpset, &priorset);

      // prepares modified signal handler for SIGTSTP
      struct sigaction new_sigtstp_action;
      // sets up new signal handler to handle SIGTSTP without printing the colon
      // case where background_capacity is currently enabled and will be disabled
      if (background_capacity) {
        memset(&new_sigtstp_action, 0, sizeof new_sigtstp_action);
        // we want to turn background capacity off if it is currently on
        new_sigtstp_action.sa_handler = background_off_nocol;
        sigfillset(&(new_sigtstp_action.sa_mask));
        // SA_RESTART lets certain functions continue if system call was deliverd in the middle of execution
        new_sigtstp_action.sa_flags = SA_RESTART;
      }

      // sets up new signal handler to handle SIGTSTP without printing the colon
      // case where background_capacity is currently disabled and will be enabled
      else {
        memset(&new_sigtstp_action, 0, sizeof new_sigtstp_action);
        // we want to turn background capacity on if it is currently off
        new_sigtstp_action.sa_handler = background_on_nocol;
        sigfillset(&(new_sigtstp_action.sa_mask));
        // SA_RESTART lets certain functions continue if system call was deliverd in the middle of execution
        new_sigtstp_action.sa_flags = SA_RESTART;
      }
      
      // enables new non-colon-printing signal behavior
      sigaction(SIGTSTP, &new_sigtstp_action, NULL);

      // waits for foreground child process to finish
      waitpid(fork_pid, &child_status, 0);

      // updates exit status if child exited normally
      if (WIFEXITED(child_status)) {
        *exitednormally = 1;
        *exitstatus = WEXITSTATUS(child_status);
      }

      // updates and prints exit status if child exited abnormally
      if (WIFSIGNALED(child_status)) {
        *exitednormally = 0;
        *exitstatus = WTERMSIG(child_status);
        fprintf(stdout, "terminated by signal %d\n", *exitstatus);
        fflush(stdout);
      }

      // unblocks signal and resets signal mask to the prior set of signals
      // if SIGTSTP was signaled should be acted upon here
      sigprocmask(SIG_SETMASK, &priorset, NULL);

      // resets SIGTSTP signal handler to default to include colon in output
      set_sigtstp_handler();
    }
    // executes command in background
    else {
      fprintf(stdout,"background pid is %jd\n", (intmax_t) fork_pid);
      fflush(stdout);
      // modify bp to store new background process
      bp->processes[bp->count] = fork_pid;
      bp->count++;
    }
  }
}



/*
* Main function that sets up variables and loops until exit is entered by the user
* Takes user input and executes commands
*/
int main() {

  // allocates heap space for string that will store command line input
  char* instring = malloc(sizeof (char) * (MAX_LINE_LEN + 1)); 

  // creates struct to store pointers to parts of the input
  struct inputparts in; 
  
  // creates struct to store the count and pids of background processes
  struct backgroundprocesses bp;
  memset(&bp, 0, sizeof bp);

  // int to store the exit status of the last foreground process
  int exitstatus = 0;
  // _Bool to store whether the last foreground process exited normally or was interrupted
  _Bool exitednormally = 1;

  // sets up handler for SIGTSTP
  set_sigtstp_handler();

  // sets up ignore for SIGINT
  struct sigaction sigint_action;
  memset(&sigint_action, 0, sizeof sigint_action);
  sigint_action.sa_handler = SIG_IGN;
  sigfillset(&(sigint_action.sa_mask));
  // SA_RESTART allows some functions to continue after being interrupted by a signal
  sigint_action.sa_flags = SA_RESTART;

  // activates signal handler
  sigaction(SIGINT, &sigint_action, NULL);

  // main loop of shell
  // prompts for input, processes it, and executes command
  while(1) {
    
    // checks to see if any background processes have terminated
    monitorbps(&bp);

    // clears the string that will hold command line input
    memset(instring, '\0', MAX_LINE_LEN + 1);

    // resets values of struct that stores information about input parts 
    memset(&in, 0, sizeof in);

    // prompt the user for input
    printf(":");
    fflush(stdout);

    // reads the user's input
    // gets MAX_LINE_LEN characters from standard input and puts them in instring
    fgets(instring, MAX_LINE_LEN + 1, stdin);

    // parses inputted string
    // handles case where string is empty or a comment by jumping to top of loop
    if (parseargs(instring, &in) == 1) {
      continue;
    }
    
    // expands arguments
    expandargs(&in);

    // implements exit command
    if (in.arg_count && !strcmp(in.args[0], "exit")) {
        // ends all backround child processes
        for (int i = 0; i < bp.count; i++) {
          if (kill(bp.processes[i], SIGTERM) == -1) {
            fprintf(stderr, "Could not terminate background pid %jd\n", (intmax_t) bp.processes[i]);
            fflush(stderr);
          }
        }
        // frees allocated memory for input string and processed arguments
        free(instring);
        for (int i = 0; i < in.arg_count; i++) {
          free(in.args[i]);
        }
        exit(0);
    }

    // implements cd command
    else if (in.arg_count && !strcmp(in.args[0], "cd")) {

      // if cd is entered by itself goes to the home directory in environment
      if (in.arg_count == 1) {

        // see if HOME is set in env, if so set it to homedir and change directory to it
        char *homedir = getenv("HOME");
        // handles if HOME is not found
        if (homedir == NULL) {
          fprintf(stderr, "no HOME variable in environment\n");
          fflush(stderr);
        }
        
        // changes dir to home
        else {
          chdir(homedir);
        }
      }

      // changes directory to passed-in value
      else if (in.arg_count && in.arg_count == 2) {
        // if chdir fails print error
        if (chdir(in.args[1])) {
          fprintf(stderr, "path not found\n");
          fflush(stderr);
        }
      }

      // if more than 2 arguments passed with cd does nothing
      else {
        fprintf(stderr, "Too many arguments provided for cd command\n");
        fflush(stderr);
      }
    }

    // implements status capacity
    else if (in.arg_count && !strcmp(in.args[0], "status")) {
      if (exitednormally) {
        printf("exit value %i\n", exitstatus);
        fflush(stdout);
      }
      else {
        printf("terminated by signal %i\n", exitstatus);
        fflush(stdout);
      }
    }

    // implements capacity to execute other commands
    else {
      execcommand(&in, &bp, &exitstatus, &exitednormally);
    }
    
    // free every non-null pointer in in.args before next command
    for (int i = 0; i < in.arg_count; i++) {
      free(in.args[i]);
    }
  }
}


