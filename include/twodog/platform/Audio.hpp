#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
namespace twodog {
class Audio final {
public:
    Audio();~Audio();Audio(const Audio&)=delete;Audio& operator=(const Audio&)=delete;
    std::string play(const std::filesystem::path& file,std::string_view bus,bool loop);
    void stop(std::string_view voice);
    void volume(std::string_view bus,float value);
    void pause(bool value);
    void reset();
    void collect();
    bool available() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
