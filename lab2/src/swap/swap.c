#include "swap.h"

void Swap(char *left, char *right)
{
	char ch3;
	ch3 = *left;
	*left = *right;
	*right = ch3;


}
