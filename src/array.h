#pragma once

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
        free(ptr);
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
        res->data = (char**)calloc(count,sizeof(char*));
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

        res->n = i;
    }

    return res;
}

typedef struct {
    double* arr;
    int n;
} ArrayDouble;

void freeArrayDouble(ArrayDouble* ptr) {
    if (ptr) {
        if (ptr->arr) {
            free(ptr->arr);
        }
        ptr->n = 0;
        free(ptr);
    }
}

ArrayDouble* newArrayDouble(int n, double start_val) {
    ArrayDouble* res = NULL;
    if (n > 0) {
        res = (ArrayDouble*)malloc(sizeof(ArrayDouble));
        res->arr = (double*)malloc(n*sizeof(double));
        res->n = n;
        for (int i = 0; i < n; i++) {
            res->arr[i] = start_val;
        }
    }
    return res;
}

void printArrayDouble(ArrayDouble* ptr) {
    if (ptr && ptr->arr) {
        printf("{%lf", ptr->arr[0]);
        for (int i = 1; i < ptr->n; i++) {
            printf(",%lf", ptr->arr[i]);
        }
        printf("}\n");
    }
}

typedef struct {
    int* arr;
    int n;
} ArrayInt;

void freeArrayInt(ArrayInt* ptr) {
    if (ptr) {
        if (ptr->arr) {
            free(ptr->arr);
        }
        ptr->n = 0;
        free(ptr);
    }
}

ArrayInt* newArrayInt(int n, int start_val) {
    ArrayInt* res = NULL;
    if (n > 0) {
        res = (ArrayInt*)malloc(sizeof(ArrayInt));
        res->arr = (int*)malloc(n*sizeof(int));
        res->n = n;
        for (int i = 0; i < n; i++) {
            res->arr[i] = start_val;
        }
    }
    return res;
}

typedef struct {
    ArrayDouble* list;
    int n;
} Nodes;

void freeNodes(Nodes* ptr) {
    if (ptr) {
        if (ptr->list) {
            for (int i = 0; i < ptr->n; i++) {
                if (ptr->list[i].arr) {
                    free(ptr->list[i].arr);
                    ptr->list[i].n = 0;
                }
            }
            free(ptr->list);
        }
        ptr->n = 0;
        free(ptr);
    }
}

Nodes* newNodes(int n) {
    Nodes* res = NULL;
    if (n > 0) {
        res = (Nodes*)malloc(sizeof(Nodes));
        res->list = (ArrayDouble*)malloc(n*sizeof(ArrayDouble));
        res->n = n;
    }
    return res;
}

void printNodes(Nodes* ptr) {
    if (ptr && ptr->list) {
        for (int i = 0; i < ptr->n; i++) {
            for (int j = 0; j < ptr->list[i].n; j++) {
                printf("[%lf] ", ptr->list[i].arr[j]);
            }
            printf("\n");
        }
        printf("\n");
    }
}

bool compareArrayDouble (ArrayDouble* x, ArrayDouble* y) {
    bool res = false;
    if (x && x->arr && y && y->arr && (x->n == y->n)) {
        int i = 0;
        while (i < x->n && (x->arr[i] == y->arr[i])) {
            i++;
        }
        if (i == x->n) {
            res = true;
        }
    }
    return res;
}

void copyArrayDouble (ArrayDouble* dest, ArrayDouble* src) {
    if (dest && dest->arr && src && src->arr && (dest->n == src->n)) {
        for (int l = 0; l < src->n; l++) {
            dest->arr[l] = src->arr[l];
        }
    }
}

void addArrayDouble (ArrayDouble* x, double amount) {
    if (x && x->arr) {
        for (int i = 0; i < x->n; i++) {
            x->arr[i] += amount;
        }
    }
}

void sumArrayDouble (ArrayDouble* x, ArrayDouble* y) {
    if (x && x->arr && y && y->arr && (x->n == y->n)) {
        for (int i = 0; i < x->n; i++) {
            x->arr[i] += y->arr[i];
        }
    }
}

void divArrayDouble (ArrayDouble* x, double y) {
    if (x && x->arr && y != 0) {
        for (int i = 0; i < x->n; i++) {
            x->arr[i] /= y;
        }
    }
}

Nodes* readFile(const char* const path) {
    Nodes* res = NULL;
    if (path) 
    {
        FILE* fp = fopen(path, "r"); 

        char buffer [100];
        if (!fp) {
            printf("Error: file could not be opened");
        } 
        else 
        {
            const int LINES = 21;

            res = newNodes(LINES);

            int row = 0;
            while (fgets(buffer, sizeof(buffer), fp))    // buffer = line in the file
            {
                int last = (int)strlen(buffer)-1;
                if (last > 0 && buffer[last] == '\n') {
                    buffer[last] = '\0';
                }

                ArrayString* str_line = split(buffer);   // Split on [,]

                ArrayDouble* tmp = newArrayDouble(str_line->n, 0.0);
                res->list[row] = *tmp;
                free(tmp);

                for (int i = 0; i < str_line->n; i++) 
                {
                    double value = atof(str_line->data[i]);
                    res->list[row].arr[i] = value;
                }

                freeArrayString(str_line);
                row++;
            }
            fclose(fp);
        }
    }
    return res;
}
