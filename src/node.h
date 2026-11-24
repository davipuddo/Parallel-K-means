#pragma once

double distance (double* x, double* y, int n) {
    double sum = 0.0;
    if (x && y && n > 0) {
        for (int i = 0; i < n; i++) {
            double dx = (x[i] - y[i]);
            sum += dx * dx;
        }
    }
    return sum;     // There is no need to take the sqrt
}
