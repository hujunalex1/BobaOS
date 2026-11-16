#include "config_server.hpp"

#include "connectivity.hpp"
#include "esp_http_server.h"
#include "esp_log.h"

#include <cstdio>
#include <cstring>
#include <mutex>

namespace core::config {
namespace {

constexpr const char *TAG = "ConfigSrv";

std::mutex g_mu;
std::vector<Provider> g_providers;
httpd_handle_t g_server = nullptr;
int g_port = 0;
int g_users = 0;

std::string urlDecode(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            unsigned int value = 0;
            if (std::sscanf(in.substr(i + 1, 2).c_str(), "%2x", &value) == 1) {
                out += static_cast<char>(value);
                i += 2;
                continue;
            }
        }
        out += (in[i] == '+') ? ' ' : in[i];
    }
    return out;
}

std::string escape(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::string formValue(const std::string &body, const std::string &key) {
    const std::string needle = key + "=";
    size_t pos = 0;
    while ((pos = body.find(needle, pos)) != std::string::npos) {
        const bool atFieldStart = pos == 0 || body[pos - 1] == '&';
        if (atFieldStart) {
            const size_t start = pos + needle.size();
            const size_t end = body.find('&', start);
            return urlDecode(body.substr(start, end == std::string::npos ? std::string::npos : end - start));
        }
        pos += needle.size();
    }
    return {};
}

const char *kBootstrap = R"HTML(<!DOCTYPE html><html lang="zh-CN"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>BobaOS 配置</title><style>
:root{color-scheme:light}
body{margin:0;padding:20px 16px 40px;background:#fff8f2;color:#382e25;
font-family:-apple-system,'PingFang SC',sans-serif;font-size:15px;line-height:1.5}
h1{font-size:19px;margin:0 0 4px}
.sub{color:#9c8e80;font-size:12px;margin:0 0 18px}
section{background:#fff;border:1px solid #eee4db;border-radius:14px;padding:16px;margin-bottom:14px}
h2{font-size:15px;margin:0 0 2px}
.note{color:#9c8e80;font-size:12px;margin:0 0 12px}
label{display:block;font-size:12px;color:#8b7a6a;margin:12px 0 4px}
input{width:100%;box-sizing:border-box;padding:11px 12px;border:1px solid #eee4db;
border-radius:10px;font-size:14px;color:#382e25;background:#fffaf6}
input:focus{outline:none;border-color:#ff6b8b}
button{width:100%;margin-top:14px;padding:13px;border:0;border-radius:10px;
background:#ff6b8b;color:#fff;font-size:15px;font-weight:600}
.ok{background:#e7f7ee;color:#237a4a;border-radius:10px;padding:10px 12px;
font-size:13px;margin-bottom:14px}
</style></head><body>
<h1>BobaOS 配置</h1>
<p class="sub">手表与手机在同一网络时可用，保存后立即生效</p>
)HTML";

const char *kTail = "</body></html>";

std::string buildPage(bool saved) {
    std::string html = kBootstrap;
    if (saved) html += "<div class='ok'>已保存</div>";

    std::vector<Provider> providers;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        providers = g_providers;
    }
    for (const auto &provider : providers) {
        html += "<section><h2>" + escape(provider.title) + "</h2>";
        if (!provider.note.empty()) html += "<p class='note'>" + escape(provider.note) + "</p>";
        html += "<form method='post' action='/save'>";
        html += "<input type='hidden' name='provider' value='" + escape(provider.id) + "'>";
        for (const auto &field : provider.fields()) {
            html += "<label>" + escape(field.label) + "</label>";
            const bool set = field.secret && !field.value.empty();
            const std::string shown = field.secret ? std::string() : escape(field.value);
            const std::string hint = set ? "已设置，留空则保持不变" : escape(field.placeholder);
            html += "<input name='" + escape(field.key) + "' value='" + shown + "' placeholder='" +
                    hint + "' maxlength='" + std::to_string(field.maxLength) + "'";
            if (set) html += " autocomplete='off'";
            html += ">";
        }
        html += "<button type='submit'>保存</button></form></section>";
    }
    html += kTail;
    return html;
}

esp_err_t rawHandler(httpd_req_t *req) {
    static const char *kRaw = "<!DOCTYPE html><meta charset='utf-8'><h1>raw ok</h1>";
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, kRaw, HTTPD_RESP_USE_STRLEN);
}

esp_err_t pageHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "page request");
    char query[48] = {0};
    bool saved = false;
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        saved = std::strstr(query, "saved=1") != nullptr;
    }
    const std::string html = buildPage(saved);
    ESP_LOGI(TAG, "page built: %u bytes", static_cast<unsigned>(html.size()));
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html.c_str(), html.size());
}

esp_err_t pingHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "ok", 2);
}

esp_err_t saveHandler(httpd_req_t *req) {
    const size_t total = req->content_len;
    if (total == 0 || total > 4096) return httpd_resp_send_500(req);

    std::string body(total, '\0');
    size_t received = 0;
    while (received < total) {
        const int got = httpd_req_recv(req, &body[received], total - received);
        if (got <= 0) break;
        received += got;
    }
    if (received != total) return httpd_resp_send_500(req);

    const std::string id = formValue(body, "provider");
    Provider provider;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        for (const auto &item : g_providers) {
            if (item.id == id) {
                provider = item;
                found = true;
                break;
            }
        }
    }
    if (!found) return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "unknown provider");

    std::vector<std::pair<std::string, std::string>> values;
    for (const auto &field : provider.fields()) {
        values.emplace_back(field.key, formValue(body, field.key));
    }
    const bool ok = provider.save(values);
    ESP_LOGI(TAG, "save provider=%s ok=%d", id.c_str(), ok ? 1 : 0);

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", ok ? "/?saved=1" : "/");
    return httpd_resp_send(req, "", 0);
}

bool listenOn(int port) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = port;
    cfg.ctrl_port = static_cast<uint16_t>(32768 + (port - 80));
    cfg.max_uri_handlers = 6;
    cfg.stack_size = 12288;
    const esp_err_t err = httpd_start(&g_server, &cfg);
    if (err != ESP_OK) {
        g_server = nullptr;
        ESP_LOGW(TAG, "port %d refused: %s", port, esp_err_to_name(err));
        return false;
    }

    httpd_uri_t root{};
    root.uri = "/";
    root.method = HTTP_GET;
    root.handler = pageHandler;
    httpd_register_uri_handler(g_server, &root);

    httpd_uri_t raw{};
    raw.uri = "/raw";
    raw.method = HTTP_GET;
    raw.handler = rawHandler;
    httpd_register_uri_handler(g_server, &raw);

    httpd_uri_t ping{};
    ping.uri = "/ping";
    ping.method = HTTP_GET;
    ping.handler = pingHandler;
    httpd_register_uri_handler(g_server, &ping);

    httpd_uri_t save{};
    save.uri = "/save";
    save.method = HTTP_POST;
    save.handler = saveHandler;
    httpd_register_uri_handler(g_server, &save);

    g_port = port;
    ESP_LOGI(TAG, "config server on port %d", port);
    return true;
}

} // namespace

void registerProvider(Provider provider) {
    std::lock_guard<std::mutex> lock(g_mu);
    for (auto &existing : g_providers) {
        if (existing.id == provider.id) {
            existing = std::move(provider);
            return;
        }
    }
    g_providers.push_back(std::move(provider));
}

void acquire() {
    {
        std::lock_guard<std::mutex> lock(g_mu);
        ++g_users;
        if (g_server) return;
    }
    // 80 first; the LAN web page also defaults to 80, so fall back rather
    // than silently losing the config page.
    for (int port : {80, 81, 82}) {
        if (listenOn(port)) return;
    }
    ESP_LOGE(TAG, "config server unavailable on ports 80/81/82");
    std::lock_guard<std::mutex> lock(g_mu);
    ESP_LOGW(TAG, "no free port for the config server");
}

void release() {
    // The config service is a boot service: it stays up so the page is
    // reachable without first opening an app on the watch.
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_users > 0) --g_users;
}

bool running() {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_server != nullptr;
}

int port() {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_port;
}

std::string url() {
    const int current = port();
    if (current == 0) return {};  // no listener: never hand out a dead address
    const auto snapshot = sys::Connectivity::snapshot();
    if (snapshot.ip.empty()) return {};
    std::string base = "http://" + snapshot.ip;
    if (current != 80) base += ":" + std::to_string(current);
    return base + "/";
}

} // namespace core::config
