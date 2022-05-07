//
// Created by serpentspirale on 02/05/2022.
//

#include "vgpu_string_utils.h"
#include "memory.h"
#include "../string_utils.h"


char * InplaceReplaceByIndex(char* pBuffer, int* size, const int startIndex, const int endIndex, const char* replacement){
    int length_difference;
    if(endIndex < startIndex)
        length_difference = strlen(replacement) + (endIndex - startIndex);
    else if(endIndex == startIndex)
        length_difference = strlen(replacement) - 1; // The initial char gets replaced
    else
        length_difference = strlen(replacement) - (endIndex - startIndex); // Can be negative if repl is smaller

    pBuffer = ResizeIfNeeded(pBuffer, size, length_difference);
    // Move the end of the string
    memmove(pBuffer + startIndex + strlen(replacement) , pBuffer + endIndex + 1, strlen(pBuffer) - endIndex + 1);

    // Insert the replacement
    memcpy(pBuffer + startIndex, replacement, strlen(replacement));

    return pBuffer;
}

int isDigit(char value){
    return (value >= '0' && value <= '9');
}

int isValidFunctionName(char value){
    return ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value == '_'));
}
