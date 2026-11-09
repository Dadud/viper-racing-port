#include "native_resource_store.h"

#include <algorithm>

namespace native_resource {
namespace {

std::string path_key(const std::filesystem::path& path) {
    std::string key = path.lexically_normal().generic_string();
#ifdef _WIN32
    for (char& c : key) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
#endif
    return key;
}

const char* resolution_error(content::ResolutionStatus status) {
    switch (status) {
    case content::ResolutionStatus::missing: return "Resource is missing from the selected package and shared archives";
    case content::ResolutionStatus::ambiguous: return "Resource name is ambiguous in the selected archive";
    case content::ResolutionStatus::indeterminate: return "Resource cannot be verified because an archive is invalid";
    case content::ResolutionStatus::found: break;
    }
    return "Resource resolution failed";
}

} // namespace

bool Store::begin(const content::RaceSelection& selection,
                  const std::vector<content::Entry>& shared_archives,
                  std::string& error) {
    clear();
    error.clear();
    if (selection.car_id.empty() || selection.car_archive.empty() ||
        selection.track_id.empty() || selection.track_archive.empty()) {
        error = "Race selection is missing a car or track identity";
        return false;
    }
    selection_ = selection;
    shared_archives_ = shared_archives;

    content::Entry car{};
    car.kind = content::Kind::car;
    car.path = selection_.car_archive;
    car.id = selection_.car_id;
    car.resources = selection_.car_resources;
    car.valid = true;
    if (!reader_for(car, error)) {
        clear();
        return false;
    }

    content::Entry track{};
    track.kind = content::Kind::track;
    track.path = selection_.track_archive;
    track.id = selection_.track_id;
    track.resources = selection_.track_resources;
    track.valid = true;
    if (!reader_for(track, error)) {
        clear();
        return false;
    }
    return true;
}

void Store::clear() {
    readers_.clear();
    shared_archives_.clear();
    selection_ = content::RaceSelection{};
}

bool Store::active() const {
    if (selection_.car_id.empty() || selection_.track_id.empty()) return false;
    const auto car = readers_.find(path_key(selection_.car_archive));
    const auto track = readers_.find(path_key(selection_.track_archive));
    return car != readers_.end() && car->second.is_open() &&
           track != readers_.end() && track->second.is_open();
}

bool Store::expected_archive(const std::filesystem::path& path,
                             content::Entry& entry, std::string& error) const {
    const std::string key = path_key(path);
    if (key == path_key(selection_.car_archive)) {
        entry = content::Entry{};
        entry.kind = content::Kind::car;
        entry.path = selection_.car_archive;
        entry.id = selection_.car_id;
        entry.resources = selection_.car_resources;
        entry.valid = true;
        return true;
    }
    if (key == path_key(selection_.track_archive)) {
        entry = content::Entry{};
        entry.kind = content::Kind::track;
        entry.path = selection_.track_archive;
        entry.id = selection_.track_id;
        entry.resources = selection_.track_resources;
        entry.valid = true;
        return true;
    }
    for (const auto& shared : shared_archives_) {
        if (path_key(shared.path) != key) continue;
        if (shared.kind != content::Kind::shared || !shared.valid) {
            error = "Resolved shared archive is not valid: " + shared.path.string();
            return false;
        }
        entry = shared;
        return true;
    }
    error = "Resolved archive is outside the active race content snapshot: " + path.string();
    return false;
}

content::ArchiveReader* Store::reader_for(const content::Entry& entry,
                                          std::string& error) {
    const std::string key = path_key(entry.path);
    auto found = readers_.find(key);
    if (found != readers_.end()) {
        if (!found->second.is_open()) {
            error = "Validated resource archive handle is no longer open";
            return nullptr;
        }
        return &found->second;
    }
    auto inserted = readers_.try_emplace(key);
    if (!inserted.first->second.open(entry, error)) {
        readers_.erase(inserted.first);
        return nullptr;
    }
    return &inserted.first->second;
}

bool Store::read(content::RacePackage package, const std::string& name,
                 size_t max_size, std::vector<uint8_t>& bytes,
                 std::string& error, content::Resource* descriptor) {
    error.clear();
    if (!active()) {
        error = "No validated native race selection is active";
        return false;
    }
    const content::Resolution resolution =
        content::resolve_selected_resource(name, selection_, package, shared_archives_);
    if (resolution.status != content::ResolutionStatus::found) {
        error = std::string(resolution_error(resolution.status)) + ": " + name;
        return false;
    }
    content::Entry expected{};
    if (!expected_archive(resolution.archive, expected, error)) return false;
    content::ArchiveReader* reader = reader_for(expected, error);
    return reader && reader->read(name, max_size, bytes, error, descriptor);
}

} // namespace native_resource
