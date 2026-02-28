#ifndef CHARTPROJECTOR_HPP
#define CHARTPROJECTOR_HPP

#include "BMSData.hpp"
#include <cstdint>

class ChartProjector {
public:
    // データを事前計算（書き込み）するため非const参照に変更
    void init(BMSData& data) { 
        bmsData = &data; 
        calculateAllTimestamps();
    }

    double getMsFromY(int64_t target_y) const;
    int64_t getYFromMs(double cur_ms) const;
    double getBpmFromMs(double cur_ms) const;

    double getDurationMs(int64_t y_start, int64_t y_end) const {
        return getMsFromY(y_end) - getMsFromY(y_start);
    }

private:
    void calculateAllTimestamps(); // 【追加】初期化時に全要素のmsを計算する
    BMSData* bmsData = nullptr;    // 非constに変更
};

#endif