//
// Created by serpentspirale on 25/07/2022.
//

#include "../program.h"

#ifndef GL4ES_UTILS_H
#define GL4ES_UTILS_H

int testGenericShader(struct shader_s* shader_source);
char * InplaceInsertByIndex(char * source, int *sourceLength, int insertPoint, const char *insertedString);
char * InplaceReplaceByIndex(char* pBuffer, int* size, int startIndex, int endIndex, const char* replacement);
int isDigit(char value);
int isValidFunctionName(char value);

#endif //GL4ES_UTILS_H
