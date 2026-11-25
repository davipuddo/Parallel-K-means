#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define K 2
#define N_ATTRIBUTES 5
#define N_CLASSES 2
#define MAX_ITERATIONS 30
#define LINE_LIMIT 256

/* Rows read from the dataset by default; override with argv[2] (0 = all). */
#define MAX_INSTANCES 0

#define DEFAULT_DATASET "../datasets/treino_balanced_big.csv"

/* Dataset: n_instances rows of N_ATTRIBUTES attributes, stored row-major. */
static double *instances = NULL;
/* Ground-truth class of each instance (last CSV column), used only to score. */
static int *labels = NULL;
/* Cluster assigned to each instance by k-means. */
static int *results = NULL;
static double centers[K * N_ATTRIBUTES];

static size_t n_instances = 0;

/*
 * Counts the data rows of a CSV file, ignoring the header and blank lines.
 * Stops as soon as 'limit' rows have been seen (limit == 0 means no limit),
 * so capped runs do not pay for a full scan of the file.
 */
static size_t count_rows(const char *path, size_t limit) {
    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "Error: could not open '%s'\n", path);
        return (size_t)-1;
    }

    char line[LINE_LIMIT];
    size_t rows = 0;

    if (!fgets(line, sizeof(line), file)) { /* header */
        fclose(file);
        return 0;
    }

    while ((limit == 0 || rows < limit) && fgets(line, sizeof(line), file)) {
        if (line[0] != '\n' && line[0] != '\r' && line[0] != '\0') {
            rows++;
        }
    }

    fclose(file);
    return rows;
}

// Loads instances and their labels from a CSV file into the global arrays
static int read_csv(const char *path, size_t max_instances) {
    size_t rows = count_rows(path, max_instances);
    if (rows == (size_t)-1) {
        return 0; /* count_rows already reported the failure */
    }
    if (rows == 0) {
        fprintf(stderr, "Error: no data rows found in '%s'\n", path);
        return 0;
    }

    instances = (double *)malloc(rows * N_ATTRIBUTES * sizeof(double));
    labels = (int *)malloc(rows * sizeof(int));
    results = (int *)malloc(rows * sizeof(int));
    if (!instances || !labels || !results) {
        fprintf(stderr, "Error: out of memory for %zu instances\n", rows);
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "Error: could not open '%s'\n", path);
        return 0;
    }

    char line[LINE_LIMIT];
    size_t count = 0;

    if (!fgets(line, sizeof(line), file)) { /* skip header */
        fclose(file);
        return 0;
    }

    while (count < rows && fgets(line, sizeof(line), file)) {
        char *p = line;
        int ok = 1;

        for (int j = 0; j < N_ATTRIBUTES; j++) {
            char *end = NULL;
            double value = strtod(p, &end);

            if (end == p) {
                fprintf(stderr, "Error converting attribute %d on row %zu\n", j, count);
                value = 0.0;
                ok = 0;
            }

            instances[count * N_ATTRIBUTES + j] = value;

            p = end;
            if (*p == ',') {
                p++;
            }
        }

        char *end = NULL;
        long label = strtol(p, &end, 10);
        if (end == p || label < 0 || label >= N_CLASSES) {
            if (ok) {
                fprintf(stderr, "Error converting label on row %zu\n", count);
            }
            label = 0;
        }
        labels[count] = (int)label;

        results[count] = -1;
        count++;
    }

    fclose(file);

    n_instances = count;
    printf("Loaded instances: %zu\n", n_instances);
    return count > 0;
}

static void free_instances(void) {
    free(instances);
    free(labels);
    free(results);
    instances = NULL;
    labels = NULL;
    results = NULL;
}

// Initializes cluster centers randomly from the loaded instances
static void initialize_centers(void) {
    srand(3);
    for (int i = 0; i < K; i++) {
        size_t index = (size_t)(rand() % (int)n_instances);
        for (int j = 0; j < N_ATTRIBUTES; j++) {
            centers[i * N_ATTRIBUTES + j] = instances[index * N_ATTRIBUTES + j];
        }
    }
}

// Counts the number of instances assigned to each cluster
static void count_instances(size_t *c) {
    for (int i = 0; i < K; i++) {
        c[i] = 0;
    }
    for (size_t i = 0; i < n_instances; i++) {
        c[results[i]]++;
    }
}

// Assigns each instance to the nearest cluster center (sequential version)
static void assign_cluster(void) {
    for (size_t i = 0; i < n_instances; i++) {
        double min_dist = INFINITY;
        int best_cluster = -1;

        for (int j = 0; j < K; j++) {
            double sum = 0.0;
            for (int n = 0; n < N_ATTRIBUTES; n++) {
                double diff = instances[i * N_ATTRIBUTES + n] - centers[j * N_ATTRIBUTES + n];
                sum += diff * diff;
            }

            if (sum < min_dist) {
                min_dist = sum;
                best_cluster = j;
            }
        }

        results[i] = best_cluster;
    }
}

// Updates cluster centers based on assigned instances
static void update_centers(size_t *c) {
    count_instances(c);

    double sum[K * N_ATTRIBUTES];
    for (int i = 0; i < K * N_ATTRIBUTES; i++) {
        sum[i] = 0.0;
    }

    for (size_t j = 0; j < n_instances; j++) {
        int cluster = results[j];
        for (int a = 0; a < N_ATTRIBUTES; a++) {
            sum[cluster * N_ATTRIBUTES + a] += instances[j * N_ATTRIBUTES + a];
        }
    }

    for (int i = 0; i < K; i++) {
        for (int j = 0; j < N_ATTRIBUTES; j++) {
            int idx = i * N_ATTRIBUTES + j;
            centers[idx] = (c[i] != 0) ? (sum[idx] / (double)c[i]) : 0.0;
        }
    }
}

// Main k-means loop: initializes centers, assigns clusters, updates centers
static void kmeans(void) {
    initialize_centers();
    size_t c[K];

    for (int i = 0; i < MAX_ITERATIONS; i++) {
        assign_cluster();
        update_centers(c);
    }
}

/*
 * Accuracy of an unsupervised clustering against the ground-truth labels.
 *
 * Cluster ids carry no class meaning, so each cluster is mapped to the class
 * that is most frequent among its members (majority vote); the instances that
 * agree with that mapping count as correct. This is the best accuracy
 * achievable over all cluster->class mappings.
 */
static double evaluate_accuracy(void) {
    size_t contingency[K][N_CLASSES];

    for (int i = 0; i < K; i++) {
        for (int j = 0; j < N_CLASSES; j++) {
            contingency[i][j] = 0;
        }
    }

    for (size_t i = 0; i < n_instances; i++) {
        int cluster = results[i];
        if (cluster >= 0 && cluster < K) {
            contingency[cluster][labels[i]]++;
        }
    }

    size_t correct = 0;
    for (int i = 0; i < K; i++) {
        size_t best = 0;
        int best_class = 0;
        for (int j = 0; j < N_CLASSES; j++) {
            if (contingency[i][j] > best) {
                best = contingency[i][j];
                best_class = j;
            }
        }
        correct += best;

        size_t total = 0;
        for (int j = 0; j < N_CLASSES; j++) {
            total += contingency[i][j];
        }
        printf("Cluster %d: %zu instances -> class %d\n", i, total, best_class);
    }

    return (n_instances > 0) ? ((double)correct / (double)n_instances) : 0.0;
}

int main(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : DEFAULT_DATASET;
    size_t max_instances = (argc > 2) ? strtoul(argv[2], NULL, 10) : MAX_INSTANCES;

    if (!read_csv(path, max_instances)) {
        return EXIT_FAILURE;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    kmeans();

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (double)(end.tv_sec - start.tv_sec)
                   + (double)(end.tv_nsec - start.tv_nsec) / 1e9;

    double accuracy = evaluate_accuracy();

    printf("Time: %.6f s\n", elapsed);
    printf("Accuracy: %.4f%%\n", accuracy * 100.0);

    free_instances();
    return EXIT_SUCCESS;
}
