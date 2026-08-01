#include "../../src/app/playable_library_flow.hpp"

#include <iostream>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

void TestEmptyLibraryHasNoSelection() {
    PlayableLibraryFlow flow;
    flow.Reset(0u);
    Expect(!flow.HasSelection() && flow.VisibleCount() == 0u,
           "empty library exposes no selected or visible package");
    flow.SelectNext();
    flow.NextPage();
    Expect(flow.SelectedIndex() == 0u && flow.FirstVisibleIndex() == 0u,
           "empty-library navigation is inert");
}

void TestSelectionMovesAcrossPagesAndWraps() {
    PlayableLibraryFlow flow;
    flow.Reset(12u);
    for (int index = 0; index < 10; ++index) flow.SelectNext();
    Expect(flow.SelectedIndex() == 10u && flow.FirstVisibleIndex() == 10u && flow.VisibleCount() == 2u,
           "moving beyond the first page reveals the selected package");

    flow.SelectNext();
    flow.SelectNext();
    Expect(flow.SelectedIndex() == 0u && flow.FirstVisibleIndex() == 0u,
           "next selection wraps from the final package to the first");

    flow.SelectPrevious();
    Expect(flow.SelectedIndex() == 11u && flow.FirstVisibleIndex() == 10u,
           "previous selection wraps from the first package to the final one");
}

void TestPageNavigationPreservesVisibleRow() {
    PlayableLibraryFlow flow;
    flow.Reset(25u);
    flow.SelectVisible(4u);
    flow.NextPage();
    Expect(flow.SelectedIndex() == 14u && flow.FirstVisibleIndex() == 10u,
           "next page preserves the selected row");
    flow.NextPage();
    Expect(flow.SelectedIndex() == 24u && flow.FirstVisibleIndex() == 20u,
           "partial final page clamps the preserved row to the final package");
    flow.PreviousPage();
    Expect(flow.SelectedIndex() == 14u && flow.FirstVisibleIndex() == 10u,
           "previous page returns to the corresponding row");
}

void TestPointerSelectionUsesVisibleRows() {
    PlayableLibraryFlow flow;
    flow.Reset(14u);
    flow.NextPage();
    flow.SelectVisible(2u);
    Expect(flow.SelectedIndex() == 12u,
           "pointer selection maps a visible row to the absolute package index");
    flow.SelectVisible(9u);
    Expect(flow.SelectedIndex() == 12u,
           "pointer selection ignores rows outside the partial page");
}

} // namespace

int main() {
    TestEmptyLibraryHasNoSelection();
    TestSelectionMovesAcrossPagesAndWraps();
    TestPageNavigationPreservesVisibleRow();
    TestPointerSelectionUsesVisibleRows();
    if (failures == 0) std::cout << "Neon Racer playable-library flow tests passed.\n";
    return failures == 0 ? 0 : 1;
}
