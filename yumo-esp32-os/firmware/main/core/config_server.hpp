#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace core::config {

// One editable value on the phone-facing config page.
struct Field {
    std::string key;          // form field name
    std::string label;        // shown above the input
    std::string placeholder;  // hint when empty
    std::string value;        // current persisted value
    bool secret{false};       // never echoed back to the browser
    int maxLength{63};
};

// An app or subsystem that owns a group of fields. The server knows nothing
// about DeepSeek or weather; it only renders what providers declare.
struct Provider {
    std::string id;     // posted back as the provider key
    std::string title;  // section heading
    std::string note;   // one line of help under the heading
    std::function<std::vector<Field>()> fields;
    std::function<bool(const std::vector<std::pair<std::string, std::string>> &)> save;
};

using Values = std::vector<std::pair<std::string, std::string>>;

inline std::string valueOf(const Values &values, const std::string &key) {
    for (const auto &entry : values) {
        if (entry.first == key) return entry.second;
    }
    return {};
}

void registerProvider(Provider provider);

// One HTTP server for every provider, started when an app that owns
// credentials is on screen. Reference counted so two apps can share it.
void acquire();
void release();
bool running();
int port();
// "http://<device ip>[:port]/" — empty while the device has no address.
std::string url();

} // namespace core::config
