#pragma once
#ifndef SENSORS_H
#define SENSORS_H

// Function prototypes for the APIs
int getTemperature(void);
int getPressure(void);
int getHeight(void);

// Function to initialize the random number generator (call once at startup)
void initializeSensors(void);

#endif // for SENSORS_H 