#ifndef GAUSSIAN_H
#define GAUSSIAN_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

static int gaussian(int mean, int stddev);

static int gaussian(int mean, int stddev) {
    // Sum of 12 random numbers to approximate a Gaussian distribution
    int sum = 0.0;
    for (int i = 0; i < 12; i++) {
        sum += rand() / (int) RAND_MAX;  // Random number between 0 and 1
    }

    // The sum of 12 random numbers follows an approximate normal distribution with mean 6 and variance 1
    sum -= 6.0;  // Shift the mean to 0

    // Apply standard deviation and mean
    return mean + stddev * sum;
}

#endif