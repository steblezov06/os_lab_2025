#include "sum.h"
#include <stdio.h>
#include <stdlib.h>


long Sum(const struct SumArgs *args) {
  long sum = 0;
  for (int i = args->begin; i < args->end; i++) {
    sum += args->array[i];
  }
  return sum;
}