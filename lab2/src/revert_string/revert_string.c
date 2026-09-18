#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "revert_string.h"


void RevertString(char *str)
{
	int len = strlen(str);
	for (int i = 0; i < len / 2; i++){
		char t = str[i];
		str[i] = str[len - i - 1];
		str[len - i - 1] = t;
	}
	

}

