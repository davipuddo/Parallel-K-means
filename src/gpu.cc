#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <limits>
#include <sycl/sycl.hpp>

#define K 2
#define N_ATTRIBUTES 5
#define N_CLASSES 2
#define MAX_ITERATIONS 30
#define LINE_LIMIT 256

/* Rows read from the dataset by default; override with argv[2] (0 = all). */
#define MAX_INSTANCES 0

#define DEFAULT_DATASET "../datasets/treino_balanced_big.csv"

#define WORK_GROUP_SIZE 256
#define TARGET_GROUPS 4096
#define N_SUMS (K * N_ATTRIBUTES)
#define N_ACC (N_SUMS + K)

static double *instances = NULL;
static int *labels = NULL;
static int *results = NULL;
static double centers[K * N_ATTRIBUTES];

static size_t n_instances = 0;

/*
 * Device state. The host arrays above stay live -- the CSV is parsed into them
 * and the final assignments are copied back into them -- while these are the
 * USM device allocations the kernels actually work on.
 */
static sycl::queue *q= NULL;
static double *d_instances = NULL;
static double *d_centers = NULL;
static int *d_results = NULL;
/*
 * Per-work-group partial reductions: N_SUMS centroid sums followed by K
 * cluster counts, one slice per group, which the host folds together. 
 */
static double *d_partials = NULL;
static double *h_partials = NULL;
static size_t n_groups = 0;

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

/*
 * Allocate USM device buffers and transfer data
 */
static int device_setup(void) {
    size_t groups = (n_instances + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
    n_groups = (groups < TARGET_GROUPS) ? groups : TARGET_GROUPS;
    if (n_groups == 0) {
        n_groups = 1;
    }

    d_instances = sycl::malloc_device<double>(n_instances * N_ATTRIBUTES, *q);
    d_centers = sycl::malloc_device<double>(K * N_ATTRIBUTES, *q);
    d_results = sycl::malloc_device<int>(n_instances, *q);
    d_partials = sycl::malloc_device<double>(n_groups * N_ACC, *q);

    if (!d_instances || !d_centers || !d_results || !d_partials) {
        fprintf(stderr, "Error: USM device allocation failed (need %.2f GB)\n",
                (double)(n_instances * N_ATTRIBUTES * sizeof(double)) / 1e9);
        return 0;
    }

    h_partials = (double *)malloc(n_groups * N_ACC * sizeof(double));
    if (!h_partials) {
        fprintf(stderr, "Error: out of memory for partial reductions\n");
        return 0;
    }

    q->memcpy(d_instances, instances,
                  n_instances * N_ATTRIBUTES * sizeof(double));
    q->memcpy(d_results, results, n_instances * sizeof(int));
    q->wait_and_throw();
    return 1;
}

static void device_free(void) {
    if (q) {
        sycl::free(d_instances, *q);
        sycl::free(d_centers, *q);
        sycl::free(d_results, *q);
        sycl::free(d_partials, *q);
    }
    free(h_partials);
    d_instances = NULL;
    d_centers = NULL;
    d_results = NULL;
    d_partials = NULL;
    h_partials = NULL;
}

// Assigns each instance to the nearest cluster center (one work-item per row)
static void assign_cluster(void) {
    const size_t n = n_instances;
    const double *inst = d_instances;
    const double *cen = d_centers;
    int *res = d_results;
    const double inf = std::numeric_limits<double>::infinity();

    q->parallel_for(sycl::range<1>(n), [=](sycl::id<1> idx) {
        const size_t i = idx[0];

        double min_dist = inf;
        int best_cluster = -1;

        for (int j = 0; j < K; j++) {
            double sum = 0.0;
            for (int a = 0; a < N_ATTRIBUTES; a++) {
                double diff = inst[i * N_ATTRIBUTES + a] - cen[j * N_ATTRIBUTES + a];
                sum += diff * diff;
            }

            if (sum < min_dist) {
                min_dist = sum;
                best_cluster = j;
            }
        }

        res[i] = best_cluster;
    });
}

static void update_centers(size_t *c) {
    const size_t n = n_instances;
    const double *inst = d_instances;
    const int *res = d_results;
    double *partials = d_partials;
    const size_t groups = n_groups;
    const size_t stride = groups * WORK_GROUP_SIZE;

    q->submit([&](sycl::handler &cgh) {
        sycl::local_accessor<double, 1> scratch(sycl::range<1>(WORK_GROUP_SIZE), cgh);

        cgh.parallel_for(
            sycl::nd_range<1>(groups * WORK_GROUP_SIZE, WORK_GROUP_SIZE),
            [=](sycl::nd_item<1> item) {
                double acc[N_ACC];
                for (int s = 0; s < N_ACC; s++) {
                    acc[s] = 0.0;
                }

                for (size_t i = item.get_global_id(0); i < n; i += stride) {
                    const int cluster = res[i];
                    /*
                     * Masked rather than indexed by 'cluster': it keeps every
                     * acc[] subscript a compile-time constant, so the array
                     * stays in registers instead of spilling to scratch.
                     */
                    for (int j = 0; j < K; j++) {
                        const double hit = (cluster == j) ? 1.0 : 0.0;
                        acc[N_SUMS + j] += hit;
                        for (int a = 0; a < N_ATTRIBUTES; a++) {
                            acc[j * N_ATTRIBUTES + a] +=
                                hit * inst[i * N_ATTRIBUTES + a];
                        }
                    }
                }

                const auto group = item.get_group();
                const size_t lid = item.get_local_id(0);
                const size_t g = item.get_group_linear_id();

                /* One tree reduction per accumulator, reusing the scratch. */
                for (int s = 0; s < N_ACC; s++) {
                    scratch[lid] = acc[s];
                    sycl::group_barrier(group);

                    for (size_t off = WORK_GROUP_SIZE / 2; off > 0; off >>= 1) {
                        if (lid < off) {
                            scratch[lid] += scratch[lid + off];
                        }
                        sycl::group_barrier(group);
                    }

                    if (lid == 0) {
                        partials[g * N_ACC + s] = scratch[0];
                    }
                }
            });
    });

    q->memcpy(h_partials, d_partials, n_groups * N_ACC * sizeof(double));
    q->wait_and_throw();

    double acc[N_ACC];
    for (int s = 0; s < N_ACC; s++) {
        acc[s] = 0.0;
    }

    for (size_t g = 0; g < n_groups; g++) {
        for (int s = 0; s < N_ACC; s++) {
            acc[s] += h_partials[g * N_ACC + s];
        }
    }

    const double *sum = acc;
    for (int i = 0; i < K; i++) {
        c[i] = (size_t)(acc[N_SUMS + i] + 0.5);
    }

    for (int i = 0; i < K; i++) {
        for (int j = 0; j < N_ATTRIBUTES; j++) {
            int idx = i * N_ATTRIBUTES + j;
            centers[idx] = (c[i] != 0) ? (sum[idx] / (double)c[i]) : 0.0;
        }
    }

    q->memcpy(d_centers, centers, K * N_ATTRIBUTES * sizeof(double));
    q->wait_and_throw();
}

// Main k-means loop: initializes centers, assigns clusters, updates centers
static int kmeans(void) {
    initialize_centers();

    if (!device_setup()) {
        return 0;
    }
    q->memcpy(d_centers, centers, K * N_ATTRIBUTES * sizeof(double));
    q->wait_and_throw();

    size_t c[K];

    for (int i = 0; i < MAX_ITERATIONS; i++) {
        assign_cluster();
        update_centers(c);
    }

    /*
     * Bring the assignments home, so the timed region ends in the same state
     * the sequential version ends in: final labels readable on the host.
     */
    q->memcpy(results, d_results, n_instances * sizeof(int));
    q->wait_and_throw();
    return 1;
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

    int status = EXIT_SUCCESS;

    try {
        sycl::queue queue{sycl::gpu_selector_v, sycl::property::queue::in_order()};
        q= &queue;

        printf("Device: %s\n",
               q->get_device().get_info<sycl::info::device::name>().c_str());

        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        int ok = kmeans();

        clock_gettime(CLOCK_MONOTONIC, &end);

        if (!ok) {
            device_free();
            free_instances();
            return EXIT_FAILURE;
        }

        double elapsed = (double)(end.tv_sec - start.tv_sec)
                       + (double)(end.tv_nsec - start.tv_nsec) / 1e9;

        double accuracy = evaluate_accuracy();

        printf("Time: %.6f s\n", elapsed);
        printf("Accuracy: %.4f%%\n", accuracy * 100.0);

        device_free();
    } catch (const sycl::exception &e) {
        fprintf(stderr, "SYCL error: %s\n", e.what());
        status = EXIT_FAILURE;
    }

    free_instances();
    return status;
}
