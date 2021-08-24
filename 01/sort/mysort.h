#pragma once
struct Array { 
  int* a; 
  size_t size; 
}; 

struct SortLocalVariables { 
  int* a;
  int n; 
  int pe; 
  size_t i, j; 
}; 

void mySort();
