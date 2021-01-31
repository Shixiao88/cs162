#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_stdIn(struct tokens *tokens);
int cmd_stdOut(struct tokens *tokens);
int cmd_exec(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_pwd, "pwd", "display the current directory"},
  {cmd_cd, "cd", "change the directory"},
  {cmd_stdIn, ">", "redirect input"},
  {cmd_stdOut, "<", "redirect output"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

/* Display the current directory */
int cmd_pwd(unused struct tokens *tokens) {
  if (tokens_get_length(tokens) != 1) {
    printf("invalid arguments number for pwd, require 0\n");
    return 0;
  }
  char buf[80];
  getcwd(buf, sizeof(buf));
  printf("current working directory : %s\n", buf);
  return 1;
}

/* Change to the  directory */
int cmd_cd(unused struct tokens *tokens) {
  if (tokens_get_length(tokens) != 2) {
    printf("invalid arguments number for cd, require 1\n");
    return 0;
  }
  int error = chdir(tokens_get_token(tokens, 1));
  if (error) {
    perror("CD");
  } else {
    char buf[80];
    getcwd(buf, sizeof(buf));
    printf("change to working directory : %s\n", buf);
  } 
  return error;
}

/* Redirect input ">" */
int cmd_stdIn(unused struct tokens *tokens) {
  if (tokens_get_length(tokens) < 3) {
    printf("invalid arguments number for pwd, require 3 or more\n");
    return 0;
  }

  int token_length = tokens_get_length(tokens);
  char *slice = (char *) malloc(token_length * 2 + 1);
  for (int i = 0; i < token_length; i++) {
    char *v = tokens_get_token(tokens, i);
    if (strcmp(v, ">") == 0) {
      break;
    } else {
      strcat(slice, " ");
      strcat(slice, v);
    }
  }

  int stdout_copy = dup(1);
  close(1);
  int output_file = open(tokens_get_token(tokens, token_length - 1),
			 (O_WRONLY | O_CREAT | O_APPEND));
  pid_t cpid = fork();
  if (cpid == -1) {
    perror("fork");
  }
  
  if (cpid == 0) {
    setpgrp();
    cmd_exec(tokenize(slice));
    exit(0);
  } else {
    setpgid(cpid, cpid);
    wait(NULL);
    close(output_file);
    dup2(stdout_copy, 1);
    close(stdout_copy);
    return 1;
  }
}

/* Redirect output "<" */
int cmd_stdOut(unused struct tokens *tokens) {
  if (tokens_get_length(tokens) < 3) {
    printf("invalid arguments number for redirection, require more than 3\n");
    return 0;
  }
  
  int token_length = tokens_get_length(tokens);
  char *slice = (char *) malloc(token_length * 2 + 1);
  for (int i = 0; i < token_length; i++) {
    char *v = tokens_get_token(tokens, i);
    if (strcmp(v, "<") == 0) {
      break;
    } else {
      strcat(slice, " ");
      strcat(slice, v);
    }
  }

  int stdin_copy = dup(0);
  close(0);
  int outin_file = open(tokens_get_token(tokens, token_length - 1),
			(O_RDONLY));
  pid_t cpid = fork();
  if (cpid == -1) {
    perror("fork");
  }
  
  if (cpid == 0) {
    setpgrp();
    cmd_exec(tokenize(slice));
    exit(0);
  } else {
    setpgid(cpid, cpid);
    wait(NULL);
    close(outin_file);
    dup2(stdin_copy, 0);
    close(stdin_copy);
    return 1;
  }
}

void put_process_in_foreground (pid_t pid, pid_t pgid) {
  /* Put the job into the foreground.  */
  if (tcsetpgrp (shell_terminal, pgid) == -1) {
    perror("failt to set foreground in child process");
  }

  signal (SIGINT, SIG_DFL);
  signal (SIGQUIT, SIG_DFL);
  signal (SIGTSTP, SIG_DFL);
  signal (SIGTTIN, SIG_DFL);
  signal (SIGTTOU, SIG_DFL);
  signal (SIGCHLD, SIG_DFL);
}

/* Execute program */
int cmd_exec(struct tokens *tokens) {
    
  pid_t cpid = fork();
  if (cpid == -1) {
    perror("fork");
  }
  
  if (cpid == 0) {
    setpgrp();
    pid_t my_pid = getpid();
    put_process_in_foreground(my_pid, my_pid);
    
    size_t len = tokens_get_length(tokens);
    char *argv[len] ;
    for (int i = 0; i < len; i++) {
      argv[i] = tokens_get_token(tokens, i);
    }
    argv[len] = NULL;
    
    if (strchr(argv[0], '/') != NULL) {
      int res = execv(argv[0], argv);
      if (res == -1) perror("Error: ");
      exit(0);
      //return res;
    } else {
      char *dup = strdup(getenv("PATH"));
      char *s = dup;
      char *p = NULL;
      int result;
      do {
	p = strchr(s, ':');
	if (p != NULL) {
	  p[0] = 0;
	}
	char * fullPath = (char *) malloc(5 + strlen(s) + strlen(argv[0]));
	strcpy(fullPath, s);
	strcat(fullPath, "/");
	strcat(fullPath, argv[0]);
	result = execv(fullPath, argv);
	if (result != -1) {
	  exit(0);
	}
	s = p + 1;
      } while (p != NULL || result == 0);
      
    }

    /* Put the shell back in the foreground.  */

    if (tcsetpgrp (shell_terminal, shell_pgid) == -1) {
      perror("tcsetpgrp failed in child!");
      
    }

    /* Restore the shell¡¯s terminal modes.  */
    tcsetattr (shell_terminal, TCSADRAIN, &shell_tmodes);

    exit(0);
  } else {
    setpgid(cpid, cpid);

    if (tcsetpgrp(shell_terminal, cpid) == -1) {
      printf("tcsetpgrp failed in parent! error: %d\n", errno);
      tcsetpgrp (shell_terminal, shell_pgid);
      tcsetattr (shell_terminal, TCSADRAIN, &shell_tmodes);
      exit(1);
    }
    
    wait(&cpid);                /* Wait for child */
  }
  return 0;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

void termination_handler (int signum) {
  if (signum == SIGCONT) {
    printf("come to signal countinue for the shell\n");
  }
  if (signum == SIGTTIN) {
    pid_t currpgrp = getpgrp();
    
    printf("come to signal int for the shell\ncurrent group: %d\nshell group id: %d\n",
	   currpgrp, shell_pgid);

    wait(NULL);
  }
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    signal (SIGINT, SIG_IGN);
    signal (SIGQUIT, SIG_IGN);
    signal (SIGTSTP, SIG_IGN);
    signal (SIGTTIN, SIG_IGN);
    signal (SIGTTOU, SIG_IGN);
    signal (SIGCHLD, SIG_IGN);
    
    /* Saves the shell's process id */
    shell_pgid = getpid();
    if (setpgid (shell_pgid, shell_pgid) < 0) {
      perror ("Couldn't put the shell in its own process group");
      exit (1);
    }

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}


int main(unused int argc, unused char *argv[]) {
  init_shell();
  
  
  static char line[4096];
  int line_num = 0;
  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);
    int lookUpIndex = 0;

    int token_length = tokens_get_length(tokens);
    if (token_length >= 3) {
      for (int i = 0; i < token_length; i++) {
	char *v = tokens_get_token(tokens, i);
	if (strcmp(v, ">") == 0 || strcmp(v, "<") == 0) {
	  lookUpIndex = i;
	}
      }
    }

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, lookUpIndex));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
      cmd_exec(tokens);
      //fprintf(stdout, "This shell doesn't know how to run programs.\n");
    }

    if (shell_is_interactive) {
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

      /* Take control of the terminal */
      tcsetpgrp(shell_terminal, shell_pgid);
      
      /* Save the current termios to a variable, so it can be restored later. */
      tcgetattr(shell_terminal, &shell_tmodes);
    }

    /* Clean up memory */
    tokens_destroy(tokens);
  }
  return 0;
}
