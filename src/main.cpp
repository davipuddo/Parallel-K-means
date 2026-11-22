#include "node.hpp"
#include "kmeans.hpp"
#include <cfloat>

const double K_CUTOFF = 0.3;
const int RUNS = 100;

int parseArgs(int argc, const char** argv) {
    int k = 0;
    if (argc == 2) {
        k = atoi(argv[1]);
        if (k < 0) {
            std::cerr << "Error: K should be a positive value!" << std::endl;
        }
    }
    return k;
}

int main (int argc, const char** argv) {

    srand(time(NULL));

    // Read dataset and instanciate nodes
    const Nodes datapoints = Nodes("./datasets/test_dataset.txt");
    int n = datapoints.size();
    
    if (n == 0) {
        std::cerr << "Error: dataset is empty, aborting!" << std::endl;
        return -1;
    }

    int k = parseArgs(argc, argv);

    if (k > n) {
        std::cerr << "Error: K is larger than the number of entries, aborting!\n";
        std::cerr << "K = " << k << "\n";
        std::cerr << "N = " << n << "\n";
        return -1;
    }

    // Run K-means
    KmeansResult result;

    if (k != 0) {
        result = min_kmeans(datapoints, k, RUNS);

    } else {        // If K is not defined, find it dynamically

        bool stop = false;
        double cost = DBL_MAX;
        int k = 2;

        while (!stop && k < n) {

            result = min_kmeans(datapoints, k, RUNS);
            double new_cost = result.cost;

            double relative_cost = (cost - new_cost) / cost;
            cost = new_cost;

            if (relative_cost < K_CUTOFF) {
                stop = true;
            } else {
                k++;
            }
        }
    }


    // Display results

    std::cout << "K = " << result.clusters.size() << "\n";

    std::cout << "\nClusters -\n";
    result.clusters.print();

    std::cout << "\nPoints -\n";
    for (int i = 0; i < n; i++) {
        double x = datapoints[i].pos.x;
        double y = datapoints[i].pos.y;
        std::cout << "(" << x << ", " << y << ") - " << result.cluster_list[i] << "\n";
    }
    std::cout << "\nCost: " << result.cost << std::endl;

    return 0;
}
