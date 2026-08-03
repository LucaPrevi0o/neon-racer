#pragma once

#include <algorithm>
#include <cstddef>

// Raylib-free pagination for the editor's saved-draft library. The editor view
// maps keyboard, gamepad, wheel, and pointer input to this state while draft
// loading remains owned by TrackEditor.
class DraftLibraryFlow {
public:
    static const std::size_t kPageSize = 9u;

    DraftLibraryFlow() : itemCount_(0), firstVisibleIndex_(0) {}

    // Updates the available item count while keeping the current page when it
    // remains valid and clamping to the final page after deletions/refreshes.
    void Reset(std::size_t itemCount) {
        itemCount_ = itemCount;
        if (itemCount_ == 0) {
            firstVisibleIndex_ = 0;
            return;
        }

        const std::size_t lastPageStart = ((itemCount_ - 1u) / kPageSize) * kPageSize;
        if (firstVisibleIndex_ > lastPageStart) firstVisibleIndex_ = lastPageStart;
    }

    std::size_t ItemCount() const { return itemCount_; }
    std::size_t FirstVisibleIndex() const { return firstVisibleIndex_; }

    std::size_t VisibleCount() const {
        if (firstVisibleIndex_ >= itemCount_) return 0;
        return std::min(kPageSize, itemCount_ - firstVisibleIndex_);
    }

    std::size_t CurrentPage() const {
        return itemCount_ == 0 ? 0 : firstVisibleIndex_ / kPageSize + 1u;
    }

    std::size_t PageCount() const {
        return itemCount_ == 0 ? 0 : (itemCount_ + kPageSize - 1u) / kPageSize;
    }

    void PreviousPage() {
        if (HasPreviousPage()) firstVisibleIndex_ -= kPageSize;
    }

    void NextPage() {
        if (HasNextPage()) firstVisibleIndex_ += kPageSize;
    }

    bool HasPreviousPage() const { return firstVisibleIndex_ > 0; }
    bool HasNextPage() const { return firstVisibleIndex_ + kPageSize < itemCount_; }

private:
    std::size_t itemCount_;
    std::size_t firstVisibleIndex_;
};
