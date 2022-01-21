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

    // Hey we don't want to deal with implicit type stuff
    source = CoerceIntToFloat(source);

    // Avoid any weird type trying to be an index for an array
    source = ForceIntegerArrayAccess(source);

    return source;
}

char * WrapIvecFunctions(char * source){

}

/**
 * Replace a function and its calls by a wrapper version, only if needed
 * @param source The shader code as a string
 * @param functionName The function to be replaced
 * @param wrapperFunctionName The replacing function name
 * @param function The wrapper function itself
 * @return The shader as a string, maybe in a different memory location
 */
char * WrapFunction(char * source, char * functionName, char * wrapperFunctionName, char * wrapperFunction){
    // Okay, we have a few cases to distinguish to make sure we don't replace something unintended:
    // Let's say we want to replace a "FunctionName"
    // float FunctionNameResult ...             ----- something between FunctionName and the ( , skip it.
    // BiggerFunctionName(...                   ----- FunctionName part of another function ! Skip it.
    // Function(FunctionName( ...               ----- There is a call to function name

    int sourceLength = strlen(source);
    char * findStringPtr = NULL;
    findStringPtr = FindString(source, functionName);
    if(findStringPtr){
        // Check the right end
        for(int i= strlen(functionName); 1; ++i){
            if(findStringPtr[i] == ' ' || findStringPtr[i] == '\n') continue;
            if (findStringPtr[i] == '(') break; // Allowed going further, since it looks like a function call
            return source;
        }

        // Left end
        findStringPtr --;
        if(isValidFunctionName(findStringPtr[0])) return source; // Var or function name
        // At this point, the call is real, so just replace them
        InplaceReplaceSimple(source, &sourceLength, functionName, wrapperFunctionName);
        InplaceInsert(source, wrapperFunction, source, &sourceLength);

    }
}

/**
 * Replace the % operator with a mathematical equivalent (x - y * floor(x/y))
 * @param source The shader as a string
 * @return The shader as a string, maybe in a different memory location
 */
char * ReplaceModOperator(char * source){
    char * modelString = "((x) - (y) * (floor( (x) / (y) )))";
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


/** Small helper to help evaluate whether to continue or not I guess
 * Values over 9900 are not for real operators, more like stop indicators*/
int GetOperatorValue(char operator){
    if(operator == ',' || operator == ';') return 9998;
    if(operator == '=') return 9997;
    if(operator == '+' || operator == '-') return 3;
    if(operator == '*' || operator == '/' || operator == '%') return 2;
    return NO_OPERATOR_VALUE; // Meaning no value;
}

/** Get the left or right operand, given the last index of the operator
 * It bases its ability to get operands by evaluating the priority of operators.
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
    int lastOperator = 0; // Used to determine priority for unary operators

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

            // Special case for unary operator when parsing to the right
            if(GetOperatorValue(source[stringIndex]) == 3 ){ // 3 is +- operators
                stringIndex += parserDirection;
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
        // 3 - No fuckery with operators like "test = +-+-+-+-+-+-+-+-3;" although I attempt to support them

        // Higher value operators have less priority
        int currentValue = GetOperatorValue(source[stringIndex]);


        // The condition is different due to the evaluation order which is left to right, aside from the unary operators
        if((rightOperand ? currentValue >= operatorValue: currentValue > operatorValue)){
            if(currentValue == NO_OPERATOR_VALUE){
                if(source[stringIndex] == ' '){
                    stringIndex += parserDirection;
                    continue;
                }

                // Found an operand, so reset the operator eval for unary
                if(rightOperand) lastOperator = NO_OPERATOR_VALUE;

                // maybe it is the start of a function ?
                if(hasFoundParentheses){
                    parserState = 2;
                    continue;
                }
                // If no full () set is found, assume we didn't fully travel the operand
                stringIndex += parserDirection;
                continue;
            }

            // Special case when parsing unary operator to the right
            if(rightOperand && operatorValue == 3 && lastOperator < currentValue){
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

        // Special case for unary operators from the right
        if(rightOperand && operatorValue == 3) { // 3 is + - operators
            lastOperator = currentValue;
        } // Special case for unary operators from the left
        if(!rightOperand && operatorValue < 3 && currentValue == 3){
            lastOperator = NO_OPERATOR_VALUE;
            for(int j=1; 1; ++j){
                int subCurrentValue = GetOperatorValue(source[stringIndex - j]);
                if(subCurrentValue != NO_OPERATOR_VALUE){
                    lastOperator = subCurrentValue;
                    continue;
                }

                // No operator value, can be almost anything
                if(source[stringIndex - j] == ' ') continue;
                // Else we found something. Did we found a high priority operator ?
                if(lastOperator <= operatorValue){ // If so, we allow continuing and going out of the loop
                    stringIndex -= j;
                    parserState = 1;
                    break;
                }
                // No other operator found
                operandStartIndex = stringIndex;
                parserState = 3;
                break;
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

/**
 * @param source The shader as a string
 * @return The position after the #version line, start of the shader if not found
 */
char * FindPositionAfterVersion(char * source){
    char * position = FindString(source, "#version");
    if (position == NULL) return source;
    for(int i=7; 1; ++i){
        if(position[i] == '\n'){
            return position + i;
        }
    }
}
