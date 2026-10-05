#pragma once

#include <cstdint>

namespace sensors {
struct Reading {
    int raw = -1;
    int millivolts = -1;
};
extern Reading regulator33;
extern Reading regulator5;
extern Reading input;
void initialize();
bool update(uint64_t now);
int temperatureTenths(const Reading &reading);
} // namespace sensors
