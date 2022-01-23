//
// Created by serpentspirale on 13/01/2022.
//

#ifndef UNTITLED_SHADERCONV_H
#define UNTITLED_SHADERCONV_H

char * ConvertShaderVgpu(char* source);

char * GLSLHeader(char* source);
char * RemoveConstInsideBlocks(char* source);
char * ForceIntegerArrayAccess(char* source);
char * CoerceIntToFloat(char * source);
char * ReplaceModOperator(char * source);
char * WrapIvecFunctions(char * source);
char * WrapFunction(char * source, char * functionName, char * wrapperFunctionName, char * wrapperFunction);
int FindPositionAfterDirectives(char * source);

char* GetOperandFromOperator(char* source, int operatorIndex, int rightOperand, int * limit);

#endif //UNTITLED_SHADERCONV_H
