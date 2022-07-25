//
// Created by serpentspirale on 25/07/2022.
//

#include "utils.h"

#include "../../glx/hardext.h"

#include "../debug.h"
#include "../gl4es.h"
#include "../init.h"
#include "../logs.h"
#include "../loader.h"
#include "../string_utils.h"

/**
 * @param shader_source The shader as a string
 * @return whether a shader compiles properly
 */
int testGenericShader(struct shader_s* shader_source) {
    // check the current shader is valid for compilation
    LOAD_GLES2(glCreateShader);
    LOAD_GLES2(glShaderSource);
    LOAD_GLES2(glCompileShader);
    LOAD_GLES2(glGetShaderiv);
    LOAD_GLES2(glDeleteShader);

    GLuint shad = gles_glCreateShader(shader_source->type);
    gles_glShaderSource(shad, 1, (const GLchar *const *)(&shader_source->converted), NULL);
    gles_glCompileShader(shad);
    GLint compiled;
    gles_glGetShaderiv(shad, GL_COMPILE_STATUS, &compiled);

    gles_glDeleteShader(shad);

    return compiled;
}

/**
 * Insert the string at the index, pushing "every chars to the right"
 * @param source The shader as a string
 * @param sourceLength The ALLOCATED length of the shader
 * @param insertPoint The index at which the string is inserted.
 * @param insertedString The string to insert
 * @return The shader as a string, maybe in a different memory location
 */
char * InplaceInsertByIndex(char * source, int *sourceLength, const int insertPoint, const char *insertedString){
    int insertLength = strlen(insertedString);
    source = ResizeIfNeeded(source, sourceLength, insertLength);
    memmove(source + insertPoint + insertLength,  source + insertPoint, strlen(source) - insertPoint + 1);
    memcpy(source + insertPoint, insertedString, insertLength);

    return source;
}

/**
 * Insert a string into another one, erasing anything between start and end
 * @param pBuffer The current string
 * @param size Allocated size of the current
 * @param startIndex
 * @param endIndex
 * @param replacement
 * @return The string, maybe in a different memory location.
 */
char * InplaceReplaceByIndex(char* pBuffer, int* size, const int startIndex, const int endIndex, const char* replacement) {
    //SHUT_LOGD("BY INDEX: %s", replacement);
    //SHUT_LOGD("BY INDEX: %i", strlen(replacement));

    int length_difference;
    if(endIndex < startIndex)
        length_difference = strlen(replacement) + (endIndex - startIndex);
    else if(endIndex == startIndex)
        length_difference = strlen(replacement) - 1; // The initial char gets replaced
    else
        length_difference = strlen(replacement) - (endIndex - startIndex); // Can be negative if repl is smaller

    pBuffer = ResizeIfNeeded(pBuffer, size, length_difference);
    //SHUT_LOGD("BEFORE MOVING: \n%s", pBuffer);
    // Move the end of the string
    memmove(pBuffer + startIndex + strlen(replacement) , pBuffer + endIndex + 1, strlen(pBuffer) - endIndex + 1);
    //SHUT_LOGD("AFTER MOVING 1: \n%s", pBuffer);

    // Insert the replacement
    memcpy(pBuffer + startIndex, replacement, strlen(replacement));
    //strncpy(pBuffer + startIndex, replacement, strlen(replacement));
    //SHUT_LOGD("AFTER MOVING 2: \n%s", pBuffer);

    return pBuffer;
}


int isDigit(char value){
    return (value >= '0' && value <= '9');
}

int isValidFunctionName(char value){
    return ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value == '_'));
}

