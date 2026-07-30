#pragma once

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <string>

namespace PersistenceInternal {

using StreamWriter = std::function<bool(std::ostream&, std::string&)>;

// Writes through a bounded temporary sibling, flushes it to durable storage,
// and renames it over the destination only after the complete payload closes.
bool WriteAtomically(const std::string& path,
                     std::size_t maximumBytes,
                     const std::string& fileDescription,
                     const StreamWriter& writer,
                     std::string& error);

} // namespace PersistenceInternal
