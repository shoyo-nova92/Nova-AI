#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <string>
#include "utf8_text.hpp"

namespace BrowserState {
using nlohmann::json;
inline json snapshot(const std::string& raw) {
    try {
        const std::string marker = "Browser DOM result: ";
        const auto start = raw.find(marker);
        auto parsed = json::parse(start == std::string::npos ? raw : raw.substr(start + marker.size()));
        if (parsed.contains("post_action_observation")) return parsed["post_action_observation"];
        return parsed;
    } catch (...) { return json::object(); }
}
inline json project(const json& source, bool withIds) {
    json out = json::object();
    for (const char* key : {"url", "title", "page_kind", "ready_state", "auth_required", "browser_ready"})
        if (source.contains(key)) {
            out[key] = source[key];
            if (out[key].is_string()) out[key] = Utf8::truncate(out[key].get<std::string>(), 2048, std::string("browser.")+key);
        }
    if (source.contains("visible_text") && source["visible_text"].is_string())
        out["visible_text"] = Utf8::truncate(source["visible_text"].get<std::string>(), 8000, "browser.visible_text");
    if (source.contains("readable_text") && source["readable_text"].is_string())
        out["readable_text"] = Utf8::truncate(source["readable_text"].get<std::string>(),8000,"browser.readable_text");
    for (const char* key : {"dialogs", "downloads", "tabs", "session_mode", "effects"})
        if (source.contains(key)) out[key] = source[key];
    if (!withIds && out.contains("downloads") && out["downloads"].is_array()) {
        json completed=json::array();
        for (const auto& item:out["downloads"]) if (item.value("status",std::string{})=="completed") {
            auto stable=item; stable.erase("id"); completed.push_back(std::move(stable));
        }
        out["downloads"]=std::move(completed);
    }
    auto element = [&](const json& item) {
        json e = json::object();
        for (const char* key : {"role", "accessible_name", "label", "text", "value", "editable", "enabled", "visible", "focused", "present", "href", "download", "blocked"})
            if (item.contains(key)) {
                e[key] = item[key];
                if (e[key].is_string()) e[key] = Utf8::truncate(e[key].get<std::string>(), 512, std::string("browser.element.")+key);
            }
        if (withIds && item.contains("element_id")) e["element_id"] = item["element_id"];
        if (withIds && item.contains("id")) e["id"] = item["id"];
        return e;
    };
    if (source.contains("focus") && source["focus"].is_object()) out["focus"] = element(source["focus"]);
    out["elements"] = json::array();
    if (source.contains("elements") && source["elements"].is_array()) {
        // Put focused and editable controls first; retain only actionable semantics.
        auto elements = source["elements"].get<std::vector<json>>();
        std::stable_sort(elements.begin(), elements.end(), [](const json& a, const json& b) {
            auto rank = [](const json& e) { return e.value("focused", false) ? 2 : e.value("editable", false) ? 1 : 0; };
            return rank(a) > rank(b);
        });
        for (const auto& item : elements) {
            auto e = element(item);
            if (!withIds) e.erase("focused");
            if (out["elements"].size() >= 48) break;
            if (Utf8::dump(out,"browser").size() + Utf8::dump(e,"browser.element").size() > 24000) break;
            out["elements"].push_back(std::move(e));
        }
    }
    return out;
}
inline std::string compact(const std::string& raw) {
    const auto parsed = snapshot(raw);
    return parsed.empty() ? Utf8::truncate(raw, 16000, "observation") : Utf8::dump(project(parsed, true), "browser.observation");
}
inline std::string fingerprint(const std::string& raw) {
    const auto parsed = snapshot(raw);
    return parsed.empty() ? std::string{} : Utf8::dump(project(parsed, false), "browser.fingerprint");
}
inline std::string summary(const std::string& raw) {
    const auto pos = raw.find("Browser DOM result:");
    return Utf8::truncate(raw, std::min(pos == std::string::npos ? raw.size() : pos, std::size_t(2048)), "history");
}
}
