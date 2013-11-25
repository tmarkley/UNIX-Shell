// Shell program in C
//   Tommy Markley
//   October, 2013

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX 1000      // maximum buffer size for each line
#define MAXTOKS 25    // maximum number of tokens
#define MAXTOKSIZE 75 // maximum characters in a token

/* 
  @Name: command [struct]

  @members:
    char ** tok  An array of C-style strings that store each
                   token that the user enters after the prompt
    
    int count    This integer stores the number of tokens in tok
    
    int status   This integer stores a status_value to represent
                   the status of the user's input

  @Description:
    This struct is used for parsing through each line of input
    and sending commands to execute.
*/ 
struct command {
  char **tok;
  int count;
  int status;
};

// This enum is used for the 'status' member in the command
//   struct. The integer is set in read_command.
enum status_value {
  NORMAL, EOF_FOUND, INPUT_OVERFLOW, OVERSIZE_TOKEN, TOO_MANY_TOKENS
};

// These are global variables used in multiple functions 
int pipe_index; /* stores the location of the pipe character
		   in tok */
int num_paths;  /* stores the number of paths in the path
		   environment */
// strings used to compare user input for special functionality
const char exitKey[] = "exit";
const char changedir[] = "cd";
const char hack[] = "hack";
char telnet1[] = "/usr/bin/telnet";
char telnet2[] = "towel.blinkenlights.nl";

/*
  @Name: read_command
  
  @arguments:
    struct command *n  A pointer to a command object that is
                         filled in this function to be used
                         later to execute commands in tok

  @return:
    int                int describing success (1) or failure (0)

  @Description:
    The main functionality of read_command is to get the user
    input, parse it, and fill the members of the command
    object.

*/

int read_command(struct command *n) {
  char buff[MAX]; // the line is read into buff
  char *val; // used for return value of fgets
  
  // each time we read a new line we need to reallocate
  //   memory for tok and set count and pipe_index
  //   equal to zero
  n->tok = (char **) calloc((MAXTOKS+1),sizeof(char *));
  n->count = 0;
  pipe_index = 0;

  // fgets returns buff on success, and NULL on failure
  val = fgets(buff, MAX, stdin);

  /* 
     This loop parses through each character in buff and
     fills tok accordingly. 

     j is the index of buff
     t is the index of tok
     ti is the index of tok[t]

  */
  int j;
  int t = 0;
  int ti = 0;
  n->tok[0] = (char *) calloc((MAXTOKSIZE+1),sizeof(char));
  for (j=0; buff[j] != '\0'; j++) {
    // if the next character is not a space, copy it into tok
    if (!isspace(buff[j])) {
      // check if we have a pipe
      if (buff[j] == '|') {
	// keep track of which token the pipe character is
	pipe_index = t;
      }
      n->tok[t][ti] = buff[j];
      ti++;
    }
    /* 
       else if the next token has not been filled with any
       characters (and we're reading a space), then skip 
       it because it's just a space after the previous token
    */
    else if (ti == 0)
      continue;
    /* 
       else we're reading a space right after the token, so
       stop adding characters to the current token and move
       on to the next one
    */
    else {
      // make sure that we can add a nullbyte
      if ((++ti) < MAXTOKSIZE)
        n->tok[t][ti] = '\0';
      // if we can't then the token is too large
      else {
	n->status = OVERSIZE_TOKEN;
	return 0;
      }
      /* 
	 if we have room to read in another tok and the
	 next character in buff isn't the nullbyte, then
	 allocate memory for another token.
      */
      if ((t+1) < MAXTOKS && buff[j+1] != '\0') {
	n->tok[t+1] = (char *) malloc((MAXTOKSIZE+1) * sizeof(char));
      }
      /* 
	 else if we have read in too many tokens and there
	 is more input to be read, set status accordingly
      */
      else if ((t+1) == MAXTOKS && buff[j+1] != '\0') {
	n->status = TOO_MANY_TOKENS;
	return 0;
      }
      t++;
      ti = 0;
      n->count++;
    }
  }
  // if fgets returned null, we read an EOF character
  if (val == NULL) {
    n->status = EOF_FOUND;
    return 0;
  }
  // if the last character in buff wasn't a newline character, then
  //   the input was too large (more than MAX)
  else if (buff[(strlen(buff)-1)] != '\n') {
    n->status = INPUT_OVERFLOW;
    return 0;
  }
  // else everything was normal
  else {
    n->status = NORMAL;
    return 1;
  }

}

/******************** EXTRA FEATURE ***********************

  @Name: getPaths

  @arguments:
    char *** paths  A pointer to the paths variable that
                    is called by reference.

    char * path     A C style string that holds the entire
                    string returned from getenv("PATH")

  @return:
    int             The number of possible paths

  @Description:
    getPaths parses through the path string and puts the
    result of each possible path in paths

***********************************************************/
int getPaths(char ***paths, char *path) {
  int count = 0;
  // buffer is used for the separate paths
  char *buffer = (char *) malloc(25 * sizeof(char));

  // This loop parses through each character and
  //   determines where the individual paths are
  // i is index of path
  // bi is index of buffer
  // pi is index of (*paths)
  int i=0, bi=0, pi=0;
  while (path[i] != '\0') {
    // if the next character is a colon, move to next path
    if (path[i] == ':') {
      buffer[++bi] = '\0';
      (*paths)[pi++] = buffer; // add the path to paths
      buffer = (char *) malloc(50 * sizeof(char));
      bi = 0;
      i++;
      count++;
    }
    // else copy the next character into the buffer
    else {
      buffer[bi++] = path[i++];
    }
  }
  // make sure we add the last path to paths
  buffer[++bi] = '\0';
  (*paths)[pi++] = buffer;
  count++;

  return count;
}

/*
  rightPipe and leftPipe determine which tokens are to
  the right and left of the pipe character and then returns
  an array of C style strings with those tokens in them
*/

char **rightPipe(char **tok, int num) {

  int count = num - (pipe_index+1); // number of toks to the right of pipe
  char **cmd = (char **) malloc(count * sizeof(char *));
  int i, cmd_i = 0, tok_i = 0;
  for ( i = 0; i < count; i++, cmd_i = 0, tok_i = 0) {
    cmd[i] = (char *) malloc(50 * sizeof(char));
    while(tok[pipe_index+1+i][tok_i] != '\0')
      cmd[i][cmd_i++] = tok[pipe_index+1+i][tok_i++];
    cmd[i][cmd_i] = '\0';
  }

  return cmd;
}

char **leftPipe(char **tok) {

  int count = pipe_index; // number of toks to the left of pipe
  char **cmd = (char **) malloc(count * sizeof(char *));
  int i, cmd_i = 0, tok_i = 0;
  for ( i = 0; i < count; i++, cmd_i = 0, tok_i = 0 ) {
    cmd[i] = (char *) malloc(MAXTOKSIZE * sizeof(char));
    while(tok[i][tok_i] != '\0')
      cmd[i][cmd_i++] = tok[i][tok_i++];
    cmd[i][cmd_i] = '\0';
  }

  return cmd;
}

/*

  @Name: execCmd

  @arguments:
    char ** paths  An array of C style strings that are all
                   of the possible paths.

    char ** tok    An array of C style strings that hold all
                   of the tokens that the user enters

  @Description:
    This function appends the command entered by the user to
    each possible path and trys to execute it.

*/

void execCmd(char **paths, char **tok) {

  char *cmd = (char *) calloc(MAXTOKSIZE+25,sizeof(char));
  int i,cmd_i = 0, tok_i = 0, path_i = 0;
  
  for (i = 0; i < num_paths; i++, cmd_i = 0, tok_i = 0, path_i = 0) {
    while (paths[i][path_i] != '\0')
      cmd[cmd_i++] = paths[i][path_i++];
    cmd[cmd_i++] = '/';

    while (tok[0][tok_i] != '\0')
      cmd[cmd_i++] = tok[0][tok_i++];
    cmd[cmd_i] = '\0';
    execve(cmd,tok,0);
    // if execve is a success, the program doesn't continue
    cmd = (char *) calloc(MAXTOKSIZE+25,sizeof(char));
  }
}

/******************** EXTRA FEATURE **************************

  @Name: run_pipe

  @arguments:
    struct command * c  A pointer to the command variable, used
                        to send the tokens and number of tokens
			to rightPipe and leftPipe

    char ** paths       An array of C style strings that hold
                        all of the paths in the environment

  @Description:
    This function pipes the commands on each side of the pipe
    character. It sets the file descriptors in A and executes
    the two commands. The first child executes what's on the
    left of the pipe and second child executes what's on the
    right o the pipe.

*************************************************************/

void run_pipe(struct command *c, char **paths) {

  int A[2];   // file descriptors
  int status; // status used for waiting
  int pid;    // process id

  pipe(A);

  if ( (pid=fork()) == 0) {
    //Child
    char **cmd = leftPipe(c->tok);
    
    dup2(A[1],1);

    close(A[0]);
    close(A[1]);

    execCmd(paths, cmd);
    // if the child didn't successfully execute the command,
    //   there was an error
    perror("Error: Pipe in");
    exit(1);
  }
  // if fork returns -1, there was an error
  else if (pid < 0) {
    perror("Error forking in pipe");
    exit(EXIT_FAILURE);
  }

  //Fork second process
  if ( (pid=fork()) == 0) {
    //Second child
    char **cmd = rightPipe(c->tok, c->count);

    dup2(A[0], 0);

    close(A[0]);
    close(A[1]);

    execCmd(paths, cmd);
    // if we get this far, there was an error.
    perror("Error: Pipe out");
    exit(1);
  }
  // if fork returns -1, there was an error
  else if (pid < 0) {
    perror("Error forking second process");
    exit(EXIT_FAILURE);
  }

  close(A[0]);
  close(A[1]);

  wait(&status);
  wait(&status);

}

/*

  @Name: run_cmd

  @arguments:
    char ** tok    An array of C style strings that holds all 
                   of the tokens the user entered

    char ** paths  An array of C style strings that holds all
                   of paths in the environment

  @Description:
    If the user doesn't enter a pipe character, we execute only
    one command (tok[0]).

*/

void run_cmd(char **tok, char **paths) {
  int pid = fork();
  int status;

  //Fork, check which process is returned
  // if pid returns -1, it's an error
  if (pid < 0) {
    perror("fork error.");
    exit(EXIT_FAILURE);
  }
  else if ( pid == 0 ) {
    //Child Process
    
    // this is a fun Easter egg that calls telnet if you type 'hack'
    if (!strcmp(tok[0],hack)) {
      tok[0] = telnet1;
      tok[1] = telnet2;
      execve(tok[0],tok,0);
      perror("telnet error\n");
    }
    // otherwise we execute the command normally
    else {
      execCmd(paths, tok);
      printf("%s: Command not found.\n", tok[0]);
      exit(1);
    }
  }
  // parent process
  else {
    //************* EXTRA FEATURE **********************
    // if the user types "cd" then we need to handle that
    if (!strcmp(tok[0],changedir))
      chdir(tok[1]);
    wait(0);
  }
}

int main(int argc, char **argv, char **envp) {

  char *path = getenv("PATH");
  char **paths = (char **) malloc(50 * sizeof(char *));
  char * username = getenv("USER");
  num_paths = getPaths(&paths, path);

  struct command Tommy;

  printf("\nWelcome to Lion Shell, %s! Type 'exit' to quit.\n",username);

  while(1) {

    printf("%s%% ", username);

    int read = read_command(&Tommy);
    // if the user hit enter, try again
    if (!Tommy.tok[0][0])
      continue;
    // if read_command returned something other than 1,
    // tell the user what happened
    else if (!read) {
      switch(Tommy.status) {
      case(EOF_FOUND):
	printf("\nError: EOF_FOUND\n");
	break;
      case(INPUT_OVERFLOW):
	printf("Error: INPUT_OVERFLOW\n");
	break;
      case(OVERSIZE_TOKEN):
	printf("Error: OVERSIZE_TOKEN\n");
	break;
      case(TOO_MANY_TOKENS):
	printf("Error: TOO_MANY_TOKENS\n");
	break;
      default:
	printf("Unknown Error!\n");
      }
    }
    // if the user types 'exit', exit the program
    else if (!strcmp(Tommy.tok[0],exitKey))
      _exit(1);
    // else continue and try to execute what they typed
    else {
      if (pipe_index)
	run_pipe(&Tommy, paths);
      
      else
	run_cmd(Tommy.tok, paths);
    }
  }
  
  return 0;
}
