#include "../internal/atomic_file_writer.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <ostream>
#include <streambuf>
#include <unistd.h>
#include <vector>

namespace {

class FileDescriptorStreamBuffer : public std::streambuf {
public:
    FileDescriptorStreamBuffer(int descriptor, std::size_t maximumBytes)
        : descriptor_(descriptor), maximumBytes_(maximumBytes), writtenBytes_(0), failed_(false) {}

    ~FileDescriptorStreamBuffer() { Close(); }

    bool Close() {
        if (descriptor_ < 0) return !failed_;
        const int descriptor = descriptor_;
        descriptor_ = -1;
        if (close(descriptor) == 0) return !failed_;
        failed_ = true;
        return false;
    }

protected:
    std::streamsize xsputn(const char* data, std::streamsize count) override {
        if (count <= 0) return 0;
        const std::size_t requested = static_cast<std::size_t>(count);
        if (requested > maximumBytes_ - writtenBytes_) {
            errno = EFBIG;
            failed_ = true;
            return 0;
        }

        std::streamsize completed = 0;
        while (completed < count) {
            const ssize_t result = write(descriptor_, data + completed,
                                         static_cast<std::size_t>(count - completed));
            if (result > 0) {
                completed += static_cast<std::streamsize>(result);
                writtenBytes_ += static_cast<std::size_t>(result);
                continue;
            }
            if (result < 0 && errno == EINTR) continue;
            failed_ = true;
            break;
        }
        return completed;
    }

    int_type overflow(int_type character = traits_type::eof()) override {
        if (character == traits_type::eof()) return traits_type::not_eof(character);
        const char value = static_cast<char>(character);
        return xsputn(&value, 1) == 1 ? character : traits_type::eof();
    }

    int sync() override {
        if (descriptor_ < 0 || fsync(descriptor_) != 0) {
            failed_ = true;
            return -1;
        }
        return 0;
    }

private:
    int descriptor_;
    std::size_t maximumBytes_;
    std::size_t writtenBytes_;
    bool failed_;
};

} // namespace

namespace PersistenceInternal {

bool WriteAtomically(const std::string& path,
                     std::size_t maximumBytes,
                     const std::string& fileDescription,
                     const StreamWriter& writer,
                     std::string& error) {
    std::string temporaryPattern = path + ".tmp.XXXXXX";
    std::vector<char> temporaryCharacters(temporaryPattern.begin(), temporaryPattern.end());
    temporaryCharacters.push_back('\0');
    const int descriptor = mkstemp(&temporaryCharacters[0]);
    if (descriptor < 0) {
        error = std::string("Could not create temporary ") + fileDescription + ": " + std::strerror(errno);
        return false;
    }

    const std::string temporaryPath(&temporaryCharacters[0]);
    bool completed = false;
    bool closed = false;
    {
        FileDescriptorStreamBuffer buffer(descriptor, maximumBytes);
        {
            std::ostream output(&buffer);
            if (writer(output, error)) {
                output.flush();
                completed = static_cast<bool>(output);
            }
        }
        closed = buffer.Close();
    }

    if (!completed || !closed) {
        std::remove(temporaryPath.c_str());
        if (error.empty()) error = std::string("Could not finish writing ") + fileDescription + ".";
        return false;
    }
    if (std::rename(temporaryPath.c_str(), path.c_str()) != 0) {
        error = std::string("Could not finalize ") + fileDescription + ": " + std::strerror(errno);
        std::remove(temporaryPath.c_str());
        return false;
    }
    return true;
}

} // namespace PersistenceInternal
