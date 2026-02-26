#ifndef BMSONLOADER_HPP
#define BMSONLOADER_HPP

#include <string>
#include <functional> // 追加：コールバック用
#include "BMSData.hpp"
#include "json.hpp"

class BmsonLoader {
public:
    // 第2引数に進捗報告用のコールバックを追加 (デフォルトはnullptrで互換性維持)
    static BMSData load(const std::string& path, std::function<void(float)> onProgress = nullptr);
    static BMSHeader loadHeader(const std::string& path);
};

#endif