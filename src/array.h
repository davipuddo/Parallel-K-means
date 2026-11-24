#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char** data;
    int n;
} ArrayString;

static inline void freeArrayString(ArrayString* ptr) {
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

static inline void printArrayString(ArrayString* ptr) {
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

static inline void freeArrayDouble(ArrayDouble* ptr) {
    if (ptr) {
        if (ptr->arr) {
            free(ptr->arr);
        }
        ptr->n = 0;
        free(ptr);
    }
}

static inline ArrayDouble* newArrayDouble(int n, double start_val) {
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

static inline void printArrayDouble(ArrayDouble* ptr) {
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

static inline void freeArrayInt(ArrayInt* ptr) {
    if (ptr) {
        if (ptr->arr) {
            free(ptr->arr);
        }
        ptr->n = 0;
        free(ptr);
    }
}

static inline ArrayInt* newArrayInt(int n, int start_val) {
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

static inline void freeNodes(Nodes* ptr) {
    if (ptr) {
        if (ptr->list) {
            freeArrayDouble(ptr->list);
        }
        ptr->n = 0;
        free(ptr);
    }
}

static inline Nodes* newNodes(int n) {
    Nodes* res = NULL;
    if (n > 0) {
        res = (Nodes*)malloc(sizeof(Nodes));
        res->list = (ArrayDouble*)malloc(n*sizeof(ArrayDouble));
        res->n = n;
    }
    return res;
}

static inline void printNodes(Nodes* ptr) {
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

static inline bool compareArrayDouble (ArrayDouble* x, ArrayDouble* y) {
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

static inline void copyArrayDouble (ArrayDouble* dest, ArrayDouble* src) {
    if (dest && dest->arr && src && src->arr && (dest->n == src->n)) {
        for (int l = 0; l < src->n; l++) {
            dest->arr[l] = src->arr[l];
        }
    }
}

static inline void addArrayDouble (ArrayDouble* x, double amount) {
    if (x && x->arr) {
        for (int i = 0; i < x->n; i++) {
            x->arr[i] += amount;
        }
    }
}

static inline void sumArrayDouble (ArrayDouble* x, ArrayDouble* y) {
    if (x && x->arr && y && y->arr && (x->n == y->n)) {
        for (int i = 0; i < x->n; i++) {
            x->arr[i] += y->arr[i];
        }
    }
}

static inline void divArrayDouble (ArrayDouble* x, double y) {
    if (x && x->arr && y != 0) {
        for (int i = 0; i < x->n; i++) {
            x->arr[i] /= y;
        }
    }
}

const int FILE_LINE_LIMIT = 10000;

Nodes* readFile(const char* const path) {
    Nodes* res = NULL;
    if (path) 
    {
        FILE* fp = fopen(path, "r"); 

        char buffer[4096];
        if (!fp) {
            printf("Error: file could not be opened");
        } 
        else 
        {
            int capacity = 1024;
            res = newNodes(capacity);

            int row = 0;
            while (fgets(buffer, sizeof(buffer), fp) && row < FILE_LINE_LIMIT)    // buffer = line in the file
            {
                int len = (int)strlen(buffer);
                while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
                    buffer[--len] = '\0';
                }
                if (len == 0) {
                    continue;
                }

                if (row >= capacity) {
                    capacity *= 2;
                    ArrayDouble* grown = (ArrayDouble*)realloc(res->list, capacity * sizeof(ArrayDouble));
                    if (!grown) {
                        res->n = row;
                        freeNodes(res);
                        fclose(fp);
                        return NULL;
                    }
                    res->list = grown;
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
            res->n = row;
            fclose(fp);
        }
    }
    return res;
}
