#pragma once

#include <cstddef>

// Raylib-free pagination for the editor's saved-draft library. The editor view
// maps keyboard, gamepad, wheel, and pointer input to this state while draft
// loading remains owned by TrackEditor.
class DraftLibraryFlow {
public:
    static const std::size_t kPageSize = 9u;

    DraftLibraryFlow();

    // Updates the available item count while keeping the current page when it
    // remains valid and clamping to the final page after deletions/refreshes.
    void Reset(std::size_t itemCount);

    std::size_t ItemCount() const;
    std::size_t FirstVisibleIndex() const;
    std::size_t VisibleCount() const;
    std::size_t CurrentPage() const;
    std::size_t PageCount() const;

    void PreviousPage();
    void NextPage();

    bool HasPreviousPage() const;
    bool HasNextPage() const;

private:
    std::size_t itemCount_;
    std::size_t firstVisibleIndex_;
};
