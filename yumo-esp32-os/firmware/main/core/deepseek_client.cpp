#include "deepseek_client.hpp"

#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cmath>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>
#include <algorithm>
#include <sys/time.h>

namespace core {

static const char *TAG = "DeepSeekClient";
static const char *kNvsNamespace = "ds_monitor";
static httpd_handle_t s_dsServer = nullptr;

static std::string urlDecode(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            unsigned int val = 0;
            if (std::sscanf(in.substr(i + 1, 2).c_str(), "%x", &val) == 1) {
                out += static_cast<char>(val);
                i += 2;
                continue;
            }
        }
        if (in[i] == '+') {
            out += ' ';
        } else {
            out += in[i];
        }
    }
    return out;
}

static std::string findFormKey(const std::string &body, const std::string &key) {
    std::string needle = key + "=";
    size_t pos = body.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    size_t end = body.find('&', pos);
    std::string raw = (end == std::string::npos) ? body.substr(pos) : body.substr(pos, end - pos);
    return urlDecode(raw);
}

static void cleanTokenString(std::string &s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    while (!s.empty() && (s.front() == '\r' || s.front() == '\n' || s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    if (s.rfind("userToken:", 0) == 0 || s.rfind("userToken :", 0) == 0 || s.rfind("userToken=", 0) == 0) {
        size_t colon = s.find(':');
        if (colon == std::string::npos) colon = s.find('=');
        s = s.substr(colon + 1);
        while (!s.empty() && (s.front() == ' ' || s.front() == '"' || s.front() == '\'')) s.erase(s.begin());
        while (!s.empty() && (s.back() == ' ' || s.back() == '"' || s.back() == '\'')) s.pop_back();
    }
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        s = s.substr(1, s.size() - 2);
    }
    if (s.rfind("Bearer ", 0) == 0) {
        s = s.substr(7);
    }
    while (!s.empty() && (s.front() == ' ')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ')) s.pop_back();
}

static double getJsonNumber(cJSON *item, double defVal = 0.0) {
    if (!item) return defVal;
    if (item->valuestring && item->valuestring[0]) {
        return std::atof(item->valuestring);
    }
    if (cJSON_IsNumber(item)) {
        return item->valuedouble;
    }
    return defVal;
}

static int64_t getJsonInt64(cJSON *item, int64_t defVal = 0) {
    if (!item) return defVal;
    if (item->valuestring && item->valuestring[0]) {
        return std::atoll(item->valuestring);
    }
    if (cJSON_IsNumber(item)) {
        return static_cast<int64_t>(item->valuedouble);
    }
    return defVal;
}

static int64_t findJsonInt(cJSON *obj, const std::initializer_list<const char*> &keys) {
    if (!obj) return 0;
    for (const char *k : keys) {
        cJSON *item = cJSON_GetObjectItem(obj, k);
        if (item) return getJsonInt64(item);
    }
    return 0;
}

static void syncTimeFromHttpDate(const char *dateStr) {
    if (!dateStr || !*dateStr) return;
    const char *p = std::strchr(dateStr, ',');
    if (p) dateStr = p + 1;
    while (*dateStr == ' ') dateStr++;

    int day = 0, year = 0, hour = 0, min = 0, sec = 0;
    char monthStr[8] = {0};
    if (std::sscanf(dateStr, "%d %3s %d %d:%d:%d", &day, monthStr, &year, &hour, &min, &sec) == 6) {
        static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        int month = -1;
        for (int i = 0; i < 12; ++i) {
            if (std::strncmp(monthStr, months[i], 3) == 0) {
                month = i;
                break;
            }
        }
        if (month >= 0 && year >= 2024) {
            struct tm tm{};
            tm.tm_year = year - 1900;
            tm.tm_mon = month;
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = min;
            tm.tm_sec = sec;
            tm.tm_isdst = 0;
            time_t t = mktime(&tm);
            if (t > 1700000000) {
                struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
                settimeofday(&tv, nullptr);
                ESP_LOGI(TAG, "Synced system clock from HTTP Date: %04d-%02d-%02d %02d:%02d:%02d",
                         year, month + 1, day, hour, min, sec);
            }
        }
    }
}

static esp_err_t dsGetHandler(httpd_req_t *req) {
    auto &client = DeepSeekClient::instance();
    auto curData = client.getData();
    std::string curU = client.getUserToken();
    std::string curA = client.getApiKey();

    char statusInfo[512];
    if (curData.valid) {
        std::snprintf(statusInfo, sizeof(statusInfo),
            "<div style='background:#064e3b;color:#6ee7b7;padding:12px;border-radius:8px;margin-bottom:14px;font-size:13px;line-height:1.6'>"
            "<b>✓ 已同步成功</b><br>"
            "可用余额: ¥ %.2f (本金 ¥%.2f / 赠送 ¥%.2f)<br>"
            "今日用量: %lld Token | 费用: ¥ %.2f<br>"
            "<small style='color:#a7f3d0'>状态: 官方API(HTTP %d) / 平台(HTTP %d) %s</small>"
            "</div>",
            curData.totalBalance, curData.toppedUpBalance, curData.grantedBalance,
            (long long)curData.todayTokens, curData.todayCost,
            curData.lastApiStatus, curData.lastPlatformStatus,
            curData.hasTokenStats ? "[含用量数据]" : "[仅余额，无用量]");
    } else if (!curU.empty() || !curA.empty()) {
        std::snprintf(statusInfo, sizeof(statusInfo),
            "<div style='background:#7f1d1d;color:#fca5a5;padding:12px;border-radius:8px;margin-bottom:14px;font-size:13px;line-height:1.6'>"
            "<b>⚠ 同步中或遇到异常</b><br>API: HTTP %d | 平台: HTTP %d<br>%s</div>",
            curData.lastApiStatus, curData.lastPlatformStatus,
            curData.errorMsg.empty() ? "正在拉取中，请稍后刷新页面..." : curData.errorMsg.c_str());
    } else {
        std::snprintf(statusInfo, sizeof(statusInfo),
            "<div style='background:#1e293b;color:#94a3b8;padding:12px;border-radius:8px;margin-bottom:14px;font-size:13px'>"
            "尚未配置凭据，请在下方填入。</div>");
    }

    std::string html;
    html.reserve(3584);
    html += "<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1'>"
            "<title>DeepSeek 凭据配置 - CubePaw</title>"
            "<style>body{background:#0b0f19;color:#e2e8f0;font-family:-apple-system,BlinkMacSystemFont,sans-serif;padding:20px 14px;margin:0}"
            ".card{background:#131c2e;border:1px solid #1e293b;border-radius:16px;padding:20px;max-width:440px;margin:0 auto;box-shadow:0 10px 30px rgba(0,0,0,0.6)}"
            "h2{color:#38bdf8;margin:0 0 12px;font-size:20px;display:flex;align-items:center;gap:8px}"
            "label{font-size:13px;font-weight:600;color:#cbd5e1;display:block;margin-top:14px}"
            "input,textarea{width:100%;box-sizing:border-box;padding:12px;margin:6px 0 12px;background:#0f172a;border:1px solid #334155;border-radius:10px;color:#fff;font-size:13px;font-family:monospace}"
            "button{width:100%;padding:14px;background:#2563eb;color:#fff;border:none;border-radius:10px;font-weight:bold;font-size:15px;cursor:pointer;margin-top:12px}"
            "button:hover{background:#1d4ed8}"
            ".tip{background:#1a2336;border:1px solid #233554;border-radius:10px;padding:12px;font-size:12px;color:#94a3b8;margin-top:16px;line-height:1.6}"
            ".btn-sub{display:inline-block;padding:8px 14px;background:#1e293b;color:#38bdf8;border-radius:8px;text-decoration:none;font-size:12px;margin-top:8px}"
            "</style></head><body><div class='card'>"
            "<h2>🐳 DeepSeek 凭据配置</h2>";

    html += statusInfo;

    html += "<form method='POST' action='/ds'>"
            "<label>1. 平台 userToken (获取实时 Token 统计):</label>"
            "<textarea name='userToken' rows='4' placeholder='在 platform.deepseek.com 登录后按 F12 -> Local Storage 复制 userToken'>";
    html += curU;
    html += "</textarea>"
            "<label>2. API Key (获取官方余额，sk-...):</label>"
            "<input name='apiKey' placeholder='sk-...' value='";
    html += curA;
    html += "'>"
            "<button type='submit'>💾 保存并同步到设备</button>"
            "</form>"
            "<div style='text-align:center;margin-top:10px'>"
            "<a href='/ds' class='btn-sub'>🔄 刷新测试状态</a>"
            "</div>"
            "<div class='tip'><b>💡 获取指南：</b><br>"
            "• <b>官方余额</b>：在控制台创建 <code>sk-...</code> 即可立即显示真实金额。<br>"
            "• <b>Token 统计</b>：在电脑登录 <span style='color:#38bdf8'>platform.deepseek.com</span>，按 F12 打开开发者工具，在 Application (应用程序) -> Local Storage 中找到 <code>userToken</code> 完整值复制到上方。</div>"
            "</div></body></html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html.c_str(), html.size());
}

static esp_err_t dsPostHandler(httpd_req_t *req) {
    size_t totalLen = req->content_len;
    if (totalLen == 0 || totalLen > 8192) {
        return httpd_resp_send_500(req);
    }

    std::string postData;
    postData.resize(totalLen);
    size_t received = 0;
    while (received < totalLen) {
        int ret = httpd_req_recv(req, &postData[received], totalLen - received);
        if (ret <= 0) break;
        received += ret;
    }

    std::string userToken, apiKey;

    cJSON *root = cJSON_Parse(postData.c_str());
    if (root) {
        cJSON *u = cJSON_GetObjectItem(root, "userToken");
        cJSON *a = cJSON_GetObjectItem(root, "apiKey");
        if (u && u->valuestring) userToken = u->valuestring;
        if (a && a->valuestring) apiKey = a->valuestring;
        cJSON_Delete(root);
    } else {
        userToken = findFormKey(postData, "userToken");
        apiKey = findFormKey(postData, "apiKey");
    }

    cleanTokenString(userToken);
    cleanTokenString(apiKey);

    ESP_LOGI(TAG, "Received credentials: userToken len=%d, apiKey len=%d", (int)userToken.size(), (int)apiKey.size());

    if (!userToken.empty() || !apiKey.empty()) {
        DeepSeekClient::instance().setCredentials(userToken, apiKey);
        DeepSeekClient::instance().fetchAsync(nullptr);
    }

    const char *resp = "<!DOCTYPE html><html><body style='background:#0b0f19;color:#34d399;font-family:sans-serif;text-align:center;padding:40px'>"
                       "<h2>✅ 保存成功！</h2><p style='color:#cbd5e1'>凭据已更新并已保存至 NVS，后台已触发数据拉取，请查看 CubePaw 屏幕。</p>"
                       "<a href='/ds' style='display:inline-block;padding:10px 20px;background:#2563eb;color:#fff;border-radius:8px;text-decoration:none;margin-top:14px'>返回并查看状态</a></body></html>";
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static void startConfigServer() {
    if (s_dsServer) return;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;
    config.max_uri_handlers = 8;
    config.stack_size = 8192;

    if (httpd_start(&s_dsServer, &config) == ESP_OK) {
        httpd_uri_t getUri{};
        getUri.uri = "/";
        getUri.method = HTTP_GET;
        getUri.handler = dsGetHandler;
        httpd_register_uri_handler(s_dsServer, &getUri);

        httpd_uri_t getDs{};
        getDs.uri = "/ds";
        getDs.method = HTTP_GET;
        getDs.handler = dsGetHandler;
        httpd_register_uri_handler(s_dsServer, &getDs);

        httpd_uri_t postDs{};
        postDs.uri = "/ds";
        postDs.method = HTTP_POST;
        postDs.handler = dsPostHandler;
        httpd_register_uri_handler(s_dsServer, &postDs);

        httpd_uri_t apiDs{};
        apiDs.uri = "/api/deepseek";
        apiDs.method = HTTP_POST;
        apiDs.handler = dsPostHandler;
        httpd_register_uri_handler(s_dsServer, &apiDs);
    }
}

DeepSeekClient &DeepSeekClient::instance() {
    static DeepSeekClient inst;
    return inst;
}

DeepSeekClient::DeepSeekClient() {
    loadFromNvs();
}

void DeepSeekClient::init() {
    loadFromNvs();
    startConfigServer();
}

bool DeepSeekClient::hasCredentials() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_userToken.empty() || !m_apiKey.empty();
}

std::string DeepSeekClient::getUserToken() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_userToken;
}

std::string DeepSeekClient::getApiKey() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_apiKey;
}

void DeepSeekClient::setCredentials(const std::string &userToken, const std::string &apiKey) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_userToken = userToken;
        m_apiKey = apiKey;
    }
    saveToNvs();
}

void DeepSeekClient::loadFromNvs() {
    nvs_handle_t handle;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) == ESP_OK) {
        size_t len = 0;
        if (nvs_get_str(handle, "utoken", nullptr, &len) == ESP_OK && len > 0) {
            std::vector<char> buf(len);
            if (nvs_get_str(handle, "utoken", buf.data(), &len) == ESP_OK) {
                m_userToken = buf.data();
            }
        }
        len = 0;
        if (nvs_get_str(handle, "apikey", nullptr, &len) == ESP_OK && len > 0) {
            std::vector<char> buf(len);
            if (nvs_get_str(handle, "apikey", buf.data(), &len) == ESP_OK) {
                m_apiKey = buf.data();
            }
        }
        nvs_close(handle);
    }

    FILE *f = std::fopen("/sdcard/deepseek.txt", "r");
    if (!f) f = std::fopen("/sdcard/deepseek.json", "r");
    if (f) {
        char sdbuf[2048] = {0};
        size_t n = std::fread(sdbuf, 1, sizeof(sdbuf) - 1, f);
        std::fclose(f);
        if (n > 0) {
            cJSON *j = cJSON_Parse(sdbuf);
            if (j) {
                cJSON *u = cJSON_GetObjectItem(j, "userToken");
                cJSON *a = cJSON_GetObjectItem(j, "apiKey");
                if (u && u->valuestring) m_userToken = u->valuestring;
                if (a && a->valuestring) m_apiKey = a->valuestring;
                cJSON_Delete(j);
            } else {
                std::string raw(sdbuf);
                cleanTokenString(raw);
                if (raw.rfind("sk-", 0) == 0) m_apiKey = raw;
                else m_userToken = raw;
            }
            saveToNvs();
        }
    }
}

void DeepSeekClient::saveToNvs() {
    nvs_handle_t handle;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }

    nvs_set_str(handle, "utoken", m_userToken.c_str());
    nvs_set_str(handle, "apikey", m_apiKey.c_str());
    nvs_commit(handle);
    nvs_close(handle);
}

bool DeepSeekClient::isFetching() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fetching;
}

DeepSeekData DeepSeekClient::getData() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data;
}

struct HttpRespContext {
    std::string *body;
};

static esp_err_t httpEventHandler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_HEADER) {
        if (evt->header_key && strcasecmp(evt->header_key, "Date") == 0 && evt->header_value) {
            syncTimeFromHttpDate(evt->header_value);
        }
    } else if (evt->event_id == HTTP_EVENT_ON_DATA) {
        auto *ctx = static_cast<HttpRespContext *>(evt->user_data);
        if (ctx && ctx->body && evt->data && evt->data_len > 0) {
            ctx->body->append(static_cast<char *>(evt->data), evt->data_len);
        }
    }
    return ESP_OK;
}

static bool httpGet(const std::string &url, const std::string &authBearer, bool isPlatform, std::string &outBody, int &statusCode) {
    outBody.clear();
    statusCode = 0;

    HttpRespContext ctx{&outBody};
    esp_http_client_config_t config{};
    config.url = url.c_str();
    config.method = HTTP_METHOD_GET;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 15000;
    config.disable_auto_redirect = false;
    config.max_redirection_count = 3;
    config.buffer_size = 4096;
    config.buffer_size_tx = 4096;
    config.event_handler = httpEventHandler;
    config.user_data = &ctx;
    config.skip_cert_common_name_check = true;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return false;
    }

    std::string bearer = "Bearer " + authBearer;
    esp_http_client_set_header(client, "Authorization", bearer.c_str());
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    if (isPlatform) {
        esp_http_client_set_header(client, "x-client-platform", "web");
        esp_http_client_set_header(client, "Origin", "https://platform.deepseek.com");
        esp_http_client_set_header(client, "Referer", "https://platform.deepseek.com/");
    }

    esp_err_t err = esp_http_client_perform(client);
    statusCode = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "GET %s -> err=%d (%s), HTTP %d, bodyLen=%d",
             url.c_str(), err, esp_err_to_name(err), statusCode, (int)outBody.size());

    esp_http_client_cleanup(client);
    return (err == ESP_OK && statusCode >= 200 && statusCode < 300);
}

bool DeepSeekClient::performFetch() {
    std::string uToken, aKey;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        uToken = m_userToken;
        aKey = m_apiKey;
    }

    if (uToken.empty() && aKey.empty()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.valid = false;
        m_data.errorMsg = "未配置 Token 或 API Key";
        return false;
    }

    DeepSeekData result;
    result.lastUpdated = std::time(nullptr);
    bool anySuccess = false;

    // 1. If standard apiKey is provided, fetch official user balance
    if (!aKey.empty()) {
        std::string balanceResp;
        int status = 0;
        bool ok = httpGet("https://api.deepseek.com/user/balance", aKey, false, balanceResp, status);
        result.lastApiStatus = status;

        if (ok && !balanceResp.empty()) {
            cJSON *root = cJSON_Parse(balanceResp.c_str());
            if (root) {
                cJSON *isAvail = cJSON_GetObjectItem(root, "is_available");
                if (isAvail) result.isAvailable = cJSON_IsTrue(isAvail);

                cJSON *infos = cJSON_GetObjectItem(root, "balance_infos");
                if (infos && cJSON_IsArray(infos) && cJSON_GetArraySize(infos) > 0) {
                    cJSON *first = cJSON_GetArrayItem(infos, 0);
                    if (first) {
                        cJSON *tot = cJSON_GetObjectItem(first, "total_balance");
                        cJSON *grt = cJSON_GetObjectItem(first, "granted_balance");
                        cJSON *top = cJSON_GetObjectItem(first, "topped_up_balance");
                        cJSON *cur = cJSON_GetObjectItem(first, "currency");

                        result.totalBalance = getJsonNumber(tot, result.totalBalance);
                        result.grantedBalance = getJsonNumber(grt, result.grantedBalance);
                        result.toppedUpBalance = getJsonNumber(top, result.toppedUpBalance);
                        if (cur && cur->valuestring) result.currency = cur->valuestring;
                    }
                }
                cJSON_Delete(root);
                result.valid = true;
                anySuccess = true;
            }
        }
    }

    // 2. If platform userToken is provided, fetch platform user summary, amount & cost
    if (!uToken.empty()) {
        std::string summaryResp;
        int status = 0;
        bool ok = httpGet("https://platform.deepseek.com/api/v0/users/get_user_summary", uToken, true, summaryResp, status);
        result.lastPlatformStatus = status;

        if (ok && !summaryResp.empty()) {
            cJSON *root = cJSON_Parse(summaryResp.c_str());
            if (root) {
                cJSON *data = cJSON_GetObjectItem(root, "data");
                cJSON *wallet = data ? cJSON_GetObjectItem(data, "wallet") : cJSON_GetObjectItem(root, "wallet");
                if (wallet) {
                    cJSON *tot = cJSON_GetObjectItem(wallet, "total_balance");
                    cJSON *grt = cJSON_GetObjectItem(wallet, "granted_balance");
                    cJSON *top = cJSON_GetObjectItem(wallet, "topped_up_balance");
                    cJSON *cur = cJSON_GetObjectItem(wallet, "currency");

                    result.totalBalance = getJsonNumber(tot, result.totalBalance);
                    result.grantedBalance = getJsonNumber(grt, result.grantedBalance);
                    result.toppedUpBalance = getJsonNumber(top, result.toppedUpBalance);
                    if (cur && cur->valuestring) result.currency = cur->valuestring;
                }
                cJSON_Delete(root);
                result.valid = true;
                result.usingPlatformToken = true;
                result.isAvailable = (result.totalBalance > 0.0);
                anySuccess = true;
            }
        }

        // Calculate start and end timestamps in Beijing Time (GMT+8)
        time_t now = std::time(nullptr);
        if (now < 1700000000) {
            now = 1794391200; // Fallback to current period Nov 2026 if RTC uninitialized
        }

        const int64_t tzOffset = 28800; // GMT+8
        // Start of today in Beijing (in UTC timestamp)
        time_t todayStart = now - ((now + tzOffset) % 86400);
        time_t todayEnd = todayStart + 86400;

        // Start of month in Beijing (in UTC timestamp)
        time_t bjSeconds = now + tzOffset;
        struct tm tmBj{};
        gmtime_r(&bjSeconds, &tmBj);
        int daysSinceMonthStart = (tmBj.tm_mday > 0) ? (tmBj.tm_mday - 1) : 0;
        time_t monthStart = todayStart - ((int64_t)daysSinceMonthStart * 86400);
        time_t monthEnd = todayEnd;

        ESP_LOGI(TAG, "Time window GMT+8: todayStart=%lld, monthStart=%lld, now=%lld",
                 (long long)todayStart, (long long)monthStart, (long long)now);

        // Query by_api_key/amount with start, end, tz
        char amountUrl[160];
        std::snprintf(amountUrl, sizeof(amountUrl),
            "https://platform.deepseek.com/api/v0/usage/by_api_key/amount?start=%lld&end=%lld&tz=28800",
            (long long)monthStart, (long long)monthEnd);

        std::string amountResp;
        int aStatus = 0;
        bool aOk = httpGet(amountUrl, uToken, true, amountResp, aStatus);
        if (result.lastPlatformStatus == 0) result.lastPlatformStatus = aStatus;

        if (aOk && !amountResp.empty()) {
            cJSON *root = cJSON_Parse(amountResp.c_str());
            if (root) {
                cJSON *data = cJSON_GetObjectItem(root, "data");
                cJSON *series = data ? cJSON_GetObjectItem(data, "series") : cJSON_GetObjectItem(root, "series");
                if (series && cJSON_IsArray(series)) {
                    int sCount = cJSON_GetArraySize(series);
                    int64_t tToday = 0, pToday = 0, cToday = 0, hToday = 0;
                    int64_t tMonth = 0, pMonth = 0, cMonth = 0, hMonth = 0;

                    for (int s = 0; s < sCount; ++s) {
                        cJSON *sItem = cJSON_GetArrayItem(series, s);
                        if (!sItem) continue;
                        cJSON *buckets = cJSON_GetObjectItem(sItem, "buckets");
                        if (buckets && cJSON_IsArray(buckets)) {
                            int bCount = cJSON_GetArraySize(buckets);
                            for (int b = 0; b < bCount; ++b) {
                                cJSON *bItem = cJSON_GetArrayItem(buckets, b);
                                if (!bItem) continue;
                                int64_t bTime = findJsonInt(bItem, {"timestamp", "time", "date", "start_time", "begin_time"});
                                if (bTime > 10000000000LL) {
                                    bTime /= 1000; // Convert ms to seconds
                                }
                                int64_t tot = findJsonInt(bItem, {"total_tokens", "tokens", "total_token", "amount", "total"});
                                int64_t pmt = findJsonInt(bItem, {"prompt_tokens", "input_tokens", "prompt_token", "input"});
                                int64_t cpt = findJsonInt(bItem, {"completion_tokens", "output_tokens", "completion_token", "output"});
                                int64_t hit = findJsonInt(bItem, {"prompt_cache_hit_tokens", "cache_hit_tokens", "prompt_cache_hit", "cache_hit"});

                                tMonth += tot;
                                pMonth += pmt;
                                cMonth += cpt;
                                hMonth += hit;

                                if (bTime == 0 || bTime >= todayStart || bCount == 1) {
                                    tToday += tot;
                                    pToday += pmt;
                                    cToday += cpt;
                                    hToday += hit;
                                }
                            }
                        }
                    }

                    result.monthTokens = tMonth;
                    result.todayTokens = tToday;
                    result.promptTokens = pToday;
                    result.completionTokens = cToday;
                    result.cacheHitTokens = hToday;
                    if (pToday > 0) {
                        result.cacheHitRate = static_cast<double>(hToday) / static_cast<double>(pToday) * 100.0;
                    }
                    result.hasTokenStats = true;
                    result.valid = true;
                    anySuccess = true;
                    ESP_LOGI(TAG, "Parsed by_api_key/amount: todayTokens=%lld, monthTokens=%lld",
                             (long long)tToday, (long long)tMonth);
                }
                cJSON_Delete(root);
            }
        }

        // Query by_api_key/cost for exact real money spent today
        char costUrl[160];
        std::snprintf(costUrl, sizeof(costUrl),
            "https://platform.deepseek.com/api/v0/usage/by_api_key/cost?start=%lld&end=%lld&tz=28800",
            (long long)todayStart, (long long)todayEnd);

        std::string costResp;
        int cStatus = 0;
        bool cOk = httpGet(costUrl, uToken, true, costResp, cStatus);
        if (cOk && !costResp.empty()) {
            cJSON *root = cJSON_Parse(costResp.c_str());
            if (root) {
                cJSON *data = cJSON_GetObjectItem(root, "data");
                cJSON *series = data ? cJSON_GetObjectItem(data, "series") : cJSON_GetObjectItem(root, "series");
                if (series && cJSON_IsArray(series)) {
                    int sCount = cJSON_GetArraySize(series);
                    double costSum = 0.0;
                    for (int s = 0; s < sCount; ++s) {
                        cJSON *sItem = cJSON_GetArrayItem(series, s);
                        if (!sItem) continue;
                        cJSON *buckets = cJSON_GetObjectItem(sItem, "buckets");
                        if (buckets && cJSON_IsArray(buckets)) {
                            int bCount = cJSON_GetArraySize(buckets);
                            for (int b = 0; b < bCount; ++b) {
                                cJSON *bItem = cJSON_GetArrayItem(buckets, b);
                                if (!bItem) continue;
                                double cVal = getJsonNumber(cJSON_GetObjectItem(bItem, "cost"));
                                if (cVal == 0.0) cVal = getJsonNumber(cJSON_GetObjectItem(bItem, "amount"));
                                if (cVal == 0.0) cVal = getJsonNumber(cJSON_GetObjectItem(bItem, "total_cost"));
                                costSum += cVal;
                            }
                        }
                    }
                    if (costSum > 0.0) {
                        result.todayCost = costSum;
                        ESP_LOGI(TAG, "Parsed by_api_key/cost: todayCost=%.4f", costSum);
                    }
                }
                cJSON_Delete(root);
            }
        }

        // If todayCost was not returned by API but tokens exist, compute estimate
        if (result.todayCost == 0.0 && result.todayTokens > 0) {
            result.todayCost = (result.todayTokens / 1000000.0) * 1.5;
        }
    }

    if (!anySuccess) {
        result.errorMsg = "网络连接异常或 Token 无效";
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data = result;
    }
    return result.valid;
}

struct TaskParam {
    DeepSeekClient *self;
    std::function<void(bool success)> cb;
};

void DeepSeekClient::fetchAsync(std::function<void(bool success)> onComplete) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_fetching) return;
        m_fetching = true;
    }

    auto param = new TaskParam{this, onComplete};

    xTaskCreatePinnedToCore([](void *arg) {
        auto p = static_cast<TaskParam *>(arg);
        bool ok = p->self->performFetch();
        {
            std::lock_guard<std::mutex> lock(p->self->m_mutex);
            p->self->m_fetching = false;
        }
        if (p->cb) {
            p->cb(ok);
        }
        delete p;
        vTaskDelete(nullptr);
    }, "ds_fetch", 8192, param, 3, nullptr, 1);
}

} // namespace core
