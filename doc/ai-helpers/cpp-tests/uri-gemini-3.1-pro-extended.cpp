#include <string>
#include <string_view>
#include <filesystem>
#include <vector>
#include <utility>
#include <optional>
#include <ostream>
#include <concepts>
#include <algorithm>

class uri {
public:
    using query_list = std::vector<std::pair<std::string, std::string>>;

    // --- Constructors ---
    uri() = default;

    explicit uri(std::string_view uri_str) {
        parse(uri_str);
    }

    uri(const std::filesystem::path& local_path)
        : scheme_("file"), path_(local_path) {}

    // --- Modifiers ---
    void clear() noexcept {
        scheme_.clear();
        user_info_.clear();
        host_.clear();
        port_.reset();
        path_.clear();
        query_params_.clear();
        fragment_.clear();
    }

    uri& operator/=(const uri& other) {
        path_ /= other.path_;
        return *this;
    }

    uri& operator/=(const std::filesystem::path& sub_path) {
        path_ /= sub_path;
        return *this;
    }

    // --- Path-like Decomposition Observers ---
    [[nodiscard]] uri parent_path() const {
        uri result = *this;
        result.path_ = result.path_.parent_path();
        return result;
    }

    [[nodiscard]] std::filesystem::path filename() const {
        return path_.filename();
    }

    [[nodiscard]] std::filesystem::path extension() const {
        return path_.extension();
    }

    [[nodiscard]] std::filesystem::path stem() const {
        return path_.stem();
    }

    [[nodiscard]] bool is_absolute() const noexcept {
        // A URI is absolute if it has a non-empty scheme or if the underlying path is absolute
        return !scheme_.empty() || path_.is_absolute();
    }

    [[nodiscard]] bool is_relative() const noexcept {
        return !is_absolute();
    }

    // --- URL Component Getters & Setters ---
    [[nodiscard]] std::string_view scheme() const noexcept { return scheme_; }
    void set_scheme(std::string_view scheme) { scheme_ = scheme; }

    [[nodiscard]] std::string_view user_info() const noexcept { return user_info_; }
    void set_user_info(std::string_view user_info) { user_info_ = user_info; }

    [[nodiscard]] std::string_view host() const noexcept { return host_; }
    void set_host(std::string_view host) { host_ = host; }

    [[nodiscard]] std::optional<uint16_t> port() const noexcept { return port_; }
    void set_port(std::optional<uint16_t> port) noexcept { port_ = port; }

    [[nodiscard]] const std::filesystem::path& path() const topic noexcept { return path_; }
    void set_path(const std::filesystem::path& path) { path_ = path; }

    [[nodiscard]] const query_list& query_parameters() const noexcept { return query_params_; }

    void set_query_parameters(query_list params) {
        query_params_ = std::move(params);
    }

    void append_query_parameter(std::string key, std::string value) {
        query_params_.emplace_back(std::move(key), std::move(value));
    }

    [[nodiscard]] std::string_view fragment() const noexcept { return fragment_; }
    void set_fragment(std::string_view fragment) { fragment_ = fragment; }

    // --- Conversions & String Representations ---
    [[nodiscard]] std::string string() const {
        std::string result;

        if (!scheme_.empty()) {
            result += scheme_ + "://";
        }

        if (!user_info_.empty()) {
            result += user_info_ + "@";
        }

        if (!host_.empty()) {
            result += host_;
            if (port_) {
                result += ":" + std::to_string(*port_);
            }
        }

        std::string path_str = path_.generic_string();
        // Ensure accurate separator if authority exists and path is relative
        if (!host_.empty() && !path_str.empty() && !path_str.starts_with('/')) {
            result += "/";
        }
        result += path_str;

        if (!query_params_.empty()) {
            result += "?";
            for (size_t i = 0; i < query_params_.size(); ++i) {
                result += query_params_[i].first + "=" + query_params_[i].second;
                if (i + 1 < query_params_.size()) {
                    result += "&";
                }
            }
        }

        if (!fragment_.empty()) {
            result += "#" + fragment_;
        }

        return result;
    }

    explicit operator std::string() const { return string(); }

    // --- Path Iterator Support ---
    auto begin() const { return path_.begin(); }
    auto end() const { return path_.end(); }

    // --- Non-member Operators ---
    friend uri operator/(uri lhs, const std::filesystem::path& rhs) {
        lhs /= rhs;
        return lhs;
    }

    friend uri operator/(uri lhs, const uri& rhs) {
        lhs /= rhs;
        return lhs;
    }

    friend std::ostream& operator<<(std::ostream& os, const uri& u) {
        os << u.string();
        return os;
    }

private:
    std::string scheme_;
    std::string user_info_;
    std::string host_;
    std::optional<uint16_t> port_;
    std::filesystem::path path_; // Embedded std::filesystem::path instance
    query_list query_params_;
    std::string fragment_;

    // Simple parsing logic utilizing C++23 string_view additions (like .contains)
    void parse(std::string_view uri_str) {
        clear();
        std::string_view remaining = uri_str;

        // 1. Extract Scheme
        if (auto scheme_end = remaining.find("://"); scheme_end != std::string_view::npos) {
            scheme_ = std::string(remaining.substr(0, scheme_end));
            remaining.remove_prefix(scheme_end + 3);

            // 2. Extract Authority (User Info, Host, Port)
            auto auth_end = remaining.find_first_of("/?#");
            std::string_view authority = (auth_end == std::string_view::npos)
                ? remaining
                : remaining.substr(0, auth_end);

            if (auth_end != std::string_view::npos) {
                remaining.remove_prefix(auth_end);
            } else {
                remaining = "";
            }

            if (!authority.empty()) {
                if (auto user_end = authority.find('@'); user_end != std::string_view::npos) {
                    user_info_ = std::string(authority.substr(0, user_end));
                    authority.remove_prefix(user_end + 1);
                }

                if (auto port_start = authority.find(':'); port_start != std::string_view::npos) {
                    host_ = std::string(authority.substr(0, port_start));
                    std::string port_str(authority.substr(port_start + 1));
                    if (!port_str.empty()) {
                        port_ = static_cast<uint16_t>(std::stoi(port_str));
                    }
                } else {
                    host_ = std::string(authority);
                }
            }
        } else {
            // No schema delimiter found, parse entirely as a local path fallback
            scheme_ = "file";
        }

        // 3. Extract Fragment
        if (auto frag_start = remaining.find('#'); frag_start != std::string_view::npos) {
            fragment_ = std::string(remaining.substr(frag_start + 1));
            remaining = remaining.substr(0, frag_start);
        }

        // 4. Extract Query Parameters
        if (auto query_start = remaining.find('?'); query_start != std::string_view::npos) {
            std::string_view query_str = remaining.substr(query_start + 1);
            remaining = remaining.substr(0, query_start);

            size_t pos = 0;
            while (pos < query_str.size()) {
                auto ampersand = query_str.find('&', pos);
                std::string_view pair = (ampersand == std::string_view::npos)
                    ? query_str.substr(pos)
                    : query_str.substr(pos, ampersand - pos);

                if (auto equal = pair.find('='); equal != std::string_view::npos) {
                    query_params_.emplace_back(std::string(pair.substr(0, equal)), std::string(pair.substr(equal + 1)));
                } else if (!pair.empty()) {
                    query_params_.emplace_back(std::string(pair), "");
                }

                if (ampersand == std::string_view::npos) break;
                pos = ampersand + 1;
            }
        }

        // 5. Assign remaining tokens to the local filesystem path component
        path_ = std::filesystem::path(remaining);
    }
};


#include <iostream>

int main() {
    // Scenario A: Manipulating a web URL
    uri endpoint("https://user:pass@api.example.com:8080/v1/users");
    endpoint.append_query_parameter("limit", "10");
    endpoint.append_query_parameter("sort", "desc");
    endpoint.set_fragment("profile");

    // Seamless path appending on a URL path segment!
    endpoint /= "settings";

    std::cout << "--- URL Processing ---\n";
    std::cout << "Full URI:  " << endpoint << "\n";
    std::cout << "Scheme:    " << endpoint.scheme() << "\n";
    std::cout << "Host:      " << endpoint.host() << "\n";
    std::cout << "Port:      " << endpoint.port().value_or(0) << "\n";
    std::cout << "Filename:  " << endpoint.filename() << "\n\n";

    // Scenario B: Manipulating local file systems natively
    std::filesystem::path local_base = "/var/log";
    uri local_uri(local_base);

    local_uri /= "nginx" / std::filesystem::path("access.log");

    std::cout << "--- Local Path Compatibility ---\n";
    std::cout << "Full URI:  " << local_uri << "\n";
    std::cout << "Extension: " << local_uri.extension() << "\n";
    std::cout << "Stem:      " << local_uri.stem() << "\n";
    std::cout << "Parent:    " << local_uri.parent_path() << "\n";
}


