#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <stdexcept>
#include <regex>

namespace fs = std::filesystem;

class uri {
public:
    // Constructors
    uri() noexcept;
    explicit uri(const std::string& uri_str);
    uri(const fs::path& path);
    uri(const uri& other);
    uri(uri&& other) noexcept;
    ~uri() noexcept;

    // Assignment operators
    uri& operator=(const uri& other);
    uri& operator=(uri&& other) noexcept;

    // Path operations (compatible with std::filesystem::path)
    uri& operator/=(const fs::path& rhs);
    uri operator/(const fs::path& rhs) const;
    void append(const fs::path& rhs);
    void normalize();
    fs::path::iterator begin() const;
    fs::path::iterator end() const;

    // URI scheme handling
    std::string scheme() const;
    void set_scheme(const std::string& scheme);
    bool has_scheme() const noexcept;

    // Authority components (user\:pass@host\:port)
    std::optional<std::string> user_info() const;
    void set_user_info(const std::string& user_info);
    std::string host() const;
    void set_host(const std::string& host);
    std::optional<uint16_t> port() const;
    void set_port(uint16_t port);

    // Query parameters (key-value pairs)
    std::map<std::string, std::string> query_parameters() const;
    void set_query_parameter(const std::string& key, const std::string& value);
    void remove_query_parameter(const std::string& key);
    void clear_query_parameters();

    // Fragment identifier
    std::optional<std::string> fragment() const;
    void set_fragment(const std::string& fragment);

    // Conversion to/from strings
    std::string to_string() const;
    fs::path to_path() const;
    static uri from_string(const std::string& uri_str);

    // Comparison operators
    bool operator==(const uri& other) const noexcept;
    bool operator!=(const uri& other) const noexcept;
    bool operator<(const uri& other) const noexcept;

private:
    fs::path m_path; // Internal path representation
    std::string m_scheme;
    std::optional<std::string> m_user_info;
    std::string m_host;
    std::optional<uint16_t> m_port;
    std::map<std::string, std::string> m_query_parameters;
    std::optional<std::string> m_fragment;

    // Helper functions for parsing and validation
    void parse_uri(const std::string& uri_str);
    void validate_scheme(const std::string& scheme) const;
    void validate_port(uint16_t port) const;
    std::string percent_encode(const std::string& str) const;
    std::string percent_decode(const std::string& str) const;
};


#include "uri.h"
#include <stdexcept>
#include <regex>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

// Helper function to percent-encode a string
std::string uri::percent_encode(const std::string& str) const {
    std::string encoded;
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else {
            encoded += '%';
            encoded += "0123456789ABCDEF"[(c >> 4) & 0xF];
            encoded += "0123456789ABCDEF"[c & 0xF];
        }
    }
    return encoded;
}

// Helper function to percent-decode a string
std::string uri::percent_decode(const std::string& str) const {
    std::string decoded;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int value;
            std::istringstream is(str.substr(i + 1, 2));
            if (is >> std::hex >> value) {
                decoded += static_cast<char>(value);
                i += 2;
            } else {
                throw std::invalid_argument("Invalid percent-encoded string");
            }
        } else {
            decoded += str[i];
        }
    }
    return decoded;
}

// Constructor from string
uri::uri(const std::string& uri_str) {
    parse_uri(uri_str);
}

// Constructor from fs::path
uri::uri(const fs::path& path) : m_path(path) {}

// Copy constructor
uri::uri(const uri& other)
    : m_path(other.m_path),
      m_scheme(other.m_scheme),
      m_user_info(other.m_user_info),
      m_host(other.m_host),
      m_port(other.m_port),
      m_query_parameters(other.m_query_parameters),
      m_fragment(other.m_fragment) {}

// Move constructor
uri::uri(uri&& other) noexcept
    : m_path(std::move(other.m_path)),
      m_scheme(std::move(other.m_scheme)),
      m_user_info(std::move(other.m_user_info)),
      m_host(std::move(other.m_host)),
      m_port(std::move(other.m_port)),
      m_query_parameters(std::move(other.m_query_parameters)),
      m_fragment(std::move(other.m_fragment)) {}

// Assignment operators
uri& uri::operator=(const uri& other) {
    if (this != &other) {
        m_path = other.m_path;
        m_scheme = other.m_scheme;
        m_user_info = other.m_user_info;
        m_host = other.m_host;
        m_port = other.m_port;
        m_query_parameters = other.m_query_parameters;
        m_fragment = other.m_fragment;
    }
    return *this;
}

uri& uri::operator=(uri&& other) noexcept {
    if (this != &other) {
        m_path = std::move(other.m_path);
        m_scheme = std::move(other.m_scheme);
        m_user_info = std::move(other.m_user_info);
        m_host = std::move(other.m_host);
        m_port = std::move(other.m_port);
        m_query_parameters = std::move(other.m_query_parameters);
        m_fragment = std::move(other.m_fragment);
    }
    return *this;
}

// Path operations
uri& uri::operator/=(const fs::path& rhs) {
    m_path /= rhs;
    return *this;
}

uri uri::operator/(const fs::path& rhs) const {
    uri result(*this);
    result /= rhs;
    return result;
}

void uri::append(const fs::path& rhs) {
    m_path /= rhs;
}

void uri::normalize() {
    m_path = m_path.lexically_normal();
}

// URI scheme handling
std::string uri::scheme() const {
    return m_scheme;
}

void uri::set_scheme(const std::string& scheme) {
    validate_scheme(scheme);
    m_scheme = scheme;
}

bool uri::has_scheme() const noexcept {
    return !m_scheme.empty();
}

// Authority components
std::optional<std::string> uri::user_info() const {
    return m_user_info;
}

void uri::set_user_info(const std::string& user_info) {
    m_user_info = user_info;
}

std::string uri::host() const {
    return m_host;
}

void uri::set_host(const std::string& host) {
    m_host = host;
}

std::optional<uint16_t> uri::port() const {
    return m_port;
}

void uri::set_port(uint16_t port) {
    validate_port(port);
    m_port = port;
}

// Query parameters
std::map<std::string, std::string> uri::query_parameters() const {
    return m_query_parameters;
}

void uri::set_query_parameter(const std::string& key, const std::string& value) {
    m_query_parameters[key] = value;
}

void uri::remove_query_parameter(const std::string& key) {
    m_query_parameters.erase(key);
}

void uri::clear_query_parameters() {
    m_query_parameters.clear();
}

// Fragment identifier
std::optional<std::string> uri::fragment() const {
    return m_fragment;
}

void uri::set_fragment(const std::string& fragment) {
    m_fragment = fragment;
}

// Conversion to/from strings
std::string uri::to_string() const {
    std::string result;
    if (!m_scheme.empty()) {
        result += m_scheme + "://";
    }
    if (m_user_info) {
        result += *m_user_info + "@";
    }
    result += m_host;
    if (m_port) {
        result += ":" + std::to_string(*m_port);
    }
    result += m_path.string();
    if (!m_query_parameters.empty()) {
        result += "?";
        for (const auto& [key, value] : m_query_parameters) {
            result += percent_encode(key) + "=" + percent_encode(value) + "&";
        }
        result.pop_back(); // Remove trailing '&'
    }
    if (m_fragment) {
        result += "#" + *m_fragment;
    }
    return result;
}

fs::path uri::to_path() const {
    return m_path;
}

uri uri::from_string(const std::string& uri_str) {
    return uri(uri_str);
}

// Comparison operators
bool uri::operator==(const uri& other) const noexcept {
    return m_path == other.m_path &&
           m_scheme == other.m_scheme &&
           m_user_info == other.m_user_info &&
           m_host == other.m_host &&
           m_port == other.m_port &&
           m_query_parameters == other.m_query_parameters &&
           m_fragment == other.m_fragment;
}

bool uri::operator!=(const uri& other) const noexcept {
    return !(*this == other);
}

bool uri::operator<(const uri& other) const noexcept {
    return m_path < other.m_path;
}

// Helper function to parse URI string
void uri::parse_uri(const std::string& uri_str) {
    std::regex uri_regex(
        R"(([a-zA-Z][a-zA-Z0-9+.-]*):\/\/)?([^@\/?#]*)@?([^:\/?#]*)(:([0-9]+))?([^\?#]*)(\?([^#]*))?(#(.*))?)"
    );
    std::smatch match;
    if (!std::regex_match(uri_str, match, uri_regex)) {
        throw std::invalid_argument("Invalid URI format");
    }

    if (match[1].matched) {
        m_scheme = match[1].str();
    }
    if (match[2].matched) {
        m_user_info = match[2].str();
    }
    m_host = match[3].str();
    if (match[5].matched) {
        m_port = std::stoi(match[5].str());
    }
    m_path = fs::path(match[6].str());
    if (match[8].matched) {
        std::string query = match[8].str();
        std::regex query_regex(R"(([^=&]+)=([^&]+))");
        auto query_begin = std::sregex_iterator(query.begin(), query.end(), query_regex);
        auto query_end = std::sregex_iterator();
        for (std::sregex_iterator i = query_begin; i != query_end; ++i) {
            std::smatch query_match = *i;
            m_query_parameters[query_match[1].str()] = query_match[2].str();
        }
    }
    if (match[10].matched) {
        m_fragment = match[10].str();
    }
}

// Helper function to validate scheme
void uri::validate_scheme(const std::string& scheme) const {
    if (scheme.empty()) {
        return;
    }
    if (!std::all_of(scheme.begin(), scheme.end(), [](char c) {
        return isalpha(c) || c == '+' || c == '-' || c == '.';
    })) {
        throw std::invalid_argument("Invalid URI scheme");
    }
}

// Helper function to validate port
void uri::validate_port(uint16_t port) const {
    if (port > 65535) {
        throw std::invalid_argument("Invalid port number");
    }
}

#include <iostream>

int main() {
    // Construct from a URI string
    uri u("http://user\:pass@example.com:8080/path/to/resource?query=value&param=123#fragment");

    // Modify components
    u.set_scheme("https");
    u.set_host("api.example.com");
    u.set_port(443);
    u.set_query_parameter("query", "new_value");
    u.set_fragment("section1");

    // Print the modified URI
    std::cout << u.to_string() << std::endl;

    // Construct from a filesystem path
    uri file_uri(fs::path("/home/user/file.txt"));

    // Append path components
    file_uri /= "subdir";
    file_uri /= "data.csv";

    // Print the path
    std::cout << file_uri.to_path().string() << std::endl;

    return 0;
}
