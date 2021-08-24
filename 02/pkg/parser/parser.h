#include "../strings/strings.h"

typedef enum Type{Command, Operator} Type;

typedef struct Cmd {
  Type type;
  char* command;
  char**  argv;
  size_t argc;
} Cmd;

// Reads entire line from stream, storing the address of the buffer into *lineptr. 
// Continues reading if the newline character was met inside double or single quotes or followed by backslash character.
ssize_t getRawCmdLine(char** lineptr, FILE* stream) {
  ssize_t cmdSize = 1;
  if (*lineptr == NULL) {
    *lineptr = malloc(cmdSize);
  }
  (*lineptr)[0] = '\0';

  enum State state = Outside;
  bool next;
  do {
    next = false;
    char* line = NULL;
    size_t n = 0;
    ssize_t len = getline(&line, &n, stream);
    if (len == -1) {
      free(*lineptr);
      free(line);
      *lineptr = NULL;
      return len;
    }
    for (int i = 0; i < len; ++i) {
      char c = line[i];
      if (c == '\\' && (state == Outside || state == doubleQuote)) {
        ++i;
        continue;
      }
      changeStateIfQuote(line[i], &state); 
    }
    if (state != Outside || (len > 1 && line[len-2] == '\\')) {
      next = true; 
    } 
    cmdSize += len;
    char* newCmdLine = realloc(*lineptr, cmdSize);
    if (newCmdLine == NULL) {
      free(*lineptr);
      *lineptr = NULL;
      return -1;
    }
    *lineptr = newCmdLine;
    strcat(*lineptr, line);
    free(line);
  } while (next);

  size_t len = strlen(*lineptr);
  if((*lineptr)[len - 1] == '\n') {
    (*lineptr)[len-1] = '\0';
  }
  return strlen(*lineptr);
}

bool isOperator(const char c) {
  return c == '|' || 
         c == '&' ||
         c == '>' ||
         c == ' ' ||
         c == '&' ||
         c == '#';
}

ssize_t cmdResize(Cmd** p, size_t* cap) {
  size_t newCap = (*cap + 1) * 2;
  size_t newSize = newCap * sizeof(Cmd);
  Cmd* np = realloc(*p, newSize);
  if (np == NULL) {
    return -1; 
  }
  *p = np;
  *cap = newCap;
  return 0;
}

ssize_t strResize(char*** p, size_t* cap) {
  size_t newCap = (*cap + 1) * 2;
  size_t newSize = newCap * sizeof(char*);
  char** np = realloc(*p, newSize);
  if (np == NULL) {
    return -1; 
  }
  *p = np;
  *cap = newCap;
  return 0;
}

void cmdFree(Cmd* cmd, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    free(cmd[i].command);
    for (size_t j = 1; j < cmd[i].argc; ++j) {
      free((cmd[i].argv)[j]);
    }
    free(cmd[i].argv);
  }
  free(cmd);
}

ssize_t cmdFill(Cmd* cmd, char** cmdToken, Type type) {
  char* token = *cmdToken;
  cmd->type = type;
  cmd->command = token; 
  cmd->argv = NULL;
  cmd->argc = 0;

  char** p = NULL;
  if (type == Command) {
    size_t count = 0, cap = 0;
    ssize_t err = strResize(&p, &cap);
    if (err == -1) {
      return err;
    }
    p[count++] = token;

    token = Strtok(NULL, isOperator);
    while (token != NULL && !isOperator(*token)) {
      if (count + 1 > cap) {
        err = strResize(&p, &cap);
        if (err == -1) {
          for (size_t j = 0; j < count; ++j) {
            free(p[j]);
          }
          free(p);
          return err;
        }
      }
      p[count++] = token;
      token = Strtok(NULL, isOperator); 
    }
    if (count + 1 > cap) {
      err = strResize(&p, &cap);
      if (err == -1) {
        for (size_t j = 0; j < count; ++j) {
          free(p[j]);
        }
        free(p);
        return err;
      }
    }
    p[count] = NULL;
    cmd->argc = count;
    cmd->argv = p;
  } else {
    token = Strtok(NULL, isOperator); 
  }
  *cmdToken = token;
  return 0;
}

ssize_t getCmds(Cmd** cmds, FILE* stream) {
  char* rawCmdLine = NULL;
  ssize_t len = getRawCmdLine(&rawCmdLine, stream);
  if (len == -1 || len == 0) {
    free(rawCmdLine);
    return -1;
  }

  Cmd* lcmds = NULL;
  size_t count = 0, cap = 0;

  char* token = Strtok(rawCmdLine, isOperator);
  while (token != NULL) {
    if (count + 1 > cap) {
      ssize_t err = cmdResize(&lcmds, &cap);
      if (err == -1) {
        cmdFree(lcmds, count);
        free(rawCmdLine);
        return err;
      }
    }
    if (*token == '#') {
      free(token);
      break; 
    }
    ssize_t err = 0; 
    if (isOperator(*token)) {
      err = cmdFill(&(lcmds[count]), &token, Operator); 
    } else {
      err = cmdFill(&(lcmds[count]), &token, Command); 
    }
    if (err == -1) {
      cmdFree(lcmds, count);
      free(token);
      free(rawCmdLine);
      return err;
    }
    ++count;
  }
  *cmds = lcmds;
  free(rawCmdLine);
  return count;
}
