#include "find_min_max.h"

#include <limits.h>

struct MinMax GetMinMax(int *array, unsigned int begin, unsigned int end) {
  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;
  for (unsigned int i = begin; i < end; i++){
    int val = array[i];
    if (min_max.min > val){
      min_max.min = val;
    }
    if(min_max.max < val){
      min_max.max = val;
    }
  }

  return min_max;
}
