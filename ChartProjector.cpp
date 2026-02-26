#include "ChartProjector.hpp"

double ChartProjector::getMsFromY(int64_t target_y) const {
    if (!bmsData) return 0.0;
    double total_ms = 0.0, current_bpm = bmsData->header.bpm;
    int64_t current_y = 0, res = bmsData->header.resolution;
    for (const auto& ev : bmsData->bpm_events) {
        if (ev.y > target_y) break;
        total_ms += (double)(ev.y - current_y) * (60000.0 / (current_bpm * res));
        current_y = ev.y; current_bpm = ev.bpm;
    }
    return total_ms + (double)(target_y - current_y) * (60000.0 / (current_bpm * res));
}

int64_t ChartProjector::getYFromMs(double cur_ms) const {
    if (!bmsData) return 0;
    double res = bmsData->header.resolution;
    if (cur_ms < 0) return (int64_t)(cur_ms * (bmsData->header.bpm * res / 60000.0));
    double elapsed_ms = 0, current_bpm = bmsData->header.bpm;
    int64_t current_y = 0;
    for (const auto& ev : bmsData->bpm_events) {
        double step = (double)(ev.y - current_y) * (60000.0 / (current_bpm * res));
        if (elapsed_ms + step > cur_ms) break;
        elapsed_ms += step; current_y = ev.y; current_bpm = ev.bpm;
    }
    return current_y + (int64_t)((cur_ms - elapsed_ms) * (current_bpm * res / 60000.0));
}

double ChartProjector::getBpmFromMs(double cur_ms) const {
    if (!bmsData) return 120.0;
    if (cur_ms < 0) return bmsData->header.bpm;
    double elapsed_ms = 0, current_bpm = bmsData->header.bpm, res = bmsData->header.resolution;
    int64_t current_y = 0;
    for (const auto& ev : bmsData->bpm_events) {
        double step = (double)(ev.y - current_y) * (60000.0 / (current_bpm * res));
        if (elapsed_ms + step > cur_ms) break;
        elapsed_ms += step; current_y = ev.y; current_bpm = ev.bpm;
    }
    return current_bpm;
}