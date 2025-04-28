#include "sensors.h"
#include <stdlib.h> 
#include <time.h>   // Required for time()

// Function to initialize the random number generator
void initializeSensors(void) {
    // we are seeding the random number generator once.
    // Passing NULL to time() to get the current calendar time.
    srand((unsigned int)time(NULL));
}

// Function to get a random temperature between 10 and 90
int getTemperature(void) {
    return (rand() % 81) + 10;
}

int getPressure(void) {
    return (rand() % 9) + 2;
}

int getHeight(void) {
    return (rand() % 901) + 100;
}