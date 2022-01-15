//
// Created by serpentspirale on 13/01/2022.
//

#ifndef UNTITLED_SHADERCONV_H
#define UNTITLED_SHADERCONV_H

char * ConvertShader(char* source);

char * GLSLHeader(char* source);
char * RemoveConstInsideBlocks(char* source);
char * ForceIntegerArrayAccess(char* source);
char * CoerceIntToFloat(char * source);

char* GetOperandFromOperator(char* source, int operatorIndex, int rightOperand);

#endif //UNTITLED_SHADERCONV_H
