#include <twodog/data/Json.hpp>
#include <atomic>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace twodog {
    namespace {
        [[noreturn]] void bad(const char* message) { throw std::invalid_argument(message);
        }
        class Parser {
            std::string_view source_;
            std::size_t pos_ = 0;
            void space() { while(pos_ < source_.size() && (source_[pos_]==' ' || source_[pos_]=='\r' || source_[pos_]=='\n' || source_[pos_]=='\t')) ++pos_;
            }
            bool take(char c) { space();
                if(pos_ < source_.size() && source_[pos_]==c) { ++pos_;
                    return true;
                } return false;
            }
            unsigned hex4() {
                unsigned n=0;
                for(int i=0;i<4;++i) {
                    if(pos_==source_.size()) bad("Incomplete JSON unicode escape");
                    const char c=source_[pos_++];
                    n*=16;
                    if(c>='0'&&c<='9') n+=static_cast<unsigned>(c-'0');
                    else if(c>='a'&&c<='f') n+=static_cast<unsigned>(c-'a'+10);
                    else if(c>='A'&&c<='F') n+=static_cast<unsigned>(c-'A'+10);
                    else bad("Invalid JSON unicode escape");
                }
                return n;
            }
            static void utf8(std::string& out,unsigned n) {
                if(n<0x80) out+=static_cast<char>(n);
                else if(n<0x800) { out+=static_cast<char>(0xc0|(n>>6));
                    out+=static_cast<char>(0x80|(n&63));
                }
                else if(n<0x10000) { out+=static_cast<char>(0xe0|(n>>12));
                    out+=static_cast<char>(0x80|((n>>6)&63));
                    out+=static_cast<char>(0x80|(n&63));
                }
                else { out+=static_cast<char>(0xf0|(n>>18));
                    out+=static_cast<char>(0x80|((n>>12)&63));
                    out+=static_cast<char>(0x80|((n>>6)&63));
                    out+=static_cast<char>(0x80|(n&63));
                }
            }
            std::string string() {
                if(!take('"')) bad("JSON string expected");
                std::string out;
                while(pos_<source_.size()) {
                    const auto c=static_cast<unsigned char>(source_[pos_++]);
                    if(c=='"') return out;
                    if(c<32) bad("Unescaped JSON control character");
                    if(c!='\\') { out+=static_cast<char>(c);
                        continue;
                    }
                    if(pos_==source_.size()) bad("Incomplete JSON escape");
                    switch(source_[pos_++]) {
                    case '"':out+='"';
                        break;
                    case '\\':out+='\\';
                        break;
                    case '/':out+='/';
                        break;
                    case 'b':out+='\b';
                        break;
                    case 'f':out+='\f';
                        break;
                    case 'n':out+='\n';
                        break;
                    case 'r':out+='\r';
                        break;
                    case 't':out+='\t';
                        break;
                    case 'u': {
                            auto cp=hex4();
                            if(cp>=0xd800 && cp<=0xdbff) {
                                if(pos_+2>source_.size() || source_.substr(pos_,2)!="\\u") bad("Missing low surrogate");
                                pos_+=2;
                                const auto low=hex4();
                                if(low<0xdc00||low>0xdfff) bad("Invalid low surrogate");
                                cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);
                            } else if(cp>=0xdc00&&cp<=0xdfff) bad("Unexpected low surrogate");
                            utf8(out,cp);
                            break;
                        }
                    default:bad("Unknown JSON escape");
                    }
                }
                bad("Unterminated JSON string");
            }
            Json value(unsigned depth) {
                if(depth>64) bad("JSON nesting exceeds 64 levels");
                space();
                if(pos_==source_.size()) bad("JSON value expected");
                if(source_[pos_]=='"') return Json(string());
                if(take('{')) {
                    Json::Object object;
                    if(take('}')) return object;
                    do {
                        auto key=string();
                        if(!take(':')) bad("JSON colon expected");
                        if(!object.emplace(std::move(key),value(depth+1)).second) bad("Duplicate JSON key");
                    } while(take(','));
                    if(!take('}')) bad("JSON object not closed");
                    return object;
                }
                if(take('[')) {
                    Json::Array array;
                    if(take(']')) return array;
                    do { array.push_back(value(depth+1));
                    } while(take(','));
                    if(!take(']')) bad("JSON array not closed");
                    return array;
                }
                for(const auto literal:{std::string_view("true"),std::string_view("false"),std::string_view("null")}) {
                    if(source_.substr(pos_,literal.size())==literal) {pos_+=literal.size();
                        if(literal=="null") return {};
                        return Json(literal=="true");
                    }
                }
                const auto start=pos_;
                if(source_[pos_]=='-') ++pos_;
                if(pos_==source_.size()) bad("Incomplete JSON number");
                if(source_[pos_]=='0') ++pos_;
                else {
                    if(source_[pos_]<'1'||source_[pos_]>'9') bad("Invalid JSON value");
                    while(pos_<source_.size()&&source_[pos_]>='0'&&source_[pos_]<='9') ++pos_;
                }
                if(pos_<source_.size()&&source_[pos_]=='.') {
                    const auto begin=++pos_;
                    while(pos_<source_.size()&&source_[pos_]>='0'&&source_[pos_]<='9') ++pos_;
                    if(begin==pos_) bad("JSON fraction expected");
                }
                if(pos_<source_.size()&&(source_[pos_]=='e'||source_[pos_]=='E')) {
                    ++pos_;
                    if(pos_<source_.size()&&(source_[pos_]=='+'||source_[pos_]=='-')) ++pos_;
                    const auto begin=pos_;
                    while(pos_<source_.size()&&source_[pos_]>='0'&&source_[pos_]<='9') ++pos_;
                    if(begin==pos_) bad("JSON exponent expected");
                }
                double n=0;
                const auto result=std::from_chars(source_.data()+start,source_.data()+pos_,n);
                if(result.ec!=std::errc{} || result.ptr!=source_.data()+pos_) bad("JSON number out of range");
                return n;
            }
            public:
            explicit Parser(std::string_view s):source_(s) {}
            Json parse() {auto result=value(0);
                space();
                if(pos_!=source_.size()) bad("Trailing JSON data");
                return result;
            }
        };
        std::string quoted(const std::string& value) {
            std::string out="\"";
            constexpr char hex[]="0123456789abcdef";
            for(unsigned char c:value) {
                if(c=='"'||c=='\\') {out+='\\';
                    out+=static_cast<char>(c);
                }
                else if(c<32) {out+="\\u00";
                    out+=hex[c>>4];
                    out+=hex[c&15];
                }
                else out+=static_cast<char>(c);
            }
            return out+'"';
        }
    }
    Json::Json(double value):value_(value) {if(!std::isfinite(value)) bad("Non-finite JSON number");
    }
    bool Json::null() const noexcept {return std::holds_alternative<std::nullptr_t>(value_);
    }
    bool Json::isObject() const noexcept {return std::holds_alternative<Object>(value_);
    }
    bool Json::isArray() const noexcept {return std::holds_alternative<Array>(value_);
    }
    bool Json::boolean() const {return std::get<bool>(value_);
    }
    double Json::number() const {return std::get<double>(value_);
    }
    int Json::integer() const {const auto v=number();
        if(v<std::numeric_limits<int>::min()||v>std::numeric_limits<int>::max()||std::trunc(v)!=v) bad("Integer out of range");
        return static_cast<int>(v);
    }
    const std::string& Json::string() const {return std::get<std::string>(value_);
    }
    const Json::Array& Json::array() const {return std::get<Array>(value_);
    }
    Json::Array& Json::array() {return std::get<Array>(value_);
    }
    const Json::Object& Json::object() const {return std::get<Object>(value_);
    }
    Json::Object& Json::object() {return std::get<Object>(value_);
    }
    bool Json::contains(std::string_view key) const {return isObject()&&object().contains(key);
    }
    const Json& Json::at(std::string_view key) const {const auto it=object().find(key);
        if(it==object().end()) throw std::invalid_argument("Missing field: "+std::string(key));
        return it->second;
    }
    Json& Json::at(std::string_view key) {return const_cast<Json&>(std::as_const(*this).at(key));
    }
    Json& Json::operator[](std::string key) {if(null()) value_=Object{};
        return object()[std::move(key)];
    }
    std::string Json::text(std::string_view key,std::string fallback) const {return contains(key)?at(key).string():std::move(fallback);
    }
    double Json::num(std::string_view key,double fallback) const {return contains(key)?at(key).number():fallback;
    }
    bool Json::flag(std::string_view key,bool fallback) const {return contains(key)?at(key).boolean():fallback;
    }
    std::string Json::dump() const {
        if(null()) return "null";
        if(std::holds_alternative<bool>(value_)) return boolean()?"true":"false";
        if(std::holds_alternative<double>(value_)) {char buffer[64];
            const auto result=std::to_chars(buffer,buffer+64,number(),std::chars_format::general,std::numeric_limits<double>::max_digits10);
            if(result.ec!=std::errc{}) bad("Cannot format number");
            return {buffer,result.ptr};
        }
        if(std::holds_alternative<std::string>(value_)) return quoted(string());
        std::string out=isArray()?"[":"{";
        bool first=true;
        if(isArray()) {for(const auto& v:array()) {if(!first) out+=',';
                first=false;
                out+=v.dump();
            }}
        else {for(const auto& [key,v]:object()) {if(!first) out+=',';
                first=false;
                out+=quoted(key)+":"+v.dump();
            }}
        return out+(isArray()?']':'}');
    }
    Json Json::parse(std::string_view source) {if(source.size()>16*1024*1024) bad("JSON exceeds 16 MiB");
        return Parser(source).parse();
    }
    Json Json::read(const std::filesystem::path& path) {
        const auto size=std::filesystem::file_size(path);
        if(size>16*1024*1024) bad("JSON exceeds 16 MiB");
        std::ifstream file(path,std::ios::binary);
        if(!file) throw std::runtime_error("Cannot read "+path.string());
        std::string bytes(static_cast<std::size_t>(size),'\0');
        file.read(bytes.data(),static_cast<std::streamsize>(size));
        if(!file) throw std::runtime_error("Incomplete read: "+path.string());
        // PowerShell and editors may emit an UTF-8 BOM.
        return parse(bytes.starts_with("\xef\xbb\xbf")?std::string_view(bytes).substr(3):std::string_view(bytes));
    }
    void Json::save(const std::filesystem::path& path) const {
        static std::atomic<unsigned> sequence{0};
        auto temporary=path;
        temporary+=".tmp-"+std::to_string(sequence.fetch_add(1));
        {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
            const auto bytes=dump()+"\n";
            file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
            file.flush();
            if(!file) throw std::runtime_error("Cannot save "+temporary.string());
        }
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot replace "+path.string());
#else
        std::filesystem::rename(temporary,path);
#endif
    }
    std::filesystem::path containedPath(const std::filesystem::path& root,const std::filesystem::path& relative) {
        if(relative.empty()||relative.is_absolute()||relative.has_root_name()) bad("Asset path must be relative");
        const auto base=std::filesystem::weakly_canonical(root);
        const auto full=std::filesystem::weakly_canonical(base/relative);
        auto b=base.begin(),f=full.begin();
        for(;b!=base.end();++b,++f) if(f==full.end()||*b!=*f) bad("Asset path escapes project root");
        return full;
    }
}
