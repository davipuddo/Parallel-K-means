#include <limits.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include "file.h"

const double K_CUTOFF = 0.3;
const int RUNS = 100;

int parseArgs(int argc, const char** argv) {
    int k = 0;
    if (argc == 2) {
        k = atoi(argv[1]);
        if (k < 0) {
            printf("Error: K should be a positive value!\n");
        }
    }
    return k;
}

int main (int argc, const char** argv) {

    srand(time(NULL));

    ArrayDouble* res = readFile("../datasets/test_dataset.txt");
    if (res && res->data) {
        for (int i = 0; i < res->i; i++) {
            printf("[%lf]\n", res->data[i]);
        }
        printf("\n");
    }

    return 0;
}
