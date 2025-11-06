// =============================================================
// player_sm_json.cpp — tiny JSON (DOM + parser) for FSM loader
// Supports: object, array, string, number, true/false/null
// String escapes: \" \\ \/ \b \f \n \r \t and \uXXXX (BMP; no surrogate pairs)
// UTF-8 input (BOM allowed). Keys/strings建议直接用UTF-8，不依赖\u转义。
// =============================================================

#include "player_sm_json.h"

#include <fstream>
#include <filesystem>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>
#include <unordered_map>
#include <cassert>

namespace smjson {

    // ---------- helpers ----------
    static inline void StripUTF8BOM(std::string& s) {
        if (s.size() >= 3 &&
            (unsigned char)s[0] == 0xEF &&
            (unsigned char)s[1] == 0xBB &&
            (unsigned char)s[2] == 0xBF) {
            s.erase(0, 3);
        }
    }

    static inline void appendUTF8(uint32_t cp, std::string& out) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        }
        else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else {
            // out of BMP (we don't expect this from \uXXXX); encode anyway
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    static inline bool hexval(char c, int& v) {
        if (c >= '0' && c <= '9') { v = c - '0'; return true; }
        if (c >= 'a' && c <= 'f') { v = 10 + (c - 'a'); return true; }
        if (c >= 'A' && c <= 'F') { v = 10 + (c - 'A'); return true; }
        return false;
    }

    // ---------- parser ----------
    struct Parser {
        const char* p;
        const char* e;
        std::string* err;

        Parser(const char* begin, const char* end, std::string* er)
            : p(begin), e(end), err(er) {
        }

        void skipWS() { while (p < e && std::isspace((unsigned char)*p)) ++p; }

        bool match(char c) {
            skipWS();
            if (p < e && *p == c) { ++p; return true; }
            return false;
        }

        bool expect(char c) {
            skipWS();
            if (p < e && *p == c) { ++p; return true; }
            fail(std::string("expected '") + c + "'");
            return false;
        }

        [[noreturn]] void fail(const std::string& m) {
            if (err) *err = m;
            throw 1;
        }

        bool eof() { skipWS(); return p >= e; }

        // value -> object | array | string | number | true | false | null
        Value parseValue() {
            skipWS();
            if (p >= e) fail("unexpected end of input");
            char c = *p;

            if (c == '{') return parseObject();
            if (c == '[') return parseArray();
            if (c == '"') { Value v; v.type = Value::Type::String; v.str = parseString(); return v; }
            if (c == 't') { parseLiteral("true");  Value v; v.type = Value::Type::Bool;  v.b = true;  return v; }
            if (c == 'f') { parseLiteral("false"); Value v; v.type = Value::Type::Bool;  v.b = false; return v; }
            if (c == 'n') { parseLiteral("null");  Value v; v.type = Value::Type::Null;  return v; }

            // number
            Value v; v.type = Value::Type::Number; v.num = parseNumber(); return v;
        }

        Value parseObject() {
            Value v; v.type = Value::Type::Object;
            expect('{');
            skipWS();
            if (match('}')) return v;

            while (true) {
                skipWS();
                if (p >= e || *p != '"') fail("object key must be a string");
                std::string key = parseString();
                expect(':');
                Value val = parseValue();
                v.obj.emplace(std::move(key), std::move(val));

                if (match('}')) break;
                if (!match(',')) fail("expected ',' or '}' in object");
            }
            return v;
        }

        Value parseArray() {
            Value v; v.type = Value::Type::Array;
            expect('[');
            skipWS();
            if (match(']')) return v;

            while (true) {
                Value elem = parseValue();
                v.arr.emplace_back(std::move(elem));
                if (match(']')) break;
                if (!match(',')) fail("expected ',' or ']' in array");
            }
            return v;
        }

        std::string parseString() {
            std::string s;
            if (*p != '"') fail("string must start with '\"'");
            ++p; // consume opening quote
            while (p < e) {
                char c = *p++;
                if (c == '"') break; // closing quote
                if (c == '\\') {
                    if (p >= e) fail("bad escape");
                    char esc = *p++;
                    switch (esc) {
                    case '"': s.push_back('"');  break;
                    case '\\': s.push_back('\\'); break;
                    case '/': s.push_back('/');   break;
                    case 'b': s.push_back('\b');  break;
                    case 'f': s.push_back('\f');  break;
                    case 'n': s.push_back('\n');  break;
                    case 'r': s.push_back('\r');  break;
                    case 't': s.push_back('\t');  break;
                    case 'u': {
                        // read 4 hex digits (BMP code point)
                        int h = 0, vcode = 0;
                        for (int i = 0; i < 4; ++i) {
                            if (p >= e || !hexval(*p++, h)) fail("bad \\uXXXX");
                            vcode = (vcode << 4) | h;
                        }
                        // Note: no surrogate-pair handling; assume not used in our FSM JSON.
                        appendUTF8((uint32_t)vcode, s);
                    } break;
                    default: fail("unknown escape");
                    }
                }
                else {
                    s.push_back(c);
                }
            }
            return s;
        }

        void parseLiteral(const char* lit) {
            for (const char* q = lit; *q; ++q) {
                if (p >= e || *p != *q) fail(std::string("expected literal '") + lit + "'");
                ++p;
            }
        }

        double parseNumber() {
            skipWS();
            const char* b = p;
            if (p < e && (*p == '-' || *p == '+')) ++p;
            while (p < e && std::isdigit((unsigned char)*p)) ++p;
            if (p < e && *p == '.') { ++p; while (p < e && std::isdigit((unsigned char)*p)) ++p; }
            if (p < e && (*p == 'e' || *p == 'E')) {
                ++p;
                if (p < e && (*p == '+' || *p == '-')) ++p;
                while (p < e && std::isdigit((unsigned char)*p)) ++p;
            }
            return std::strtod(b, nullptr);
        }
    };

    // ---------- public API ----------
    bool ParseText(const std::string& utf8, Value& out, std::string* err) {
        std::string s = utf8;
        StripUTF8BOM(s);
        try {
            Parser ps(s.data(), s.data() + s.size(), err);
            out = ps.parseValue();
            ps.skipWS();
            if (!ps.eof()) { if (err) *err = "extra characters after JSON value"; return false; }
            return true;
        }
        catch (...) {
            return false;
        }
    }

    bool ParseFileUTF8(const wchar_t* path, Value& out, std::string* err) {
        std::ifstream ifs(std::filesystem::path(path), std::ios::binary);
        if (!ifs) { if (err) *err = "cannot open file"; return false; }
        std::string text((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        return ParseText(text, out, err);
    }

} // namespace smjson
