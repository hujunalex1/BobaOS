#include "weather_client.hpp"

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "miniz.h"
#include "nvs.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <vector>

namespace core {
namespace {

constexpr const char *TAG = "Weather";
constexpr const char *kNvsConfig = "weather";
constexpr const char *kNvsCache = "weather_cache";

std::mutex g_mu;
WeatherSnapshot g_snap;
bool g_fetching = false;
bool g_initialized = false;

std::string trimCopy(const std::string &in) {
    size_t start = 0;
    while (start < in.size() && (in[start] == ' ' || in[start] == '\t' || in[start] == '\r' || in[start] == '\n')) ++start;
    size_t end = in.size();
    while (end > start && (in[end - 1] == ' ' || in[end - 1] == '\t' || in[end - 1] == '\r' || in[end - 1] == '\n')) --end;
    return in.substr(start, end - start);
}

std::string cleanHost(const std::string &in) {
    std::string s = trimCopy(in);
    if (s.rfind("https://", 0) == 0) s.erase(0, 8);
    else if (s.rfind("http://", 0) == 0) s.erase(0, 7);
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

// QWeather v1: per-account API host, coordinates instead of LocationID, and the
// credential travels in X-QW-Api-Key (the public devapi host is being retired).
struct Config {
    std::string host;                 // e.g. abcxyz.qweatherapi.com
    std::string key;
    std::string lat{"30.27"};
    std::string lon{"120.16"};
    std::string city{"杭州"};
};
Config g_cfg;

std::string nvsRead(const char *ns, const char *key) {
    nvs_handle_t handle;
    std::string out;
    if (nvs_open(ns, NVS_READONLY, &handle) != ESP_OK) return out;
    size_t len = 0;
    if (nvs_get_str(handle, key, nullptr, &len) == ESP_OK && len > 0 && len < 512) {
        std::string value(len, '\0');
        if (nvs_get_str(handle, key, value.data(), &len) == ESP_OK) {
            value.resize(std::strlen(value.c_str()));
            out = std::move(value);
        }
    }
    nvs_close(handle);
    return out;
}

int nvsReadInt(const char *ns, const char *key, int fallback) {
    nvs_handle_t handle;
    int32_t value = fallback;
    if (nvs_open(ns, NVS_READONLY, &handle) != ESP_OK) return fallback;
    if (nvs_get_i32(handle, key, &value) != ESP_OK) value = fallback;
    nvs_close(handle);
    return value;
}

void nvsWrite(const char *ns, const char *key, const std::string &value) {
    nvs_handle_t handle;
    if (nvs_open(ns, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_str(handle, key, value.c_str());
    nvs_commit(handle);
    nvs_close(handle);
}

void nvsWriteInt(const char *ns, const char *key, int value) {
    nvs_handle_t handle;
    if (nvs_open(ns, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_i32(handle, key, value);
    nvs_commit(handle);
    nvs_close(handle);
}

// QWeather gzips its JSON even when the client asks for identity, so the body
// has to be inflated. ESP32-S3 exposes miniz's tinfl in ROM: no library, no
// flash cost. The gzip wrapper is stripped here; the trailer (CRC32/ISIZE) is
// not needed for parsing.
bool gunzipBody(const std::string &in, std::string &out) {
    if (in.size() < 18) return false;
    if (static_cast<unsigned char>(in[0]) != 0x1f || static_cast<unsigned char>(in[1]) != 0x8b) return false;
    if (static_cast<unsigned char>(in[2]) != 8) return false;  // must be deflate

    const unsigned char flags = static_cast<unsigned char>(in[3]);
    size_t pos = 10;
    if (flags & 0x04) {  // FEXTRA
        if (pos + 2 > in.size()) return false;
        const size_t extra = static_cast<unsigned char>(in[pos]) |
                             (static_cast<size_t>(static_cast<unsigned char>(in[pos + 1])) << 8);
        pos += 2 + extra;
        if (pos >= in.size()) return false;
    }
    if (flags & 0x08) {  // FNAME
        while (pos < in.size() && in[pos] != 0) ++pos;
        ++pos;
        if (pos >= in.size()) return false;
    }
    if (flags & 0x10) {  // FCOMMENT
        while (pos < in.size() && in[pos] != 0) ++pos;
        ++pos;
        if (pos >= in.size()) return false;
    }
    if (flags & 0x02) pos += 2;  // FHCRC
    if (pos >= in.size()) return false;

    // tinfl_decompressor is ~11 KB; keeping it and the output buffer off the
    // worker stack prevents FreeRTOS stack-canary panics inside tinfl_decompress_mem_to_mem.
    constexpr size_t kCapacity = 16384;
    auto allocBuf = [](size_t bytes) -> void * {
        void *p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        return p ? p : heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    };
    auto *decomp = static_cast<tinfl_decompressor *>(allocBuf(sizeof(tinfl_decompressor)));
    auto *buffer = static_cast<unsigned char *>(allocBuf(kCapacity));
    if (!decomp || !buffer) {
        if (decomp) heap_caps_free(decomp);
        if (buffer) heap_caps_free(buffer);
        ESP_LOGE(TAG, "no buffer for gunzip");
        return false;
    }

    tinfl_init(decomp);
    size_t inLen = in.size() - pos;
    size_t outLen = kCapacity;
    const tinfl_status st = tinfl_decompress(
        decomp,
        reinterpret_cast<const mz_uint8 *>(in.data() + pos), &inLen,
        buffer, buffer, &outLen,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    heap_caps_free(decomp);
    if (st != TINFL_STATUS_DONE || outLen == 0) {
        ESP_LOGW(TAG, "gunzip failed status=%d out=%u", static_cast<int>(st), static_cast<unsigned>(outLen));
        heap_caps_free(buffer);
        return false;
    }
    out.assign(reinterpret_cast<const char *>(buffer), outLen);
    heap_caps_free(buffer);
    return true;
}

std::string describeBadBody(const std::string &body) {
    if (body.empty()) return "空响应";
    if (body.size() > 1 && static_cast<unsigned char>(body[0]) == 0x1f &&
        static_cast<unsigned char>(body[1]) == 0x8b) {
        return "响应被 gzip 压缩";
    }
    std::string head;
    for (char c : body) {
        if (head.size() >= 10) break;
        head += (c >= 32 && c < 127) ? c : '.';
    }
    return "解析失败:" + head;
}

bool httpGet(const std::string &url, const std::string &apiKey, std::string &outBody, int &status) {
    outBody.clear();
    status = 0;
    esp_http_client_config_t config{};
    config.url = url.c_str();
    config.method = HTTP_METHOD_GET;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 12000;
    config.buffer_size = 2048;
    config.buffer_size_tx = 2048;
    config.skip_cert_common_name_check = false;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return false;

    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    esp_http_client_set_header(client, "X-QW-Api-Key", apiKey.c_str());
    esp_http_client_set_header(client, "User-Agent", "BobaOS/1.0 (ESP32-S3)");

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return false;
    }
    esp_http_client_fetch_headers(client);
    status = esp_http_client_get_status_code(client);

    char chunk[512];
    while (true) {
        const int len = esp_http_client_read(client, chunk, sizeof(chunk));
        if (len <= 0) break;
        outBody.append(chunk, len);
        if (outBody.size() > 16384) break;
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (outBody.size() > 2 && static_cast<unsigned char>(outBody[0]) == 0x1f &&
        static_cast<unsigned char>(outBody[1]) == 0x8b) {
        std::string inflated;
        if (gunzipBody(outBody, inflated)) {
            ESP_LOGI(TAG, "gunzip %u -> %u bytes", static_cast<unsigned>(outBody.size()),
                     static_cast<unsigned>(inflated.size()));
            outBody = std::move(inflated);
        }
    }
    ESP_LOGI(TAG, "GET %s -> HTTP %d, %u bytes", url.c_str(), status,
             static_cast<unsigned>(outBody.size()));
    return status >= 200 && status < 300;
}

std::string failureText(int status) {
    switch (status) {
    case 0: return "网络不可用";
    case 401: return "401 Key 无效";
    case 403: return "403 Host 不匹配";
    case 404: return "404 接口或位置错误";
    case 429: return "调用额度已用完";
    default: return "HTTP " + std::to_string(status);
    }
}

// v1 wraps every number in {"value": x, "unit": "..."}.
double valueOf(cJSON *object) {
    if (!object) return 0.0;
    if (cJSON_IsNumber(object)) return object->valuedouble;
    cJSON *value = cJSON_GetObjectItem(object, "value");
    return value && cJSON_IsNumber(value) ? value->valuedouble : 0.0;
}

cJSON *child(cJSON *object, const char *key) { return object ? cJSON_GetObjectItem(object, key) : nullptr; }


// Turns an unexpected payload into something readable on a 240px footer, so a
// wrong content encoding can be diagnosed without a serial console.

void saveCache(const WeatherSnapshot &snap) {
    nvsWrite(kNvsCache, "text", snap.text);
    nvsWrite(kNvsCache, "city", snap.city);
    nvsWrite(kNvsCache, "updated", snap.updated);
    nvsWrite(kNvsCache, "tomorrow", snap.tomorrow);
    nvsWrite(kNvsCache, "after", snap.dayAfter);
    nvsWriteInt(kNvsCache, "temp", snap.temp);
    nvsWriteInt(kNvsCache, "feels", snap.feels);
    nvsWriteInt(kNvsCache, "max", snap.tempMax);
    nvsWriteInt(kNvsCache, "min", snap.tempMin);
    nvsWriteInt(kNvsCache, "rain", snap.rainChance);
    nvsWriteInt(kNvsCache, "at", static_cast<int>(snap.fetchedAt));
}

void loadCache(WeatherSnapshot &snap) {
    const std::string updated = nvsRead(kNvsCache, "updated");
    const int at = nvsReadInt(kNvsCache, "at", 0);
    if (updated.empty() || at == 0) return;
    snap.text = nvsRead(kNvsCache, "text");
    snap.city = nvsRead(kNvsCache, "city");
    snap.updated = updated;
    snap.tomorrow = nvsRead(kNvsCache, "tomorrow");
    snap.dayAfter = nvsRead(kNvsCache, "after");
    snap.temp = nvsReadInt(kNvsCache, "temp", 0);
    snap.feels = nvsReadInt(kNvsCache, "feels", 0);
    snap.tempMax = nvsReadInt(kNvsCache, "max", 0);
    snap.tempMin = nvsReadInt(kNvsCache, "min", 0);
    snap.rainChance = nvsReadInt(kNvsCache, "rain", -1);
    snap.fetchedAt = at;
    snap.valid = true;
}

bool fetchAll(const Config &cfg, WeatherSnapshot &snap, std::string &error);

void worker(void *arg) {
    auto cfg = *static_cast<Config *>(arg);
    delete static_cast<Config *>(arg);

    WeatherSnapshot snap;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        snap = g_snap;
    }
    snap.fetching = true;

    ESP_LOGI(TAG, "fetch start %s", cfg.host.c_str());
    std::string error;
    const bool ok = fetchAll(cfg, snap, error);
    ESP_LOGI(TAG, "fetch done ok=%d err=%s", ok ? 1 : 0, error.c_str());

    {
        std::lock_guard<std::mutex> lock(g_mu);
        snap.fetching = false;
        snap.error = ok ? std::string() : error;
        if (ok) {
            snap.valid = true;
            snap.fetchedAt = static_cast<long long>(std::time(nullptr));
        }
        g_snap = snap;
        g_fetching = false;
    }
    if (ok) saveCache(snap);
    vTaskDelete(nullptr);
}

bool fetchAll(const Config &cfg, WeatherSnapshot &snap, std::string &error) {
    char url[320];
    std::string body;
    int status = 0;

    std::snprintf(url, sizeof(url), "https://%s/weather/v1/current/%s/%s?lang=zh",
                  cfg.host.c_str(), cfg.lat.c_str(), cfg.lon.c_str());
    if (!httpGet(url, cfg.key, body, status)) {
        error = failureText(status);
        if (!body.empty()) ESP_LOGW(TAG, "refused body: %.200s", body.c_str());
        return false;
    }

    cJSON *root = cJSON_Parse(body.c_str());
    if (!root) {
        ESP_LOGW(TAG, "unparsable body (%u bytes): %.120s", static_cast<unsigned>(body.size()), body.c_str());
        error = describeBadBody(body);
        return false;
    }
    const double temp = valueOf(child(root, "temperature"));
    const double feels = valueOf(child(root, "feelsLike"));
    snap.temp = static_cast<int>(std::lround(temp));
    snap.feels = static_cast<int>(std::lround(feels));
    cJSON *condition = child(root, "condition");
    cJSON *text = child(condition, "text");
    if (text && cJSON_IsString(text) && text->valuestring) snap.text = text->valuestring;
    cJSON_Delete(root);

    // One daily call covers min/max, today's rain probability and the next two
    // days' highs.
    ESP_LOGI(TAG, "current parsed temp=%d text=%s", snap.temp, snap.text.c_str());
    std::snprintf(url, sizeof(url), "https://%s/weather/v1/daily/%s/%s?days=3&lang=zh",
                  cfg.host.c_str(), cfg.lat.c_str(), cfg.lon.c_str());
    if (httpGet(url, cfg.key, body, status)) {
        root = cJSON_Parse(body.c_str());
        if (root) {
            cJSON *days = child(root, "days");
            cJSON *today = cJSON_IsArray(days) ? cJSON_GetArrayItem(days, 0) : nullptr;
            cJSON *tomorrow = cJSON_IsArray(days) ? cJSON_GetArrayItem(days, 1) : nullptr;
            cJSON *after = cJSON_IsArray(days) ? cJSON_GetArrayItem(days, 2) : nullptr;

            if (today) {
                snap.tempMax = static_cast<int>(std::lround(valueOf(child(today, "temperatureMax"))));
                snap.tempMin = static_cast<int>(std::lround(valueOf(child(today, "temperatureMin"))));
                cJSON *daytime = child(today, "daytime");
                cJSON *precip = child(daytime, "precipitation");
                cJSON *probability = child(precip, "probability");
                if (probability && cJSON_IsNumber(probability)) {
                    snap.rainChance = static_cast<int>(std::lround(probability->valuedouble * 100.0));
                }
            }
            char buf[16];
            if (tomorrow) {
                std::snprintf(buf, sizeof(buf), "%d°",
                              static_cast<int>(std::lround(valueOf(child(tomorrow, "temperatureMax")))));
                snap.tomorrow = buf;
            }
            if (after) {
                std::snprintf(buf, sizeof(buf), "%d°",
                              static_cast<int>(std::lround(valueOf(child(after, "temperatureMax")))));
                snap.dayAfter = buf;
            }
            cJSON_Delete(root);
        }
    }

    const std::time_t nowTime = std::time(nullptr);
    struct tm local{};
    localtime_r(&nowTime, &local);
    char clock[8];
    std::strftime(clock, sizeof(clock), "%H:%M", &local);
    snap.updated = clock;
    snap.city = cfg.city;
    return true;
}

} // namespace

WeatherClient &WeatherClient::instance() {
    static WeatherClient client;
    return client;
}

void WeatherClient::init() {
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_initialized) return;
    g_initialized = true;

    const std::string host = cleanHost(nvsRead(kNvsConfig, "host"));
    const std::string key = trimCopy(nvsRead(kNvsConfig, "key"));
    const std::string lat = trimCopy(nvsRead(kNvsConfig, "lat"));
    const std::string lon = trimCopy(nvsRead(kNvsConfig, "lon"));
    const std::string city = trimCopy(nvsRead(kNvsConfig, "city"));
    if (!host.empty()) g_cfg.host = host;
    if (!key.empty()) g_cfg.key = key;
    if (!lat.empty()) g_cfg.lat = lat;
    if (!lon.empty()) g_cfg.lon = lon;
    if (!city.empty()) g_cfg.city = city;

    g_snap = WeatherSnapshot{};
    g_snap.city = g_cfg.city;
    loadCache(g_snap);
    g_snap.configured = !g_cfg.key.empty() && !g_cfg.host.empty();
}

bool WeatherClient::configured() const {
    std::lock_guard<std::mutex> lock(g_mu);
    return !g_cfg.key.empty() && !g_cfg.host.empty();
}

std::string WeatherClient::host() const {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_cfg.host;
}

std::string WeatherClient::apiKey() const {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_cfg.key;
}

std::string WeatherClient::latitude() const {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_cfg.lat;
}

std::string WeatherClient::longitude() const {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_cfg.lon;
}

std::string WeatherClient::city() const {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_cfg.city;
}

void WeatherClient::setConfig(const std::string &host, const std::string &apiKey,
                              const std::string &latitude, const std::string &longitude,
                              const std::string &city) {
    const std::string cHost = cleanHost(host);
    const std::string cKey = trimCopy(apiKey);
    const std::string cLat = trimCopy(latitude);
    const std::string cLon = trimCopy(longitude);
    const std::string cCity = trimCopy(city);

    std::string hostOut, latOut, lonOut, cityOut;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_initialized = true;
        if (!cHost.empty()) g_cfg.host = cHost;
        g_cfg.key = cKey;
        if (!cLat.empty()) g_cfg.lat = cLat;
        if (!cLon.empty()) g_cfg.lon = cLon;
        if (!cCity.empty()) g_cfg.city = cCity;
        g_snap.configured = !g_cfg.key.empty() && !g_cfg.host.empty();
        g_snap.city = g_cfg.city;
        hostOut = g_cfg.host;
        latOut = g_cfg.lat;
        lonOut = g_cfg.lon;
        cityOut = g_cfg.city;
    }
    nvsWrite(kNvsConfig, "host", hostOut);
    nvsWrite(kNvsConfig, "key", cKey);
    nvsWrite(kNvsConfig, "lat", latOut);
    nvsWrite(kNvsConfig, "lon", lonOut);
    nvsWrite(kNvsConfig, "city", cityOut);
}

void WeatherClient::fetchAsync() {
    Config cfg;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (g_fetching || g_cfg.key.empty() || g_cfg.host.empty()) return;
        g_fetching = true;
        g_snap.fetching = true;
        cfg = g_cfg;
    }
    if (xTaskCreatePinnedToCore(worker, "weather_fetch", 16384, new Config(cfg), 3, nullptr, 0) != pdPASS) {
        std::lock_guard<std::mutex> lock(g_mu);
        g_fetching = false;
        g_snap.fetching = false;
        g_snap.error = "内存不足";
    }
}

bool WeatherClient::isFetching() const {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_fetching;
}

bool WeatherClient::stale(int seconds) const {
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_snap.valid || g_snap.fetchedAt == 0) return true;
    return (std::time(nullptr) - g_snap.fetchedAt) >= seconds;
}

WeatherSnapshot WeatherClient::snapshot() const {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_snap;
}

} // namespace core
