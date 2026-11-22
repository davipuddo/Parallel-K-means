#include <cfloat>
#include <cstdlib>
#include <vector>

#include "node.hpp"


const double CUTOFF = 1e-4;

const int MAX_ITER = 500;

const double MIN_RAND = 1.0f;
const double MAX_RAND = 100.0f;


inline double rand_double(int min, int max) {
    int INT = (rand() % (int)(max - min + 1.0)) + min;
    double DEC = ((double)rand() / (double)RAND_MAX);
    return INT + DEC;
}


struct KmeansResult {

    Nodes clusters;
    std::vector<int> cluster_list;
    double cost;

    KmeansResult() {}

    KmeansResult(Nodes clusters, std::vector<int> cluster_list, double cost)
        : clusters(clusters), cluster_list(cluster_list), cost(cost) {}

    ~KmeansResult() = default;

};


KmeansResult kmeans(const Nodes& datapoints, int k) {

    Nodes clusters;

    int n = datapoints.size();

    // Create clusters from k random datapoints
    for (int i = 0; i < k; i++) {
        int r = rand() % n;
        Node initial = datapoints[r];

        bool already_in = false;
        int j = 0;

        while (j < i && !already_in) {
            if (initial == clusters[j]) {
                already_in = true;
            }
            j++;
        }

        if (!already_in) {
            clusters.add(initial);
        } else {

            // Avoid duplicates by adding a random distance to the selected point
            Pos pos = initial.pos + rand_double(MIN_RAND, MAX_RAND);
            clusters.add(Node(pos));
        }
    }
    
    double cluster_cost = 0.0;
    std::vector<int> cluster_list(n, -1);

    int iterations = 0;
    int moves;

    do {

        cluster_cost = 0.0;

        std::vector<Pos> pos_accumulator(k);
        std::vector<int> pos_count(k, 0);

        moves = 0;

        // Assign points to clusters and calculate cluster-point cost
        for (int i = 0; i < n; i++) {

            Node node = datapoints[i];

            double min = DBL_MAX;
            int which_cluster = -1;

            // Finding the closest cluster j for the node i
            for (int j = 0; j < k; j++) {
                double dist = node.distance(clusters[j]);

                if (dist < min) {
                    min = dist;
                    which_cluster = j;
                }
            }
            
            if (cluster_list[i] != which_cluster) {
                moves++;
                cluster_list[i] = which_cluster;
            }

            pos_accumulator[which_cluster] += node.pos;
            pos_count[which_cluster]++;
        }

        // Move clusters to the new positions
        for (int i = 0; i < k; i++) {

            // If the cluster is empty, move it randomly
            if (pos_count[i] == 0) {
                clusters[i].pos += rand_double(MIN_RAND, MAX_RAND);

            } else {

                Pos new_pos = (pos_accumulator[i] / pos_count[i]); // Average
                cluster_cost += clusters[i].pos.distance(new_pos);
                clusters[i].pos = new_pos;
            }
        }
        iterations++;

    } while ((iterations < MAX_ITER) && (cluster_cost > CUTOFF) && (moves != 0));

    // Calculate total cost
    double cost = 0.0;
    for (int i = 0; i < n; i++) {
        int which = cluster_list[i];
        Node cluster = clusters[which];
        double dist = cluster.distance(datapoints[i]);
        cost += dist;
    }

    return KmeansResult(clusters, cluster_list, cost);
}

KmeansResult min_kmeans(const Nodes& datapoints, int k, int runs) {

    KmeansResult res;
    double cost = DBL_MAX;

    for (int i = 0; i < runs; i++) {
        KmeansResult tmp = kmeans(datapoints, k);
        if (tmp.cost < cost) {
            res = tmp;
            cost = tmp.cost;
        }
    }
    return res;
}
