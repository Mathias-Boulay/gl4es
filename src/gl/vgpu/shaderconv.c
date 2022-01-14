//
// Created by serpentspirale on 13/01/2022.
//

#include <string.h>
#include "shaderconv.h"
#include "../string_utils.h"

// Version expected to be replaced
char * old_version = "#version 120";
// The version we will declare as
//char * new_version = "#version 450 compatibility";
char * new_version = "#version 320 es";

/** Replace the old version by new version, could be more advanced
 * @param source The pointer to the start of the shader */
void GLSLHeader(char* source){
    /*
    if(!hardext.glsl320es && !hardext.glsl310es && !hardext.glsl300es){
        return;
    }*/
    int lS = strlen(source) + 1;
    source = InplaceReplace(source, &lS , old_version, new_version);
}

/** Remove 'const ' storage qualifier from variables inside {..} blocks
 * @param source The pointer to the start of the shader */
void RemoveConstInsideBlocks(char* source){
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

                InplaceReplaceByIndex(source, &sourceLength, i, i+j - 1, " ");
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

}


