//
// Created by serpentspirale on 02/05/2022.
//

#ifndef GL4ES_VGPU_STRING_UTILS_H
#define GL4ES_VGPU_STRING_UTILS_H

int isDigit(char value);
int isValidFunctionName(char value);
char * InplaceReplaceByIndex(char* pBuffer, int* size, int startIndex, int endIndex, const char* replacement);

#endif //GL4ES_VGPU_STRING_UTILS_H
