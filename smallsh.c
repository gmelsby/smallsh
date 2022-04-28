// Name: Gregory Melsby
// OSU Email: melsbyg@oregonstate.edu
// Course: CS344 - Operating Systems / Section 400
// Assignment: Assignment 3: smallsh
// Due Date: 05/09/2022
// Description: Shell that can run basic commands

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>

// maximum number of characters the command line will read
const int MAX_LINE_LEN = 2048;
// maximum number of arguments the command line supports
const int MAX_ARGS = 512;


/*
*
*
*
*/
int execute_exit() {
  return 0;
}




/*
*
*
*
*/
int main() {

  // allocates heap space for string that will store command line input
  char* instring = malloc(sizeof (char) * (MAX_LINE_LEN + 1)); 

  // struct to store pointers to each part of the input
  struct inputparts {
    // stores a pointer to the command
    char *command;
    // stores array of pointers to arguments
    char** args;
    // stores pointer to string representing path of input stream
    char *instream;
    // stores pointer to string representing path of output stream
    char * outstream;
    // whether the command should run in the background
    _Bool background;
  };

  struct inputparts in; 

  while(1) {
    // clears the string that will hold command line inpuMt
    memset(instring, '\0', MAX_LINE_LEN + 1);

    // resets values of struct that stores information about input parts 
    memset(&in, 0, sizeof in);
    // allocates room for 2 pointers to arguments, this will get resized up to MAX_ARGS as necessary
    size_t args_size = sizeof (char*) * 2;
    in.args = malloc(args_size);

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
    if (instring[strlen(instring) - 1] == '&') {
      in.background = 1;
      instring[strlen(instring) - 1] = '\0';
    }

    // parses input with strtok
    // strips out spaces
    in.command = strtok(instring, " ");

    // if the line is empty or the first non-whitespace character is '#' we don't do anything else with the input
    if (in.command == NULL || *in.command == '#') {
      continue;
    }

    // setup for while loop
    char *nextstr = strtok(NULL, " ");
    int arg_index = 0;
    
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
        if (arg_index >= MAX_ARGS) {
          fprintf(stderr, "Too many arguments passed in. Some will not be parsed.");
          fflush(stderr);
          break;
        }

        // otherwise we add the argument string to the array in struct in and increment arg_index
        // we resize the array args if necessary
        if (sizeof (char*) * arg_index == args_size) {
          in.args = realloc(in.args, args_size * 2);
          args_size *= 2;
        }
        in.args[arg_index] = nextstr;
        arg_index++;
      }

      // gets a pointer to the next string
      nextstr = strtok(NULL, " ");
    }

    // print diagnostics
    fprintf(stderr, "COMMAND: %s\n", in.command);
    for (int i=0;i<arg_index;i++) {
      fprintf(stderr, "ARG %d: %s\n", i, in.args[i]);
    }
    fprintf(stderr, "INPUT: %s\n", in.instream);
    fprintf(stderr, "OUTPUT: %s\n", in.outstream);




    // Array of pointers to expanded strings
    // size is MAX_ARGS + 1 because the command and every argument can be expanded 
    char *expanded[MAX_ARGS + 1];
    int expanded_count = 0;



    // free the memory for all the expanded strings
    for (int i = 0; i < expanded_count; i++) {
      free(expanded[i]);
    }
  }
  
  free(instring);
}


