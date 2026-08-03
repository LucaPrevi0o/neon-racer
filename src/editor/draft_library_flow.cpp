#include "neon_racer/editor/draft_library_flow.hpp"

#include <algorithm>

DraftLibraryFlow::DraftLibraryFlow() : itemCount_(0), firstVisibleIndex_(0) {}

void DraftLibraryFlow::Reset(std::size_t itemCount) {
    itemCount_ = itemCount;
    if (itemCount_ == 0) {
        firstVisibleIndex_ = 0;
        return;
    }

    const std::size_t lastPageStart = ((itemCount_ - 1u) / kPageSize) * kPageSize;
    if (firstVisibleIndex_ > lastPageStart) firstVisibleIndex_ = lastPageStart;
}

std::size_t DraftLibraryFlow::ItemCount() const { return itemCount_; }
std::size_t DraftLibraryFlow::FirstVisibleIndex() const { return firstVisibleIndex_; }

std::size_t DraftLibraryFlow::VisibleCount() const {
    if (firstVisibleIndex_ >= itemCount_) return 0;
    return std::min(kPageSize, itemCount_ - firstVisibleIndex_);
}

std::size_t DraftLibraryFlow::CurrentPage() const {
    return itemCount_ == 0 ? 0 : firstVisibleIndex_ / kPageSize + 1u;
}

std::size_t DraftLibraryFlow::PageCount() const {
    return itemCount_ == 0 ? 0 : (itemCount_ + kPageSize - 1u) / kPageSize;
}

void DraftLibraryFlow::PreviousPage() {
    if (!HasPreviousPage()) return;
    firstVisibleIndex_ -= kPageSize;
}

void DraftLibraryFlow::NextPage() {
    if (!HasNextPage()) return;
    firstVisibleIndex_ += kPageSize;
}

bool DraftLibraryFlow::HasPreviousPage() const { return firstVisibleIndex_ > 0; }

bool DraftLibraryFlow::HasNextPage() const {
    return firstVisibleIndex_ + kPageSize < itemCount_;
}
