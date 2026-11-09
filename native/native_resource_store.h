#pragma once

#include "../hook/content_catalog.h"

#include <map>
#include <string>
#include <vector>

namespace native_resource {

// Host-owned resource view for one revalidated native race selection. It keeps
// exact selected archive handles open and applies the shared catalog's
// selected-package-first resolver for every lookup.
class Store {
public:
    bool begin(const content::RaceSelection& selection,
               const std::vector<content::Entry>& shared_archives,
               std::string& error);
    void clear();
    bool read(content::RacePackage package, const std::string& name,
              size_t max_size, std::vector<uint8_t>& bytes,
              std::string& error, content::Resource* descriptor = nullptr);
    bool active() const;

private:
    bool expected_archive(const std::filesystem::path& path,
                          content::Entry& entry, std::string& error) const;
    content::ArchiveReader* reader_for(const content::Entry& entry,
                                       std::string& error);

    content::RaceSelection selection_;
    std::vector<content::Entry> shared_archives_;
    std::map<std::string, content::ArchiveReader> readers_;
};

} // namespace native_resource
