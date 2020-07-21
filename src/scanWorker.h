#ifndef __SCANNER_WORKER__
#define __SCANNER_WORKER__
#include <stdio.h>
#include "csteg.h"

#define EOB_ENCOUNTERED 123
#define ZRL_ENCOUNTERED 124
#define END_OF_FILE_ENCOUNTERED 117

#define READ_MESSAGE_CODE_PROCESSOR 91  // For use with processBit()

#define IS_BIT(bit) (bit == 0 || bit == 1)

int scannerHideMessage(FILE*, jpegStats*, char*, long);

char* scannerReadMessage(FILE*, jpegStats*, long);

long getMaxMessageSize(FILE*, jpegStats*, long);
#endif
