#pragma once 

#include <float.h>
#include <stdlib.h>

#include "array.h"

const double CUTOFF = 1e-4;

const int MAX_ITER = 500;

const double MIN_RAND = 1.0f;
const double MAX_RAND = 100.0f;



inline double rand_double(int min, int max) {
    int INT = (rand() % (int)(max - min + 1.0)) + min;
    double DEC = ((double)rand() / (double)RAND_MAX);
    return INT + DEC;
}

double distance (ArrayDouble* x, ArrayDouble* y) {
    double sum = 0.0;
    if (x && x->arr && y && y->arr && (x->n == y->n)) {
        for (int i = 0; i < x->n; i++) {
            double dx = (x->arr[i] - y->arr[i]);
            sum += dx * dx;
        }
    }
    return sum;     // There is no need to take the sqrt
}



typedef struct {
    Nodes* clusters;
    ArrayInt* cluster_list;
    double cost;
} KmeansResult;

void freeKmeansResult(KmeansResult* ptr) {
    if (ptr) {
        if (ptr->clusters) {
            freeNodes(ptr->clusters);
        }
        if (ptr->cluster_list) {
            freeArrayInt(ptr->cluster_list);
        }
        ptr->cost = 0.0;
        free(ptr);
    }
}    

void printKmeansResult(KmeansResult* res, Nodes* datapoints) {
    if (res && res->clusters && res->clusters->list && res->cluster_list) {
        printf("Clusters - %d\n", res->clusters->n);
        printNodes(res->clusters);

        printf("Datapoints - %d\n", datapoints->n);
        for (int i = 0; i < datapoints->n; i++) {
            printf("[%lf", datapoints->list[i].arr[0]);
            for (int j = 1; j < datapoints->list[i].n; j++) {
                printf(",%lf", datapoints->list[i].arr[j]);
            }
            printf("] - %d\n", res->cluster_list->arr[i]);
        }

        printf("\nCost: %lf\n", res->cost);
    }
}



typedef ArrayDouble Node;

Nodes* createInitialClusters(const Nodes* const datapoints, int k) {
    Nodes* clusters = newNodes(k);

    for (int i = 0; i < k; i++) {
        int r = rand() % datapoints->n;
        Node* initial = &datapoints->list[r];

        int j = 0;

        // Check for duplicates
        while (j < i && !compareArrayDouble(initial, &clusters->list[j])) 
        {
            j++;
        }

        Node* tmp = newArrayDouble(initial->n, 0.0);
        clusters->list[i] = *tmp;
        free(tmp);

        copyArrayDouble(&clusters->list[i], initial);

        if (j < i) {
            // Avoid duplicates by adding a random distance to the selected point
            addArrayDouble(&clusters->list[i], rand_double(MIN_RAND, MAX_RAND));
        }
    }
    return clusters;
}

int assignAndCount(
        const Nodes* const datapoints,
        int k,
        ArrayInt* cluster_list,
        ArrayInt* cluster_count,
        Nodes* clusters,
        Nodes* pos_accumulator
    ) {

    int moves = 0;

    // Assign points to clusters and calculate cluster-point cost
    for (int i = 0; i < datapoints->n; i++) 
    {
        Node* node = &datapoints->list[i];

        double min = DBL_MAX;
        int which_cluster = -1;

        // Finding the closest cluster j for the node i
        for (int j = 0; j < k; j++) {
            double dist = distance(node, &clusters->list[j]);

            if (dist < min) {
                min = dist;
                which_cluster = j;
            }
        }

        if (which_cluster != -1) {
            if (cluster_list->arr[i] != which_cluster) {
                moves++;
                cluster_list->arr[i] = which_cluster;
            }

            sumArrayDouble(&pos_accumulator->list[which_cluster], node);
            cluster_count->arr[which_cluster]++;
        }
    }
    return moves;
}

KmeansResult* kmeans(const Nodes* const datapoints, int k) {

    Nodes* clusters = createInitialClusters(datapoints, k);

    double cluster_cost = 0.0;
    ArrayInt* cluster_list = newArrayInt(datapoints->n, -1);

    int iterations = 0;
    int moves;

    do {
        cluster_cost = 0.0;

        // Initialize accumulator
        Nodes* pos_accumulator = newNodes(k);
        for (int i = 0; i < k; i++) {
            int features_n = datapoints->list[i].n;

            Node* tmp = newArrayDouble(features_n, 0.0);
            pos_accumulator->list[i] = *tmp;
            free(tmp);
        }
        ArrayInt* cluster_count = newArrayInt(k, 0);

        moves = assignAndCount(datapoints, k, cluster_list, cluster_count, clusters, pos_accumulator);

        // Move clusters to the new positions
        for (int i = 0; i < k; i++) {

            // If the cluster is empty, move it randomly
            if (cluster_count->arr[i] == 0) {
                addArrayDouble(&clusters->list[i], rand_double(MIN_RAND, MAX_RAND));

            } else {

                // Take the average
                divArrayDouble(&pos_accumulator->list[i], (double)cluster_count->arr[i]); // The result is in pos_accumulator->list[i]

                cluster_cost += distance(&clusters->list[i], &pos_accumulator->list[i]);
                copyArrayDouble(&clusters->list[i], &pos_accumulator->list[i]);
            }
        }
        freeNodes(pos_accumulator);
        freeArrayInt(cluster_count);
        iterations++;

    } while ((iterations < MAX_ITER) && (cluster_cost > CUTOFF) && (moves != 0));

    // Calculate total cost
    double cost = 0.0;
    for (int i = 0; i < datapoints->n; i++) {
        int which = cluster_list->arr[i];
        Node cluster = clusters->list[which];
        double dist = distance(&cluster, &datapoints->list[i]);
        cost += dist;
    }

    KmeansResult* res = (KmeansResult*)malloc(sizeof(KmeansResult));

    res->clusters = clusters;
    res->cluster_list = cluster_list;
    res->cost = cost;

    return res;
}

// Select the run with the smallest cost for a given K
KmeansResult* min_kmeans(const Nodes* const datapoints, int k, int runs) {

    KmeansResult* res = NULL;
    double cost = DBL_MAX;

    for (int i = 0; i < runs; i++) {
        KmeansResult* tmp = kmeans(datapoints, k);
        if (tmp) {
            if (tmp->cost < cost) {

                // Drop old results
                if (res) {
                    freeKmeansResult(res);
                }

                res = tmp;
                cost = tmp->cost;

            } else {
                freeKmeansResult(tmp);
            }
        }
    }
    return res;
}
