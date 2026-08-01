#pragma once

#include <cstddef>

// Raylib-free selection and pagination state for the playable-track library.
// The view owns device polling and file presentation while this class keeps
// keyboard, gamepad, and pointer navigation consistent.
class PlayableLibraryFlow {
public:
    static const std::size_t kPageSize = 10u;

    PlayableLibraryFlow();

    void Reset(std::size_t itemCount);
    bool HasSelection() const;
    std::size_t ItemCount() const;
    std::size_t SelectedIndex() const;
    std::size_t FirstVisibleIndex() const;
    std::size_t VisibleCount() const;

    void SelectPrevious();
    void SelectNext();
    void SelectVisible(std::size_t visibleIndex);
    void PreviousPage();
    void NextPage();

    bool HasPreviousPage() const;
    bool HasNextPage() const;

private:
    void EnsureSelectionVisible();

    std::size_t itemCount_;
    std::size_t selectedIndex_;
    std::size_t firstVisibleIndex_;
};
