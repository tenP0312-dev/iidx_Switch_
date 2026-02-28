#ifndef CHARTPROJECTOR_HPP
#define CHARTPROJECTOR_HPP

#include "BMSData.hpp"
#include <cstdint>

class ChartProjector {
public:
    void init(const BMSData& data) { bmsData = &data; }

    double getMsFromY(int64_t target_y) const;
    int64_t getYFromMs(double cur_ms) const;
    double getBpmFromMs(double cur_ms) const;

    double getDurationMs(int64_t y_start, int64_t y_end) const {
        return getMsFromY(y_end) - getMsFromY(y_start);
    }

private:
    const BMSData* bmsData = nullptr;
};

#endif



