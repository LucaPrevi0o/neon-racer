#include "neon_racer/editor/draft_library_flow.hpp"

#include <iostream>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

void TestEmptyAndSinglePageState() {
    DraftLibraryFlow flow;
    Expect(flow.ItemCount() == 0 && flow.VisibleCount() == 0 &&
               flow.CurrentPage() == 0 && flow.PageCount() == 0 &&
               !flow.HasPreviousPage() && !flow.HasNextPage(),
           "an empty draft library exposes no pages");

    flow.Reset(5);
    Expect(flow.ItemCount() == 5 && flow.FirstVisibleIndex() == 0 &&
               flow.VisibleCount() == 5 && flow.CurrentPage() == 1 && flow.PageCount() == 1 &&
               !flow.HasPreviousPage() && !flow.HasNextPage(),
           "a short draft list fits on one page");
}

void TestPagingAndFinalPartialPage() {
    DraftLibraryFlow flow;
    flow.Reset(DraftLibraryFlow::kPageSize * 2u + 2u);
    Expect(flow.PageCount() == 3 && flow.VisibleCount() == DraftLibraryFlow::kPageSize,
           "a long draft list reports every required page");

    flow.NextPage();
    Expect(flow.FirstVisibleIndex() == DraftLibraryFlow::kPageSize &&
               flow.CurrentPage() == 2 && flow.HasPreviousPage() && flow.HasNextPage(),
           "next page advances by exactly one visible page");

    flow.NextPage();
    Expect(flow.FirstVisibleIndex() == DraftLibraryFlow::kPageSize * 2u &&
               flow.CurrentPage() == 3 && flow.VisibleCount() == 2 &&
               flow.HasPreviousPage() && !flow.HasNextPage(),
           "the final page exposes only its remaining drafts");

    flow.NextPage();
    Expect(flow.CurrentPage() == 3,
           "paging beyond the final draft page is ignored");

    flow.PreviousPage();
    Expect(flow.CurrentPage() == 2,
           "previous page returns to the preceding group");
}

void TestRefreshClampsDeletedFinalPage() {
    DraftLibraryFlow flow;
    flow.Reset(DraftLibraryFlow::kPageSize * 2u + 1u);
    flow.NextPage();
    flow.NextPage();
    Expect(flow.CurrentPage() == 3,
           "clamp setup reaches the third page");

    flow.Reset(DraftLibraryFlow::kPageSize + 1u);
    Expect(flow.CurrentPage() == 2 &&
               flow.FirstVisibleIndex() == DraftLibraryFlow::kPageSize &&
               flow.VisibleCount() == 1,
           "refreshing after deletions clamps to the new final page");

    flow.Reset(0);
    Expect(flow.CurrentPage() == 0 && flow.FirstVisibleIndex() == 0,
           "removing all drafts resets pagination completely");
}

} // namespace

int main() {
    TestEmptyAndSinglePageState();
    TestPagingAndFinalPartialPage();
    TestRefreshClampsDeletedFinalPage();
    if (failures == 0) std::cout << "Neon Racer draft-library flow tests passed.\n";
    return failures == 0 ? 0 : 1;
}
