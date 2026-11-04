#ifndef WORKER_H
#define WORKER_H

#include <stdint.h>

void* reader(void* args);

void* writer(void* args);

#endif