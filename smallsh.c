// Name: Gregory Melsby
// OSU Email: melsbyg@oregonstate.edu
// Course: CS344 - Operating Systems / Section 400
// Assignment: Assignment 3: smallsh
// Due Date: 05/09/2022
// Description: Shell that can run basic commands

#include <stdlib.h> // malloc, realloc, free, exit, etc
#include <stdint.h> // intmax_t
#include <fcntl.h> // open, close
#include <stdio.h> // printf, fprintf, etc
#include <sys/types.h> // pid_t
#include <unistd.h> // getpid, fork
#include <string.h> // size_t, strlen, etc
#include <sys/wait.h> // waitpid
#include <signal.h> // sigset_t, kill, signal processing

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
volatile sig_atomic_t background_capacity = 1;

// prototype for background_on so background_off recognizes function when compling
void background_on(int signal);

/*
 * Signal handler to toggle off background process capacity with '&'
 *
 */
void background_off(int signal) {
  char* output = "\nEntering foreground-only mode (& is now ignored)\n:";
  background_capacity = 0;
  write(STDOUT_FILENO, output, 51);
  signal++;
  struct sigaction sigtstp_action;
  memset(&sigtstp_action, 0, sizeof sigtstp_action);
  sigtstp_action.sa_handler = background_on;
  sigfillset(&(sigtstp_action.sa_mask));
  sigtstp_action.sa_flags = SA_RESTART;

  sigaction(SIGTSTP, &sigtstp_action, NULL);

}

/*
 * Signal handler to toggle on background process capacity with '&'
 *
 */ 
void background_on(int signal) {
  char* output = "\nExiting foreground-only mode\n:";
  background_capacity = 1;
  write(STDOUT_FILENO, output, 31);
  signal++;
  struct sigaction sigtstp_action;
  memset(&sigtstp_action, 0, sizeof sigtstp_action);
  sigtstp_action.sa_handler = background_off;
  sigfillset(&(sigtstp_action.sa_mask));
  sigtstp_action.sa_flags = SA_RESTART;

  sigaction(SIGTSTP, &sigtstp_action, NULL);
}


/*
 * Argument parsing functionality can go here
 *
 *
 */
int parseargs(struct inputparts *in) {
  printf("%d", (*in).background);
  return(0);
}

/*
 * Expands "$$" if present in any of the arguments 
 * Even if no arguments are expanded, allocates new memory where the arguments will be stored
 * @param *in pointer to the inputparts struct containing information about the arguments
 * @returns 0 if successful
 */
int expandargs(struct inputparts *in) {
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

  return(0);
}



/*
* Main function that sets up variables and loops until exit is entered by the user
* Takes user input and executes commands
*
*/
int main() {

  // allocates heap space for string that will store command line input
  char* instring = malloc(sizeof (char) * (MAX_LINE_LEN + 1)); 

  // creates struct to store pointers to parts of the input
  struct inputparts in; 
  
  // creates struct to store the count and pids of background processes
  struct backgroundprocesses bp;
  memset(&bp, 0, sizeof bp);


  // stores the exit status of the last foreground process
  int exitstatus = 0;

  // sets up signal handler for SIGTSTP
  struct sigaction sigtstp_action;
  memset(&sigtstp_action, 0, sizeof sigtstp_action);
  sigtstp_action.sa_handler = background_off;
  sigfillset(&(sigtstp_action.sa_mask));
  sigtstp_action.sa_flags = SA_RESTART;

  sigaction(SIGTSTP, &sigtstp_action, NULL);

  // sets up ignore for SIGINT
  struct sigaction sigint_action;
  memset(&sigint_action, 0, sizeof sigint_action);
  sigint_action.sa_handler = SIG_IGN;
  sigfillset(&(sigint_action.sa_mask));
  sigint_action.sa_flags = SA_RESTART;

  sigaction(SIGINT, &sigint_action, NULL);

  while(1) {
    // checks the current background processes to see if they have terminated
    for (int i = 0; i < bp.count;) {
      pid_t child_pid = bp.processes[i];
      int background_status;
      if (waitpid(child_pid, &background_status, WNOHANG)) {
        if (WIFEXITED(background_status)) {
          printf("background pid %jd is done: exit value %d\n", (intmax_t) child_pid, WEXITSTATUS(background_status));
        }
        if (WIFSIGNALED(background_status)) {
          printf("background pid %jd is done: terminated by signal %d\n", (intmax_t) child_pid, WTERMSIG(background_status));
        }
        for (int j = i; j < bp.count - 1; j++) {
          bp.processes[j] = bp.processes[j+1];
        }
        bp.processes[bp.count - 1] = 0;
        bp.count--;
      }
      else {
        i++;
      }
    }
    // clears the string that will hold command line input
    memset(instring, '\0', MAX_LINE_LEN + 1);

    // resets values of struct that stores information about input parts 
    memset(&in, 0, sizeof in);
    

    // prompt the user for input
    printf(":");
    fflush(stdout);

    // reads the user's input
    // gets MAX_LINE_LEN characters from standard input and puts them in the allocted string
    fgets(instring, MAX_LINE_LEN + 1, stdin);

    // if the input is fully empty we loop again
    if (strlen(instring) == 0) {
      printf("\n");
      continue;
    }

    // checks if the last character is a newline
    // if so replaces it with a null terminator
    if (instring[strlen(instring) - 1] == '\n') {
      instring[strlen(instring) - 1] = '\0';

      // if the input was just a new line we loop again
      if (strlen(instring) == 0) {
        continue;
      }
    }

    // checks if the last character is '&'
    // if so sets the command to run in background
    // we then move the null terminator one char earlier to replace '&'
    if (strlen(instring) > 1 && instring[strlen(instring) - 1] == '&' && instring[strlen(instring) - 2] == ' ') {
      // sets command to run in background if background_capacity is enabled
      if (background_capacity) {
        in.background = 1;
      }
      // removes the '& even if background_capacity is not on
      instring[strlen(instring) - 1] = '\0';
    }

    // parses input with strtok
    // strips out spaces
    in.args[0] = strtok(instring, " ");

    // if the line is empty or the first non-whitespace character is '#' we don't do anything else with the input
    if (in.args[0] == NULL || *in.args[0] == '#') {
      continue;
    }

    // setup for while loop
    char *nextstr = strtok(NULL, " ");
    in.arg_count += 1;
    
    // parses the input string with strtok until it returns null
    while (nextstr != NULL) {
      // checks if the next part of input is just '<', if so puts pointer to string after that one in in.instream
      if (*nextstr =='<' && nextstr[1] == '\0') {
        in.instream = strtok(NULL, " ");
      }

      // checks if the next part of input is just '>', if so puts pointer to string after that one in in.instream
      else if (*nextstr == '>' && nextstr[1] == '\0') {
        in.outstream = strtok(NULL, " ");
      }

      else {
        // loop exits if there are more arguments than can be stored
        if (in.arg_count == 512 + 1) {
          fprintf(stderr, "Too many arguments passed in. Some will not be parsed.\n");
          fflush(stderr);
          break;
        }

        // otherwise we add the argument string to the array in struct in and increment in.arg_count
        // we resize the args array if necessary
        in.args[in.arg_count] = nextstr;
        in.arg_count++;
      }

      // gets a pointer to the next string
      nextstr = strtok(NULL, " ");
    }

    // null terminate the args array
    in.args[in.arg_count] = (char*) NULL;

    // implement string expansion
    expandargs(&in);
    

    // implements exit command
    // needs to end child processes when those are implemented
    if (in.arg_count && !strcmp(in.args[0], "exit")) {
        for (int i = 0; i < bp.count; i++) {
          if (kill(bp.processes[i], SIGTERM) == -1) {
            fprintf(stderr, "Could not terminate background pid %jd\n", (intmax_t) bp.processes[i]);
          }
        }
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
        if (homedir == NULL) {
          fprintf(stderr, "no HOME variable in environment\n");
          fflush(stderr);
        }
        
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
        fprintf(stderr, "Too many arguments provided for cd comamand\n");
        fflush(stderr);
      }
      
    }

    // implement status capacity
    else if (in.arg_count && !strcmp(in.args[0], "status")) {
      fprintf(stdout, "exit value %i\n", exitstatus);
      fflush(stdout);
    }

    // implement exec capacity
    else {
      // checks that there will not be more than 30 background processes
      if (in.background && bp.count >= 30) {
        fprintf(stderr, "Too many background processes already! Try again when one has terminated.\n");
        continue;
      }

      // forks to create a child process
      pid_t fork_pid = fork(); 
      int child_status;
      // if fork failed
      if (fork_pid < 0) {
        fprintf(stderr, "Issue making fork\n");
        fflush(stderr);
      }

      // case where we are in the child process
      else if (fork_pid == 0) {
        // sets up ignore for SIGTSTP
        sigtstp_action.sa_handler = SIG_IGN;
        sigfillset(&(sigtstp_action.sa_mask));
        sigtstp_action.sa_flags = SA_RESTART;

        sigaction(SIGTSTP, &sigtstp_action, NULL);

        // if command is to be run in foreground, sets up default behavior for SIGINT
        if (!in.background) {
          sigint_action.sa_handler = SIG_DFL;
          sigfillset(&(sigint_action.sa_mask));
          sigint_action.sa_flags = SA_RESTART;

          sigaction(SIGINT, &sigint_action, NULL);
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
        if (in.instream != NULL || in.background) {
          // case where input redirection is passed in
          if (in.instream != NULL) {
            new_input = open(in.instream, O_RDONLY);
          }

          // if the command is to be executed in background and does not have input redirection already, gets file descriptor for dev/null
          else {
            new_input = open("/dev/null", O_RDONLY);
          }
          // makes sure open worked, if not prints error
          if (new_input < 0) {
            fprintf(stderr, "Could not redirect input\n");
            exit(1);
          }
          // redirects input
          dup2(new_input, STDIN_FILENO);
        }

        // handles ouptut redirection
        if (in.outstream != NULL || in.background) {
          // case where output redirection is passed in
          if (in.outstream != NULL) {
          // creates new file if the output file does not exist, otherwise overwrites existing file
          new_output = open(in.outstream, O_WRONLY | O_CREAT | O_TRUNC, 0777);
          }
          // if the command is to be executed in background and does not have output redirection already, gets file descriptor for dev/null
          else {
            new_output = open("/dev/null", O_WRONLY);
          }
          // makes sure open worked, if not prints error
          if (new_output < 0) {
            fprintf(stderr, "Could not redirect output\n");
            exit(1);
          }
          // redirects output
          dup2(new_output, STDOUT_FILENO);
        }
       
        // exec function to run the command with arguments
        execvp(in.args[0], in.args); 
        
        // if we are still in the child process an error has occured
        fprintf(stderr, "Command could not be executed\n");
        exit(1);
      }
      
      // case where we are in the parent process
      else {
        // waits if command is not to be executed in background
        if (!in.background) {
          waitpid(fork_pid, &child_status, 0);

          // updates exit status if child exited normally
          if (WIFEXITED(child_status)) {
            exitstatus = WEXITSTATUS(child_status);
          }

          // updates and prints exit status if child exited abnormally
          if (WIFSIGNALED(child_status)) {
              exitstatus = WTERMSIG(child_status);
              fprintf(stdout, "%d\n", exitstatus);
              fflush(stdout);
          }

        }
        // executes command in background
        else {
          fprintf(stdout,"background pid is %jd\n", (intmax_t) fork_pid);
          fflush(stdout);
          bp.processes[bp.count] = fork_pid;
          bp.count++;
        }
      }
    }
    
    // free every non-null pointer in in.args
    for (int i = 0; i < in.arg_count; i++) {
      free(in.args[i]);
    }
  }
}


