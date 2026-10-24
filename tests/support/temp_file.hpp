#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace basalt::test {

// A uniquely named file in the system temp directory, removed on destruction.
class TempFile {
public:
    explicit TempFile(std::string_view contents,
                      std::string_view extension = ".json")
    {
        static std::atomic<unsigned> counter{0};
        const auto stamp =
            std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("basalt_test_" + std::to_string(stamp) + "_" +
                 std::to_string(counter++) + std::string(extension));
        std::ofstream out(path_, std::ios::binary);
        out << contents;
    }

    TempFile(const TempFile &) = delete;
    TempFile &operator=(const TempFile &) = delete;
    TempFile(TempFile &&) = delete;
    TempFile &operator=(TempFile &&) = delete;

    ~TempFile()
    {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    [[nodiscard]] const std::filesystem::path &path() const
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

} // namespace basalt::test
