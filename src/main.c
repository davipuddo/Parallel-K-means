#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include "kmeans.h"

const double K_CUTOFF = 0.05;
const int RUNS = 30;
const int FILE_LIMIT = 1000000;

double get_monotonic_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

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

// Extract the last column as labels and exclude it from clustering features
ArrayInt* extractLabels(Nodes* datapoints) {
    ArrayInt* labels = newArrayInt(datapoints->n, 0);
    for (int i = 0; i < datapoints->n; i++) {
        int cols = datapoints->list[i].n;
        labels->arr[i] = (int)datapoints->list[i].arr[cols - 1];
        if (cols > 1) {
            datapoints->list[i].n = cols - 1;
        }
    }
    return labels;
}

// Z-score standardize each feature so large-scale features do not dominate Euclidean distance
void standardizeFeatures(Nodes* datapoints) {
    if (!datapoints || datapoints->n <= 1) return;
    int n = datapoints->n;
    int d = datapoints->list[0].n;

    for (int j = 0; j < d; j++) {
        double mean = 0.0;
        for (int i = 0; i < n; i++) {
            mean += datapoints->list[i].arr[j];
        }
        mean /= n;

        double var = 0.0;
        for (int i = 0; i < n; i++) {
            double diff = datapoints->list[i].arr[j] - mean;
            var += diff * diff;
        }
        double stddev = sqrt(var / n);
        if (stddev > 0.0) {
            for (int i = 0; i < n; i++) {
                datapoints->list[i].arr[j] = (datapoints->list[i].arr[j] - mean) / stddev;
            }
        } else {
            for (int i = 0; i < n; i++) {
                datapoints->list[i].arr[j] = 0.0;
            }
        }
    }
}

// Map each cluster ID to its majority ground-truth class and count mismatches
int evaluateClustering(const ArrayInt* labels, const ArrayInt* cluster_list, int k) {
    int n = labels->n;
    int* cluster_to_class = (int*)malloc(k * sizeof(int));

    for (int c = 0; c < k; c++) {
        int best_class = 0;
        int best_count = 0;

        for (int i = 0; i < n; i++) {
            if (cluster_list->arr[i] != c) continue;
            int candidate = labels->arr[i];
            int cnt = 0;
            for (int j = i; j < n; j++) {
                if (cluster_list->arr[j] == c && labels->arr[j] == candidate) {
                    cnt++;
                }
            }
            if (cnt > best_count) {
                best_count = cnt;
                best_class = candidate;
            }
        }
        cluster_to_class[c] = best_class;
    }

    int count = 0;
    for (int i = 0; i < n; i++) {
        int which_class = labels->arr[i];
        int which_cluster = cluster_list->arr[i];
        int predicted_class = (which_cluster >= 0 && which_cluster < k) ? cluster_to_class[which_cluster] : -1;

        if (which_class != predicted_class) {
            printf("Mismatch in [%d] | class: %d vs predicted: %d (cluster %d)\n",
                   i, which_class, predicted_class, which_cluster);
            count++;
        }
    }

    free(cluster_to_class);
    return count;
}

int main (int argc, const char** argv) {

    srand(time(NULL));

    Nodes* datapoints = readFile("../datasets/treino_sempayload.csv", FILE_LIMIT);
    if (!datapoints || !datapoints->list || datapoints->n == 0) {
        fprintf(stderr, "Error: initial node configuration is NULL or empty. Aborting!\n");
        return -1;
    }

    ArrayInt* labels = extractLabels(datapoints);
    standardizeFeatures(datapoints);

    int k = parseArgs(argc, argv);

    if (k > datapoints->n) {
        fprintf(stderr, "Error: the value of K is higher than the number of entries on the dataset. Aborting!\n");
        freeArrayInt(labels);
        freeNodes(datapoints);
        return -1;
    }

    KmeansResult* res = NULL;

    clock_t t = clock();
    double time = get_monotonic_time();
    if (k > 0) {
        res = min_kmeans(datapoints, k, RUNS);
    } else if (k == 0) {
        int cur_k = 2;
        res = min_kmeans(datapoints, cur_k, RUNS);
        double cost = res->cost;

        for (cur_k = 3; cur_k < datapoints->n; cur_k++) {
            KmeansResult* next_res = min_kmeans(datapoints, cur_k, RUNS);
            double new_cost = next_res->cost;
            double relative_cost = (cost > 0.0) ? (cost - new_cost) / cost : 0.0;

            if (relative_cost < K_CUTOFF) {
                freeKmeansResult(next_res);
                break;
            }
            freeKmeansResult(res);
            res = next_res;
            cost = new_cost;
        }
    }
    time = get_monotonic_time() - time;
    printf("time taken: %lf\n", time);

    /*
    int num_clusters = res->clusters->n;
    int count = evaluateClustering(labels, res->cluster_list, num_clusters);
    printf("K: %d | Mismatches: %d / %d (Accuracy: %.2f%%)\n",
           num_clusters, count, datapoints->n,
           100.0 * (datapoints->n - count) / datapoints->n);
    */


    // Cleanup
    freeKmeansResult(res);
    freeArrayInt(labels);
    freeNodes(datapoints);
    return 0;
}
