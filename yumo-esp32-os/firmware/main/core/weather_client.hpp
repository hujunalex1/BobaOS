#pragma once

#include <string>

namespace core {

// Last successful observation plus the state of the current sync.
struct WeatherSnapshot {
    bool configured{false};
    bool valid{false};
    bool fetching{false};

    std::string city;
    std::string text;      // 多云 / 晴 / 小雨 …
    int temp{0};
    int feels{0};
    int tempMax{0};
    int tempMin{0};
    int rainChance{-1};    // max pop over the next few hours, -1 = unknown
    std::string tomorrow;  // "24°"
    std::string dayAfter;  // "26°"
    std::string updated;   // "14:20"

    long long fetchedAt{0};
    std::string error;
};

// QWeather (和风天气) v7 client. Config lives in NVS so it survives flashing
// that keeps nvs, and the last observation is cached so the app opens offline.
class WeatherClient {
public:
    static WeatherClient &instance();

    void init();

    bool configured() const;
    std::string host() const;
    std::string apiKey() const;
    std::string latitude() const;
    std::string longitude() const;
    std::string city() const;
    void setConfig(const std::string &host, const std::string &apiKey,
                   const std::string &latitude, const std::string &longitude,
                   const std::string &city);

    void fetchAsync();
    bool isFetching() const;
    bool stale(int seconds = 1800) const;
    WeatherSnapshot snapshot() const;

private:
    WeatherClient() = default;
};

} // namespace core
