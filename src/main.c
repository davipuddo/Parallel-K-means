#include <limits.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include "kmeans.h"

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

    Nodes* datapoints = readFile("../datasets/test_dataset.txt");
    if (!datapoints || !datapoints->list) {
        fprintf(stderr, "Error: initial node configuration is NULL. Aborting!\n");
        return -1;
    }

    int k = parseArgs(argc, argv);

    if (k > datapoints->n) {
        fprintf(stderr, "Error: the value of K is higher than the number of entries on the dataset. Aborting!\n");
        return -1;
    }

    KmeansResult* res = NULL;

    if (k > 0) {
        res = min_kmeans(datapoints, k, RUNS);
        printf("here\n");
        fflush(stdout);
    } else if (k == 0) {
        bool stop = false;
        double cost = DBL_MAX;
        int k = 2;

        while (!stop && k < datapoints->n) {
            res = min_kmeans(datapoints, k, RUNS);
            double new_cost = res->cost;

            double relative_cost = (cost - new_cost) / cost;
            cost = new_cost;

            if (relative_cost < K_CUTOFF) {
                stop = true;
            } else {
                if (k < datapoints->n - 1) {
                    freeKmeansResult(res);
                }
                k++;
            }
        }
    }

    printKmeansResult(res, datapoints);

    // Cleanup

    freeKmeansResult(res);
    freeNodes(datapoints);
    return 0;
}
