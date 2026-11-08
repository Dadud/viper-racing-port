#include "content_manifest.h"

#include <cstdint>
#include <fstream>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>

namespace content {
namespace {
constexpr uint64_t kMaxManifestSize = 64 * 1024;

void append_utf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7f) out.push_back(static_cast<char>(cp));
    else if (cp <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else if (cp <= 0xffff) {
        out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    }
}

bool valid_utf8(std::string_view s) {
    for (size_t i = 0; i < s.size();) {
        const auto c = static_cast<uint8_t>(s[i]);
        if (c <= 0x7f) { ++i; continue; }
        size_t n = 0;
        uint32_t cp = 0;
        if (c >= 0xc2 && c <= 0xdf) { n = 2; cp = c & 0x1f; }
        else if (c >= 0xe0 && c <= 0xef) { n = 3; cp = c & 0x0f; }
        else if (c >= 0xf0 && c <= 0xf4) { n = 4; cp = c & 0x07; }
        else return false;
        if (i + n > s.size()) return false;
        for (size_t j = 1; j < n; ++j) {
            const auto trail = static_cast<uint8_t>(s[i + j]);
            if ((trail & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (trail & 0x3f);
        }
        if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) ||
            cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
        i += n;
    }
    return true;
}

class Parser {
public:
    explicit Parser(std::string_view input) : input_(input) {}

    bool parse(Metadata& output, std::string& error) {
        Metadata parsed;
        bool have_schema = false;
        skip_space();
        if (!take('{')) return fail(error, "expected object");
        skip_space();
        if (!take('}')) {
            for (;;) {
                std::string key;
                if (!parse_string(key)) return fail(error, reason_);
                skip_space();
                if (!take(':')) return fail(error, "expected ':'");
                skip_space();
                if (key == "schema_version") {
                    if (have_schema) return fail(error, "duplicate schema_version");
                    have_schema = true;
                    if (!take('1')) return fail(error, "schema_version must be 1");
                    if (position_ < input_.size() && is_number_char(input_[position_]))
                        return fail(error, "schema_version must be integer 1");
                } else if (key == "author" || key == "version" || key == "description") {
                    std::string value;
                    if (!parse_string(value)) return fail(error, reason_);
                    const size_t max = key == "author" ? 256 : key == "version" ? 64 : 4096;
                    if (value.size() > max) return fail(error, "metadata field too long: " + key);
                    if (key == "author") {
                        if (have_author_) return fail(error, "duplicate author");
                        have_author_ = true; parsed.author = std::move(value);
                    } else if (key == "version") {
                        if (have_version_) return fail(error, "duplicate version");
                        have_version_ = true; parsed.version = std::move(value);
                    } else {
                        if (have_description_) return fail(error, "duplicate description");
                        have_description_ = true; parsed.description = std::move(value);
                    }
                } else if (!skip_value(0)) return fail(error, reason_);
                skip_space();
                if (take('}')) break;
                if (!take(',')) return fail(error, "expected ',' or '}'");
                skip_space();
            }
        }
        skip_space();
        if (position_ != input_.size()) return fail(error, "trailing data");
        if (!have_schema) return fail(error, "missing schema_version");
        output = std::move(parsed);
        error.clear();
        return true;
    }

private:
    static bool is_number_char(char c) { return (c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-'; }
    void skip_space() {
        while (position_ < input_.size() && (input_[position_] == ' ' || input_[position_] == '\t' ||
               input_[position_] == '\r' || input_[position_] == '\n')) ++position_;
    }
    bool take(char c) {
        if (position_ >= input_.size() || input_[position_] != c) return false;
        ++position_; return true;
    }
    bool fail(std::string& error, std::string why) { error = std::move(why); return false; }
    bool hex4(uint32_t& value) {
        value = 0;
        for (unsigned i = 0; i < 4; ++i) {
            if (position_ >= input_.size()) return false;
            const char c = input_[position_++];
            uint32_t digit;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else return false;
            value = (value << 4) | digit;
        }
        return true;
    }
    bool parse_string(std::string& value) {
        if (!take('"')) { reason_ = "expected string"; return false; }
        value.clear();
        while (position_ < input_.size()) {
            const unsigned char c = static_cast<unsigned char>(input_[position_++]);
            if (c == '"') {
                if (!valid_utf8(value)) { reason_ = "invalid UTF-8 string"; return false; }
                return true;
            }
            if (c < 0x20) { reason_ = "unescaped control byte"; return false; }
            if (c != '\\') { value.push_back(static_cast<char>(c)); continue; }
            if (position_ >= input_.size()) { reason_ = "truncated escape"; return false; }
            const char escaped = input_[position_++];
            switch (escaped) {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case '/': value.push_back('/'); break;
            case 'b': value.push_back('\b'); break;
            case 'f': value.push_back('\f'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case 'u': {
                uint32_t cp;
                if (!hex4(cp)) { reason_ = "invalid Unicode escape"; return false; }
                if (cp >= 0xd800 && cp <= 0xdbff) {
                    if (position_ + 2 > input_.size() || input_[position_] != '\\' || input_[position_ + 1] != 'u') {
                        reason_ = "unpaired high surrogate"; return false;
                    }
                    position_ += 2;
                    uint32_t low;
                    if (!hex4(low) || low < 0xdc00 || low > 0xdfff) { reason_ = "invalid surrogate pair"; return false; }
                    cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
                } else if (cp >= 0xdc00 && cp <= 0xdfff) { reason_ = "unpaired low surrogate"; return false; }
                append_utf8(value, cp);
                break;
            }
            default: reason_ = "invalid escape"; return false;
            }
        }
        reason_ = "unterminated string";
        return false;
    }
    bool skip_number() {
        const size_t start = position_;
        if (take('-')) {}
        if (take('0')) {
            if (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') return false;
        } else {
            if (position_ >= input_.size() || input_[position_] < '1' || input_[position_] > '9') return false;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
        }
        if (take('.')) {
            const size_t digits = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
            if (digits == position_) return false;
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) ++position_;
            const size_t digits = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
            if (digits == position_) return false;
        }
        return position_ > start;
    }
    bool skip_value(unsigned depth) {
        if (depth > 32) { reason_ = "nesting too deep"; return false; }
        if (position_ >= input_.size()) { reason_ = "missing value"; return false; }
        if (input_[position_] == '"') { std::string ignored; return parse_string(ignored); }
        if (input_[position_] == '{') {
            ++position_; skip_space();
            if (take('}')) return true;
            for (;;) {
                std::string key;
                if (!parse_string(key)) return false;
                skip_space();
                if (!take(':')) { reason_ = "expected ':'"; return false; }
                skip_space();
                if (!skip_value(depth + 1)) return false;
                skip_space();
                if (take('}')) return true;
                if (!take(',')) { reason_ = "expected ',' or '}'"; return false; }
                skip_space();
            }
        }
        if (input_[position_] == '[') {
            ++position_; skip_space();
            if (take(']')) return true;
            for (;;) {
                if (!skip_value(depth + 1)) return false;
                skip_space();
                if (take(']')) return true;
                if (!take(',')) { reason_ = "expected ',' or ']'"; return false; }
                skip_space();
            }
        }
        if (input_.substr(position_, 4) == "true") { position_ += 4; return true; }
        if (input_.substr(position_, 5) == "false") { position_ += 5; return true; }
        if (input_.substr(position_, 4) == "null") { position_ += 4; return true; }
        if (input_[position_] == '-' || (input_[position_] >= '0' && input_[position_] <= '9')) {
            if (skip_number()) return true;
            reason_ = "invalid number"; return false;
        }
        reason_ = "invalid value";
        return false;
    }

    std::string_view input_;
    size_t position_ = 0;
    std::string reason_;
    bool have_author_ = false, have_version_ = false, have_description_ = false;
};
}

ManifestLoad load_manifest(const std::filesystem::path& archive) {
    ManifestLoad result;
    auto path = archive;
    path += ".json";
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory || (!ec && status.type() == std::filesystem::file_type::not_found)) return result;
    result.present = true;
    auto reject = [&](const char* why) { result.valid = false; result.issue = why; return result; };
    if (ec) return reject("Cannot inspect metadata manifest");
    if (std::filesystem::is_symlink(status)) return reject("Metadata manifest must not be a symlink");
    if (!std::filesystem::is_regular_file(status)) return reject("Metadata manifest is not a regular file");
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > kMaxManifestSize) return reject("Metadata manifest has invalid size");
    std::ifstream f(path, std::ios::binary);
    if (!f) return reject("Cannot open metadata manifest");
    std::string bytes(static_cast<size_t>(size), '\0');
    if (size && !f.read(bytes.data(), static_cast<std::streamsize>(size))) return reject("Cannot read metadata manifest");
    char extra;
    if (f.get(extra)) return reject("Metadata manifest has invalid size");
    if (!f.eof()) return reject("Cannot read metadata manifest");
    std::string error;
    if (!Parser(bytes).parse(result.metadata, error)) {
        result.valid = false;
        result.issue = "Invalid metadata manifest: " + error;
    }
    return result;
}
}
