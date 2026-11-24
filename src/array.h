#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char** data;
    int n;
} ArrayString;

void freeArrayString(ArrayString* ptr) {
    if (ptr) {
        if (ptr->data) {
            for (int i = 0; i < ptr->n; i++) {
                if (ptr->data[i]) {
                    free(ptr->data[i]);
                }
            }
            free(ptr->data);
        }
        ptr->n = 0;
    }
}

void printArrayString(ArrayString* ptr) {
    if (ptr && ptr->data) {
        for (int i = 0; i < ptr->n; i++) {
            if (ptr->data[i]) {
                printf("%s\n", ptr->data[i]);
            }
        }
    }
}

ArrayString* split(char* str) {
    ArrayString* res = NULL;

    if (str) {

        int n = (int)strlen(str);

        int count = 1;
        for (int i = 0; i < n; i++) {
            if (str[i] == ',') {
                count++;
            }
        }

        res = (ArrayString*)malloc(1*sizeof(ArrayString));
        res->data = (char**)malloc(count*sizeof(char*));
        res->n = count;

        char* tok = strtok(str, ",");

        int i = 0;
        while (tok != 0) {

            int m = (int)strlen(tok);
            res->data[i] = (char*)calloc(m+1, sizeof(char));
            strcpy(res->data[i], tok);
            
            tok = strtok(0, ",");
            i++;
        }

    }

    return res;
}

typedef struct {
    double* data;
    int n;
} ArrayDouble;

typedef struct {
    int* data;
    int n;
} ArrayInt;

typedef struct {
    ArrayDouble* data;
    int n;

} Nodes;

Nodes* readFile(const char* const path) {
    Nodes* res = NULL;
    if (path) {
        FILE* fp = fopen(path, "r"); 

        char buffer [100];
        if (!fp) {
            printf("Error: file could not be opened");

        } else {

            int count = 0;

            while (fgets(buffer, sizeof(buffer), fp)) {
                count++;
            }

            fseek(fp, 0, SEEK_SET);

            res = (Nodes*)malloc(sizeof(Nodes));
            res->data = (ArrayDouble*)malloc(count*sizeof(double*));
            res->n = count;

            int ri = 0;
            while (fgets(buffer, sizeof(buffer), fp)) {

                buffer[(int)strlen(buffer)-1] = '\0';    // Remove \n
                ArrayString* str_line = split(buffer);

                for (int i = 0; i < str_line->n; i++) {
                    double value = atof(str_line->data[i]);
                    res->data[ri].data[i] = value;
                }

                freeArrayString(str_line);
                ri++;
            }
        }
    }
    return res;
}
