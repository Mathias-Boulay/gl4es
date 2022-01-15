//
// Created by serpentspirale on 13/01/2022.
//

#include <string.h>
#include <malloc.h>
#include "shaderconv.h"
#include "../string_utils.h"

// Version expected to be replaced
char * old_version = "#version 120";
// The version we will declare as
//char * new_version = "#version 450 compatibility";
char * new_version = "#version 320 es";

int NO_OPERATOR_VALUE = 9999;

/** Convert the shader through multiple steps
 * @param source The start of the shader as a string*/
char * ConvertShader(char* source){
    int sourceLength = strlen(source);

    // Either vertex shader (vsh) or fragment shader (fsh)
    int vsh = 0;
    if(strstr(source, "gl_Position"))
        vsh = 1;

    // Change version header
    source = GLSLHeader(source);
    // Remove 'const' storage qualifier
    source = RemoveConstInsideBlocks(source);

    // Switch a few keywords
    if(vsh){
        source = InplaceReplace(source, &sourceLength, "attribute\0", "in\0");
        source = InplaceReplace(source, &sourceLength, "varying\0", "out\0");
    }else{
        source = InplaceReplace(source, &sourceLength, "varying\0", "in\0");
    }

    // TODO ALL THE OTHER STUFF

    return source;
}

/** Small helper to help evaluate whether to continue or not I guess */
int GetOperatorValue(char operator){
    if(operator == ',' || operator == ';') return 5;
    if(operator == '=') return 4;
    if(operator == '+' || operator == '-') return 3;
    if(operator == '*' || operator == '/' || operator == '%') return 2;
    return NO_OPERATOR_VALUE; // Meaning no value;
}

/** Get the left or right operand, given the last index of the operator
 * @param source The shader as a string
 * @param operatorIndex The index the operator is found
 * @param rightOperand Whether we get the right or left operator
 * @return newly allocated string with the operand
 */
char* GetOperandFromOperator(char* source, int operatorIndex, int rightOperand){
    int parserState = 0;
    int parserDirection = rightOperand ? 1 : -1;
    int operandStartIndex = 0, operandEndIndex = 0;
    int parenthesesLeft = 0, hasFoundParentheses = 0;
    int operatorValue = GetOperatorValue(source[operatorIndex]);

    char parenthesesStart = rightOperand ? '(' : ')';
    char parenthesesEnd = rightOperand ? ')' : '(';
    int stringIndex = operatorIndex;

    // Get to the operand
    while (parserState == 0){
        stringIndex += parserDirection;
        if(source[stringIndex] != ' '){
            parserState = 1;
            // Place the mark
            if(rightOperand){
                operandStartIndex = stringIndex;
            }else{
                operandEndIndex = stringIndex;
            }
        }
    }

    // Get to the other side of the operand, the twist is here.
    while (parenthesesLeft > 0 || parserState == 1){

        // Look for parentheses
        if(source[stringIndex] == parenthesesStart){
            hasFoundParentheses = 1;
            parenthesesLeft += 1;
            stringIndex += parserDirection;
            continue;
        }

        if(source[stringIndex] == parenthesesEnd){
            hasFoundParentheses = 1;
            parenthesesLeft -= 1;

            // Likely to happen in a function call
            if(parenthesesLeft < 0){
                parserState = 3;
                if(rightOperand){
                    operandEndIndex = stringIndex - 1;
                }else{
                    operandStartIndex = stringIndex + 1;
                }
                continue;
            }
            stringIndex += parserDirection;
            continue;
        }

        // Small optimisation
        if(parenthesesLeft > 0){
            stringIndex += parserDirection;
            continue;
        }

        // So by now the following assumptions are made
        // 1 - We aren't between parentheses
        // 2 - No implicit multiplications are present

        // Higher value operators have less priority
        int currentValue = GetOperatorValue(source[stringIndex]);
        if(currentValue >= operatorValue){
            if(currentValue == NO_OPERATOR_VALUE){
                if(source[stringIndex] == ' '){
                    stringIndex += parserDirection;
                    continue;
                }
                // Else, maybe it is the start of a function ?
                if(hasFoundParentheses){
                    parserState = 2;
                    continue;
                }
                // If no full () set is found, assume we didn't fully travel the operand
                stringIndex += parserDirection;
                continue;
            }
            // Stop, we found an operator of same worth.
            parserState = 3;
            if(rightOperand){
                operandEndIndex = stringIndex - 1;
            }else{
                operandStartIndex = stringIndex + 1;
            }
        }
        stringIndex += parserDirection;
    }

    // Status when we get the name of a function and nothing else.
    while (parserState == 2){
        if(source[stringIndex] != ' '){
            stringIndex += parserDirection;
            continue;
        }
        if(rightOperand){
            operandEndIndex = stringIndex - 1;
        }else{
            operandStartIndex = stringIndex + 1;
        }
        parserState = 3;
    }

    // At this point, we know both the start and end point of our operand, let's copy it
    char * operand = malloc(operandEndIndex - operandStartIndex + 1);
    memcpy(operand, source+operandStartIndex, operandEndIndex - operandStartIndex + 1);

    return operand;
}

/** Replace the old version by new version, could be more advanced
 * @param source The pointer to the start of the shader */
char * GLSLHeader(char* source){
    /*
    if(!hardext.glsl320es && !hardext.glsl310es && !hardext.glsl300es){
        return source;
    }*/
    int lS = strlen(source) + 1;
    source = InplaceReplace(source, &lS , old_version, new_version);
    return source;
}

/** Remove 'const ' storage qualifier from variables inside {..} blocks
 * @param source The pointer to the start of the shader */
char * RemoveConstInsideBlocks(char* source){
    int insideBlock = 0;
    char * keyword = "const \0";
    int keywordLength = strlen(keyword);
    int sourceLength = strlen(source);

    for(int i=0; i< sourceLength+1; ++i){
        // Step 1, look for a block
        if(source[i] == '{'){
            insideBlock += 1;
            continue;
        }
        if(!insideBlock) continue;

        // A block has been found, look for the keyword
        if(source[i] == keyword[0]){
            int keywordMatch = 1;
            int j = 1;
            while (j < keywordLength){
                if (source[i+j] != keyword[j]){
                    keywordMatch = 0;
                    break;
                }
                j +=1;
            }
            // A match is found, remove it
            if(keywordMatch){
                source = InplaceReplaceByIndex(source, &sourceLength, i, i+j - 1, " ");
                sourceLength = strlen(source);
                continue;
            }
        }

        // Look for an exit
        if(source[i] == '}'){
            insideBlock -= 1;
            continue;
        }
    }
    return source;
}


