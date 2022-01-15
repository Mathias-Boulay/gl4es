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
char * ConvertShaderVgpu(char* source){
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

    // TODO Convert everything to float I guess :think:

    // % operator isn't supported, so change it
    source = ReplaceModOperator(source);

    // Hey we don't want to deal with implicit type stuff
    source = CoerceIntToFloat(source);

    // Avoid any weird type trying to be an index for an array
    source = ForceIntegerArrayAccess(source);

    return source;
}

/**
 * Replace the % operator with a mathematical equivalent (x - y * floor(x/y))
 * @param source The shader as a string
 * @return The shader as a string, maybe in a different memory location
 */
char * ReplaceModOperator(char * source){
    char * modelString = "(x - y * (floor( x / y )))";
    int sourceLength = strlen(source);
    int startIndex, endIndex = 0;
    int * startPtr = &startIndex, *endPtr = &endIndex;

    for(int i=0;i<sourceLength; ++i){
        if(source[i] != '%') continue;
        // A mod operator is found !
        char * leftOperand = GetOperandFromOperator(source, i, 0, startPtr);
        char * rightOperand = GetOperandFromOperator(source, i, 1, endPtr);

        // Generate a model string to be inserted
        char * replacementString = malloc(strlen(modelString) + 1);
        strcpy(replacementString, modelString);
        int replacementSize = strlen(replacementString);
        replacementString = InplaceReplace(replacementString, &replacementSize, "x", leftOperand);
        replacementString = InplaceReplace(replacementString, &replacementSize, "y", rightOperand);

        // Insert the new string
        source = InplaceReplaceByIndex(source, &sourceLength, startIndex, endIndex, replacementString);

        // Free all the temporary strings
        free(leftOperand);
        free(rightOperand);
        free(replacementString);
    }

    return source;
}

/**
 * Change all (u)ints to floats.
 * This is a hack to avoid dealing with implicit conversions on common operators
 * @param source The shader as a string
 * @return The shader as a string, maybe in a new memory location
 * @see ForceIntegerArrayAccess
 */
char * CoerceIntToFloat(char * source){
    // Let's go the "freestyle way"
    int sourceLength = strlen(source);

    // Scalar values
    source = InplaceReplaceSimple(source, &sourceLength, "uint ", "float ");
    source = InplaceReplaceSimple(source, &sourceLength, "int ", "float ");

    // Vectors
    // TODO Avoid breaking variable name !
    source = InplaceReplaceSimple(source, &sourceLength,"ivec", "vec");

    // Step 3 is slower.
    // We need to parse hardcoded values like 1 and turn it into 1.(0)
    for(int i=0; i<sourceLength; ++i){

        // We avoid all preprocessor directives for now
        if(source[i] == '#'){
            // Look for the next line
            while (source[i] != '\n'){
                i++;
            }
        }

        if(!isDigit(source[i])){ continue; }
        // So there is a few situations that we have to distinguish:
        // functionName1 (      ----- meaning there is SOMETHING on its left side that is related to the number
        // function(1,          ----- there is something, and it ISN'T related to the number
        // float test=3;        ----- something on both sides, not related to the number.
        // float test=X.2       ----- There is a dot, so it is part of a float already
        // float test = 0.00000 ----- I have to backtrack to find the dot

        if(source[i-1] == '.' || source[i+1] == '.') continue;// Number part of a float
        if(isValidFunctionName(source[i - 1])) continue; // Char attached to something related
        if(/*isDigit(source[i-1]) || */ isDigit(source[i+1])) continue; // End of number not reached
        if(isDigit(source[i-1])){
            // Backtrack to check if the number is floating point
            int shouldBeCoerced = 0;
            for(int j=1; 1; ++j){
                if(isDigit(source[i-j])) continue;
                if(isValidFunctionName(source[i-j])) continue; // Function or variable name, don't coerce
                if(source[i-j] == '.') break; // No coercion, float already
                // Nothing found, should be coerced then
                shouldBeCoerced = 1;
                break;
            }

            if(!shouldBeCoerced) continue;
        }

        // Now we know there is nothing related to the digit, turn it into a float
        source = InplaceReplaceByIndex(source, &sourceLength, i+1, i, ".0");
    }

    // TODO Hacks for special built in values and typecasts ?

    return source;
}

/** Force all array accesses to use integers by adding an explicit typecast
 * @param source The shader as a string
 * @return The shader as a string, maybe at a new memory location */
char * ForceIntegerArrayAccess(char* source){
    char * markerStart = "$";
    char * markerEnd = "`";
    int sourceLength = strlen(source);

    // Step 1, we need to mark all [] that are empty and must not be changed
    int leftCharIndex = 0;
    for(int i=0; i< sourceLength; ++i){
        if(source[i] == '['){
            leftCharIndex = i;
            continue;
        }
        // If a start has been found
        if(leftCharIndex){
            if(source[i] == ' ' || source[i] == '\n'){
                continue;
            }
            // We find the other side and mark both ends
            if(source[i] == ']'){
                source[leftCharIndex] = *markerStart;
                source[i] = *markerEnd;
            }
        }
        //Something else is there, abort the marking phase for this one
        leftCharIndex = 0;
    }

    // Step 2, replace the array accesses with a forced typecast version
    source = InplaceReplaceSimple(source, &sourceLength, "]", ")]");
    source = InplaceReplaceSimple(source, &sourceLength, "[", "[int(");

    // Step 3, restore all marked empty []
    source = InplaceReplace(source, &sourceLength, markerStart, "[");
    source = InplaceReplace(source, &sourceLength, markerEnd, "]");

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
 * @param limit The left or right index of the operand
 * @return newly allocated string with the operand
 */
char* GetOperandFromOperator(char* source, int operatorIndex, int rightOperand, int * limit){
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

    // Send back the limitIndex
    *limit = rightOperand ? operandEndIndex : operandStartIndex;

    return operand;
}

/** Replace the version by new version, period.
 * @param source The pointer to the start of the shader */
char * GLSLHeader(char* source){
    /*
    if(!hardext.glsl320es && !hardext.glsl310es && !hardext.glsl300es){
        return source;
    }*/
    int sourceLength = strlen(source);
    int startIndex, endIndex = 0;
    int index = 0;
    int parserState = 0;

    // Step 1 get to the version
    while (parserState == 0){
        if(source[index] == '#' && source[index + 1] == 'v'){
            parserState = 1;
            startIndex = index;
        }
        index ++;
    }

    //Step 2 get to the end of the line
    while(parserState == 1){
        if(source[index] == '\n'){
            parserState = 2;
            endIndex = index;
        }
        index++;
    }

    // Step 3, replace.
    InplaceReplaceByIndex(source, &sourceLength, startIndex, endIndex, new_version);

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


