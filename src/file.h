#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ArrayString_s {
    char** data;
    int n;
} ArrayString;

typedef struct ArrayDouble_s {
    double* data;
    int n;
    int i;
} ArrayDouble;

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

ArrayDouble** readFile(const char* const path) {
    ArrayDouble** res = NULL;
    if (path) {
        FILE* fp = fopen(path, "r"); 

        char buffer [100];
        if (!fp) {
            printf("Error: file could not be opened");
        } else {

            ArrayDouble* db_line = (ArrayDouble*)malloc(sizeof(ArrayDouble));

            db_line->n = 100;
            db_line->data = (double*)malloc(db_line->n*sizeof(double));

            int count = 0;

            while (fgets(buffer, sizeof(buffer), fp)) {
                buffer[(int)strlen(buffer)-1] = '\0';
                ArrayString* str_line = split(buffer);

                for (int i = 0; i < str_line->n; i++) {

                    double value = atof(str_line->data[i]);

                    if (count > db_line->n) {
                        db_line->data = (double*)realloc(db_line->data, (db_line->n+100)*sizeof(double));
                    }
                    db_line->data[db_line->i++] = value;
                    count++;
                }

                freeArrayString(str_line);
            }
        }
    }
    return res;
}
