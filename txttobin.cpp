#include <iostream>
#include <string.h>


int main(int argc, char** argv){

    char* input = argv[1];
    char* output = argv[2];
    FILE* fp = fopen(input, "r");
    FILE* fw = fopen(output, "wb");

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, fp)) != -1) {
        char *ptr = strtok(line, "\t");
        int u = atoi(ptr);
        int v = atoi(strtok(NULL, "\n"));
        fwrite((const void*) &u, sizeof(int),1,fw);
        fwrite((const void*) &v, sizeof(int),1,fw);
    }

    fclose(fw);
    fclose(fp);

    
}