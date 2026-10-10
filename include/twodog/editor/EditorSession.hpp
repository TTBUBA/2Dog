#pragma once
#include <deque>
#include <iostream>
#include <mutex>
#include <twodog/platform/Window.hpp>
namespace twodog {
struct ConsoleBuffer final : std::streambuf {
    std::streambuf *original;
    std::deque<std::string> lines;
    std::string pending;
    std::mutex mutex;
    explicit ConsoleBuffer(std::streambuf *source) : original(source) {}
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof()))
            return traits_type::not_eof(ch);
        const char c = traits_type::to_char_type(ch);
        std::lock_guard lock(mutex);
        original->sputc(c);
        if (c == '\n' || pending.size() >= 4096) {
            lines.push_back(pending);
            pending.clear();
            if (lines.size() > 1000)
                lines.pop_front();
        } else
            pending += c;
        return ch;
    }
    int sync() override { return original->pubsync(); }
};
struct ConsoleCapture {
    ConsoleBuffer out{std::cout.rdbuf()}, err{std::cerr.rdbuf()};
    ConsoleCapture() {
        std::cout.rdbuf(&out);
        std::cerr.rdbuf(&err);
    }
    ~ConsoleCapture() {
        std::cout.rdbuf(out.original);
        std::cerr.rdbuf(err.original);
    }
};
struct ImGuiSession {
    explicit ImGuiSession(Window &window);
    ~ImGuiSession();
};
} // namespace twodog
