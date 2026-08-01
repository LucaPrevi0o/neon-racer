#include "playable_library_flow.hpp"

#include <algorithm>

PlayableLibraryFlow::PlayableLibraryFlow()
    : itemCount_(0u), selectedIndex_(0u), firstVisibleIndex_(0u) {
}

void PlayableLibraryFlow::Reset(std::size_t itemCount) {
    itemCount_ = itemCount;
    selectedIndex_ = 0u;
    firstVisibleIndex_ = 0u;
}

bool PlayableLibraryFlow::HasSelection() const { return itemCount_ > 0u; }
std::size_t PlayableLibraryFlow::ItemCount() const { return itemCount_; }
std::size_t PlayableLibraryFlow::SelectedIndex() const { return selectedIndex_; }
std::size_t PlayableLibraryFlow::FirstVisibleIndex() const { return firstVisibleIndex_; }

std::size_t PlayableLibraryFlow::VisibleCount() const {
    return firstVisibleIndex_ < itemCount_
        ? std::min(kPageSize, itemCount_ - firstVisibleIndex_)
        : 0u;
}

void PlayableLibraryFlow::SelectPrevious() {
    if (!HasSelection()) return;
    selectedIndex_ = selectedIndex_ == 0u ? itemCount_ - 1u : selectedIndex_ - 1u;
    EnsureSelectionVisible();
}

void PlayableLibraryFlow::SelectNext() {
    if (!HasSelection()) return;
    selectedIndex_ = (selectedIndex_ + 1u) % itemCount_;
    EnsureSelectionVisible();
}

void PlayableLibraryFlow::SelectVisible(std::size_t visibleIndex) {
    const std::size_t absoluteIndex = firstVisibleIndex_ + visibleIndex;
    if (absoluteIndex < itemCount_) selectedIndex_ = absoluteIndex;
}

void PlayableLibraryFlow::PreviousPage() {
    if (!HasPreviousPage()) return;
    const std::size_t row = selectedIndex_ - firstVisibleIndex_;
    firstVisibleIndex_ -= kPageSize;
    selectedIndex_ = std::min(firstVisibleIndex_ + row, itemCount_ - 1u);
}

void PlayableLibraryFlow::NextPage() {
    if (!HasNextPage()) return;
    const std::size_t row = selectedIndex_ - firstVisibleIndex_;
    firstVisibleIndex_ += kPageSize;
    selectedIndex_ = std::min(firstVisibleIndex_ + row, itemCount_ - 1u);
}

bool PlayableLibraryFlow::HasPreviousPage() const { return firstVisibleIndex_ >= kPageSize; }

bool PlayableLibraryFlow::HasNextPage() const {
    return firstVisibleIndex_ + kPageSize < itemCount_;
}

void PlayableLibraryFlow::EnsureSelectionVisible() {
    firstVisibleIndex_ = (selectedIndex_ / kPageSize) * kPageSize;
}
