#include <string>
#include <string_view>
#include <filesystem>
#include <expected>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

class uri {
private:
    std::string scheme_;
    std::string userinfo_;
    std::string host_;
    std::string port_;
    std::filesystem::path path_;
    std::string query_;
    std::string fragment_;

    [[nodiscard]] static bool is_valid_scheme_char(char c) noexcept {
        return std::isalnum(c) || c == '+' || c == '-' || c == '.';
    }

    [[nodiscard]] static std::string percent_decode(std::string_view input) {
        std::string result;
        result.reserve(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            if (input[i] == '%' && i + 2 < input.size() &&
                std::isxdigit(input[i + 1]) && std::isxdigit(input[i + 2])) {
                unsigned char hex1 = input[i + 1];
                unsigned char hex2 = input[i + 2];
                auto val = [](unsigned char c) {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return 0;
                };
                result += static_cast<char>((val(hex1) << 4) | val(hex2));
                i += 2;
            } else {
                result += input[i];
            }
        }
        return result;
    }

    [[nodiscard]] static std::string percent_encode(std::string_view input, std::string_view safe_chars = "") {
        std::ostringstream oss;
        for (char c : input) {
            if (std::isalnum(c) || std::string_view("-_.~").find(c) != std::string_view::npos ||
                safe_chars.find(c) != std::string_view::npos) {
                oss << c;
            } else {
                oss << '%' << std::hex << std::uppercase << (static_cast<unsigned int>(static_cast<unsigned char>(c)) >> 4);
                oss << std::hex << std::uppercase << (static_cast<unsigned int>(static_cast<unsigned char>(c)) & 0xF);
            }
        }
        return oss.str();
    }

    void parse_from_string(std::string_view input) {
        scheme_.clear(); userinfo_.clear(); host_.clear(); port_.clear();
        path_.clear(); query_.clear(); fragment_.clear();
        if (input.empty()) return;

        std::string_view remaining = input;

        // 1. Extract scheme
        if (auto pos = remaining.find("://"); pos != std::string_view::npos) {
            std::string_view potential_scheme = remaining.substr(0, pos);
            if (!potential_scheme.empty() && std::isalpha(potential_scheme.front()) &&
                std::all_of(potential_scheme.begin(), potential_scheme.end(), &is_valid_scheme_char)) {
                scheme_ = potential_scheme;
                remaining = remaining.substr(pos + 3);
            }
        } else if (auto pos = remaining.find(':'); pos != std::string_view::npos) {
            // Handle Windows drive letters (e.g., C:\path) without misinterpreting as scheme
            if (!(pos == 1 && std::isalpha(remaining.front()) &&
                  (remaining.size() > 2 && (remaining[2] == '/' || remaining[2] == '\\')))) {
                // Fallback: treat as scheme-less URI with opaque component
            }
        }

        // 2. Extract authority
        if (remaining.starts_with("//")) {
            remaining = remaining.substr(2);
            std::string_view auth = remaining;
            auto boundary = remaining.find_first_of("/?#");
            if (boundary != std::string_view::npos) {
                auth = remaining.substr(0, boundary);
                remaining = remaining.substr(boundary);
            } else {
                remaining = "";
            }

            auto at_pos = auth.find('@');
            if (at_pos != std::string_view::npos) {
                userinfo_ = auth.substr(0, at_pos);
                auth = auth.substr(at_pos + 1);
            }

            // Handle IPv6 hosts [host]:port simplistically
            auto colon_pos = auth.find_last_of(':');
            bool has_ipv6 = auth.starts_with('[') && auth.find(']') != std::string_view::npos;
            if (!has_ipv6 && colon_pos != std::string_view::npos) {
                host_ = auth.substr(0, colon_pos);
                port_ = auth.substr(colon_pos + 1);
            } else {
                host_ = auth;
            }
        }

        // 3. Extract path
        if (!remaining.empty() && remaining[0] == '/') {
            auto end = remaining.find_first_of("?#");
            if (end != std::string_view::npos) {
                path_ = remaining.substr(0, end);
                remaining = remaining.substr(end);
            } else {
                path_ = remaining;
                remaining = "";
            }
        } else if (!remaining.empty() && remaining[0] != '?' && remaining[0] != '#') {
            auto end = remaining.find_first_of("?#");
            if (end != std::string_view::npos) {
                path_ = remaining.substr(0, end);
                remaining = remaining.substr(end);
            } else {
                path_ = remaining;
                remaining = "";
            }
        }

        // 4. Extract query
        if (!remaining.empty() && remaining[0] == '?') {
            remaining = remaining.substr(1);
            auto frag_pos = remaining.find('#');
            if (frag_pos != std::string_view::npos) {
                query_ = remaining.substr(0, frag_pos);
                remaining = remaining.substr(frag_pos);
            } else {
                query_ = remaining;
                remaining = "";
            }
        }

        // 5. Extract fragment
        if (!remaining.empty() && remaining[0] == '#') {
            fragment_ = remaining.substr(1);
        }

        // Normalize filesystem path component
        if (!path_.empty()) {
            path_ = std::filesystem::path(path_.string()).lexically_normal();
        }
    }

    [[nodiscard]] std::string build_string() const {
        std::string result;
        if (!scheme_.empty()) {
            result += scheme_;
            result += "://";
        }
        if (!host_.empty()) {
            if (!userinfo_.empty()) {
                result += userinfo_;
                result += '@';
            }
            result += host_;
            if (!port_.empty()) {
                result += ':';
                result += port_;
            }
        }
        if (!path_.empty()) {
            // Ensure leading slash for absolute paths when authority exists
            if (!host_.empty() && !path_.is_absolute()) {
                result += '/';
            }
            result += path_.string();
        }
        if (!query_.empty()) {
            result += '?';
            result += query_;
        }
        if (!fragment_.empty()) {
            result += '#';
            result += fragment_;
        }
        return result;
    }

public:
    // Constructors
    uri() = default;
    explicit uri(std::string_view input) { parse_from_string(input); }

    [[nodiscard]] static auto parse(std::string_view input) -> std::expected<uri, std::string> {
        uri result(input);
        // Basic validation: if a scheme is present, it must have a valid structure
        if (!result.scheme_.empty() && result.host_.empty() && result.path_.empty() &&
            result.query_.empty() && result.fragment_.empty()) {
            return std::unexpected{"invalid uri structure: scheme without hierarchical part"};
        }
        return result;
    }

    // Accessors
    [[nodiscard]] auto scheme() const noexcept -> std::string_view { return scheme_; }
    [[nodiscard]] auto userinfo() const noexcept -> std::string_view { return userinfo_; }
    [[nodiscard]] auto host() const noexcept -> std::string_view { return host_; }
    [[nodiscard]] auto port() const noexcept -> std::string_view { return port_; }
    [[nodiscard]] auto query() const noexcept -> std::string_view { return query_; }
    [[nodiscard]] auto fragment() const noexcept -> std::string_view { return fragment_; }
    [[nodiscard]] auto path() const noexcept -> std::filesystem::path const& { return path_; }

    [[nodiscard]] auto string() const -> std::string { return build_string(); }
    [[nodiscard]] auto native() const -> std::string {
        if (scheme_ == "file") return path_.native();
        return build_string();
    }

    // Mutators (return *this for chaining)
    auto set_scheme(std::string_view s) -> uri& {
        scheme_ = s;
        return *this;
    }
    auto set_userinfo(std::string_view u) -> uri& {
        userinfo_ = percent_encode(u, ":");
        return *this;
    }
    auto set_host(std::string_view h) -> uri& {
        host_ = h;
        return *this;
    }
    auto set_port(std::string_view p) -> uri& {
        port_ = p;
        return *this;
    }
    auto set_query(std::string_view q) -> uri& {
        query_ = q;
        return *this;
    }
    auto set_fragment(std::string_view f) -> uri& {
        fragment_ = f;
        return *this;
    }
    auto set_path(std::filesystem::path p) -> uri& {
        path_ = std::move(p).lexically_normal();
        return *this;
    }

    // Query parameter handling
    [[nodiscard]] auto query_params() const -> std::unordered_map<std::string, std::string> {
        std::unordered_map<std::string, std::string> params;
        if (query_.empty()) return params;

        std::string_view q = query_;
        while (!q.empty()) {
            auto amp = q.find('&');
            std::string_view pair = (amp == std::string_view::npos) ? q : q.substr(0, amp);
            q = (amp == std::string_view::npos) ? std::string_view{} : q.substr(amp + 1);

            auto eq = pair.find('=');
            std::string key = (eq == std::string_view::npos) ? std::string(pair) : std::string(pair.substr(0, eq));
            std::string val = (eq == std::string_view::npos) ? "" : std::string(pair.substr(eq + 1));

            params[percent_decode(key)] = percent_decode(val);
        }
        return params;
    }

    auto set_query_param(std::string_view key, std::string_view value) -> uri& {
        auto params = query_params();
        params[percent_decode(key)] = value;

        std::vector<std::string> parts;
        for (auto const& [k, v] : params) {
            parts.push_back(percent_encode(k) + '=' + percent_encode(v));
        }

        std::string new_query;
        for (size_t i = 0; i < parts.size(); ++i) {
            new_query += parts[i];
            if (i + 1 < parts.size()) new_query += '&';
        }
        query_ = new_query;
        return *this;
    }

    auto remove_query_param(std::string_view key) -> uri& {
        auto params = query_params();
        params.erase(percent_decode(key));
        // Rebuild
        query_.clear();
        bool first = true;
        for (auto const& [k, v] : params) {
            if (!first) query_ += '&';
            query_ += percent_encode(k);
            if (!v.empty()) query_ += '=' + percent_encode(v);
            first = false;
        }
        return *this;
    }

    // std::filesystem::path compatibility forwarding
    [[nodiscard]] auto filename() const -> std::filesystem::path { return path_.filename(); }
    [[nodiscard]] auto extension() const -> std::filesystem::path { return path_.extension(); }
    [[nodiscard]] auto stem() const -> std::filesystem::path { return path_.stem(); }
    [[nodiscard]] auto parent_path() const -> uri {
        uri result = *this;
        result.path_ = path_.parent_path();
        return result;
    }
    [[nodiscard]] auto has_filename() const -> bool { return path_.has_filename(); }
    [[nodiscard]] auto has_extension() const -> bool { return path_.has_extension(); }
    [[nodiscard]] auto has_root_path() const -> bool { return !scheme_.empty() || path_.has_root_path(); }
    [[nodiscard]] auto is_absolute() const -> bool { return !scheme_.empty() || path_.is_absolute(); }

    [[nodiscard]] auto lexically_normal() const -> uri {
        uri result = *this;
        result.path_ = path_.lexically_normal();
        return result;
    }

    auto append(std::filesystem::path const& p) -> uri& {
        path_ /= p;
        return *this;
    }
    auto concat(std::filesystem::path const& p) -> uri& {
        path_ += p;
        return *this;
    }

    [[nodiscard]] auto operator/(std::filesystem::path const& p) const -> uri {
        uri result = *this;
        result.path_ /= p;
        return result;
    }
    auto operator/=(std::filesystem::path const& p) -> uri& {
        path_ /= p;
        return *this;
    }

    // Comparisons
    [[nodiscard]] auto compare(uri const& other) const -> std::strong_ordering {
        if (auto res = scheme_.compare(other.scheme_); res != 0) return res <=> 0;
        if (auto res = host_.compare(other.host_); res != 0) return res <=> 0;
        if (auto res = port_.compare(other.port_); res != 0) return res <=> 0;
        if (auto res = userinfo_.compare(other.userinfo_); res != 0) return res <=> 0;
        if (auto res = path_.string().compare(other.path_.string()); res != 0) return res <=> 0;
        if (auto res = query_.compare(other.query_); res != 0) return res <=> 0;
        if (auto res = fragment_.compare(other.fragment_); res != 0) return res <=> 0;
        return std::strong_ordering::equivalent;
    }
    [[nodiscard]] auto operator==(uri const& other) const -> bool = default;
    [[nodiscard]] auto operator<=>(uri const& other) const -> std::strong_ordering {
        return compare(other);
    }
};

// Output stream operator
inline auto operator<<(std::ostream& os, uri const& u) -> std::ostream& {
    return os << u.string();
}
