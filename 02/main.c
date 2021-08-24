#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "./pkg/parser/parser.h"
#include "./pkg/strings/strings.h"

#include <sys/stat.h>
#include <fcntl.h>

void terminateIfError(const char* msg, int err) {
  if (err == -1) {
    perror(msg);
    exit(EXIT_FAILURE);
  }
}

void runCmd(Cmd* cmds, ssize_t n) {
  if (n < 1) {
    return;
  }
  if (strcmp(cmds->command, "cd") == 0) {
    int err = chdir(cmds->argv[1]);
    terminateIfError("cd", err);
    return;
  }
  if (strcmp(cmds[n-1].command, "&") == 0) {
    pid_t pid = fork();
    if (pid == 0) {
      runCmd(cmds, n-1);
      exit(0);
    }
    return;
  }
  int status = 0;
  int err = 0;
  int in = STDIN_FILENO;
  for (int i = 0; i < n; ++i) {
    if (i > 1 && strcmp(cmds[i-1].command, "||") == 0) {
      pid_t pid = fork();
      if (pid == 0) {
        if (strcmp(cmds[i-1].command, "|") == 0) {
          dup2(in, STDIN_FILENO);
        }
        err = execvp(cmds[i].command, cmds[i].argv);
        if (err == -1) {
          exit(EXIT_FAILURE);
        }
      }
      int locStatus;
      err = waitpid(pid, &locStatus, 0);
      terminateIfError("waitpid", err);
      if (WEXITSTATUS(status) == 1) {
        status = locStatus;
      }
      continue;
    }
    if(WEXITSTATUS(status) == 1 && strcmp(cmds[i].command, "||") != 0) {
      return;
    };
    if (i + 1 < n && strcmp(cmds[i+1].command, "|") == 0) {
      int fd[2];
      err = pipe(fd);
      terminateIfError("pipe", err);
      pid_t pid = fork();
      if (pid == 0) {
        err = close(fd[0]);
        terminateIfError("file", err);
        if (i > 1 && strcmp(cmds[i-1].command, "|") == 0) {
          dup2(in, STDIN_FILENO);
        }
        dup2(fd[1], STDOUT_FILENO);
        err = execvp(cmds[i].command, cmds[i].argv);
        if (err == -1) {
          exit(EXIT_FAILURE);
        }
      }
      err =  close(fd[1]);
      terminateIfError("file", err);
      err = waitpid(pid, &status, 0);
      terminateIfError("waitpid", err);
      if (in != STDIN_FILENO) {
        err = close(in);
        terminateIfError("file", err);
      }
      in = fd[0];
      ++i;
    } else if (i + 2 < n && strcmp(cmds[i+1].command, ">>") == 0) {
      int fd[2];
      err = pipe(fd);
      terminateIfError("pipe", err);
      pid_t pid = fork();
      if (pid == 0) {
        err = close(fd[0]);
        terminateIfError("file", err);
        if (i > 1 && strcmp(cmds[i-1].command, "|") == 0) {
          dup2(in, STDIN_FILENO);
        }
        dup2(fd[1], STDOUT_FILENO);
        err = execvp(cmds[i].command, cmds[i].argv);
        if (err == -1) {
          exit(EXIT_FAILURE);
        }
      } 
      err = close(fd[1]);
      terminateIfError("file", err);
      err = waitpid(pid, &status, 0);
      terminateIfError("waitpid", err);
      int filedes = open(cmds[i + 2].command, O_RDWR | O_CREAT | O_APPEND, 
              S_IWUSR | S_IRUSR);
      terminateIfError("file", filedes);
      size_t size;
      char buf[1024];
      while((size=read(fd[0], buf, 1024)) > 0) {
        err = write(filedes, buf, size);
        terminateIfError("write", err);
      }
      err = close(filedes);
      terminateIfError("file", err);
      i += 2;
    } else if (i + 2 < n && strcmp(cmds[i+1].command, ">") == 0) {
      int fd[2];
      err = pipe(fd);
      terminateIfError("pipe", err);
      pid_t pid = fork();
      if (pid == 0) {
        if (i > 1 && strcmp(cmds[i-1].command, "|") == 0) {
          dup2(in, STDIN_FILENO);
        }
        dup2(fd[1], STDOUT_FILENO);
        err = execvp(cmds[i].command, cmds[i].argv);
        if (err == -1) {
          exit(EXIT_FAILURE);
        }
      } 
      err = close(fd[1]);
      terminateIfError("file", err);
      err = waitpid(pid, &status, 0);
      terminateIfError("watipid", err);
      int filedes = open(cmds[i + 2].command, O_RDWR | O_CREAT | O_TRUNC, 
              S_IWUSR | S_IRUSR);
      terminateIfError("file", filedes);
      size_t size;
      char buf[1024];
      while((size=read(fd[0], buf, 1024)) > 0) {
        err = write(filedes, buf, size);
        terminateIfError("write", err);
      }
      err = close(filedes);
      terminateIfError("file", err);
      i += 2;
    } else if (strcmp(cmds[i].command, "&&") == 0 || 
        strcmp(cmds[i].command, "||") == 0) {
      continue;
    } else {
      pid_t pid = fork();
      if (pid == 0) {
        if (i > 1 && strcmp(cmds[i-1].command, "|") == 0) {
          dup2(in, STDIN_FILENO);
        }
        if (execvp(cmds[i].command, cmds[i].argv) == -1) {
          cmdFree(cmds, n);
          exit(EXIT_FAILURE);
        }; 
      }
      err = waitpid(pid, &status, 0);
      terminateIfError("waitpid", err);
    }
  }
  if (in != STDIN_FILENO) {
    err = close(in);
    terminateIfError("file", err);
  }
  while (waitpid(-1, &status, WNOHANG) > 0) {}
}

int main() {
  while(true) { 
    Cmd* cmds = NULL;
    ssize_t n = getCmds(&cmds, stdin);
    if (n == -1) {
      return 0;
    }
    runCmd(cmds, n);
    cmdFree(cmds, n);
  }
  return 0;
}
