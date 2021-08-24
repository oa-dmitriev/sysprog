#pragma once
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

enum State{singleQuote='\'', doubleQuote='\"', Outside=-1};

void changeStateIfQuote(const char c, enum State* state) {
  //only if we get out of the quotation
  if (c == *state) {    
    *state = Outside;  
    return;
  }

  //first quote mark
  if (*state == Outside && (c == singleQuote || c == doubleQuote)) {
    *state = c;
  }
  return;
}

// Cleans s according to Bash grammar rules for future use in execvp.
void cleanString(char* s) {
  enum State state = Outside;
  size_t i = 0;
  while (i < strlen(s)) {
    changeStateIfQuote(s[i], &state);
    if (state == Outside) {
      for (; i < strlen(s); ++i) {
        if (s[i] == '\\') {
          if (s[i+1] == '\n') {
            memmove(s + i, s + i + 2, strlen(s + i + 1));
          } else {
            memmove(s + i, s + i + 1, strlen(s + i)); 
          }
          continue;
        }
        changeStateIfQuote(s[i], &state);
        if (state != Outside) {
          break;
        }
      }
    } else if (state == doubleQuote) {
      memmove(s + i, s + i + 1, strlen(s + i));
      for (; i < strlen(s); ++i) {
        if (s[i] == '\\') {
          if (s[i+1] == '\\') {
            memmove(s + i, s + i + 1, strlen(s + i));
          } else if (s[i+1] == '\n') {
            memmove(s + i, s + i + 2, strlen(s + i + 1));
          } else if (s[i+1] == '\"') {
            memmove(s + i, s + i + 1, strlen(s + i)); 
          }
          continue;
        }
        changeStateIfQuote(s[i], &state);
        if (state != doubleQuote) {
          break;
        }
      }
      memmove(s + i, s + i + 1, strlen(s + i));
    } else if (state == singleQuote) {
      memmove(s + i, s + i + 1, strlen(s + i));
      for (; i < strlen(s) && state == singleQuote; ++i) {
        changeStateIfQuote(s[i], &state);
        if (state != singleQuote) {
          break;
        }
      }
      memmove(s + i, s + i + 1, strlen(s + i));
    }
  }
}

bool isSpace(const char c) {
  return c == ' ' || c == '\t';
}

typedef bool (*func)(const char);

// Strtok splits the string s at each run of ASCII code points c satisfying func(c). 
// Delimiters are also returned except for the whitecpace characters.
// Returned tokens are processed to follow Bash (Bourne Again Shell) grammar rules.
// Doesn't modify the origin string s. Each returned token must be freed. 
char* Strtok(const char* s, func delim) {
  static const char* input = NULL;
  if (s != NULL) {
    input = s;
  }
  if (input == NULL) {
    return NULL;
  }
  size_t fieldStart = 0;
  size_t i = 0;
  while (i < strlen(input) && isSpace(input[i])) {
    ++i; 
  }
  fieldStart = i;

  if (delim(input[i])) {
    char* res = malloc(3);
    if (input[i] == input[i+1]) {
      memmove(res, input + i, 2);
      res[2] = '\0';
      input += i + 2;
    } else {
      memmove(res, input + i, 1);
      res[1] = '\0';
      input += i + 1;
    }
    return res;
  }

  enum State state = Outside;
  for (; i < strlen(input); ++i) {
    if (input[i] == '\\' && (state == Outside || state == doubleQuote)) {
      ++i;
      continue;
    }
    if (state == Outside) {
      if (delim(input[i])) {
        size_t n = i - fieldStart;
        char* res = malloc(n + 1);
        memmove(res, input + fieldStart, n);
        res[n] = '\0';
        cleanString(res);
        input += i;
        return res;
      }
    }
    changeStateIfQuote(input[i], &state);
  }

  if (fieldStart < strlen(input)) {
    size_t n = i - fieldStart;
    char* res = malloc(n + 1);
    memmove(res, input + fieldStart, n);
    res[n] = '\0';
    cleanString(res);
    input = NULL;
    return res;
  }
  input = NULL;
  return NULL; 
}
