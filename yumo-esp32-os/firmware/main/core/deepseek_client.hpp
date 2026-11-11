#pragma once

#include <string>
#include <functional>
#include <mutex>
#include <ctime>

namespace core {

struct DeepSeekData {
    bool valid{false};
    bool isAvailable{true};
    bool usingPlatformToken{false};

    // Balance
    double totalBalance{0.0};
    double grantedBalance{0.0};
    double toppedUpBalance{0.0};
    std::string currency{"CNY"};

    // Token usage
    int64_t todayTokens{0};
    int64_t monthTokens{0};
    int64_t promptTokens{0};
    int64_t completionTokens{0};
    int64_t cacheHitTokens{0};
    double cacheHitRate{0.0};
    double todayCost{0.0};

    time_t lastUpdated{0};
    std::string errorMsg;
};

class DeepSeekClient {
public:
    static DeepSeekClient &instance();

    void init();
    bool hasCredentials() const;
    std::string getUserToken() const;
    std::string getApiKey() const;
    void setCredentials(const std::string &userToken, const std::string &apiKey);

    void fetchAsync(std::function<void(bool success)> onComplete = nullptr);
    bool isFetching() const;
    DeepSeekData getData() const;

private:
    DeepSeekClient();
    ~DeepSeekClient() = default;

    void loadFromNvs();
    void saveToNvs();
    bool performFetch();

    mutable std::mutex m_mutex;
    std::string m_userToken;
    std::string m_apiKey;
    bool m_fetching{false};
    DeepSeekData m_data;
};

} // namespace core
