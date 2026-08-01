#pragma once

#include "race_pause_flow.hpp"

// Raylib-facing race pause overlay. It gathers keyboard, pointer, and gamepad
// intent but leaves recovery, restart, navigation, and quit policy to the
// application layer.
class RacePauseMenu {
public:
    RacePauseMenu();

    void Open(bool returnsToMainMenu);
    void Close();
    bool IsOpen() const;
    void Update();
    void Draw() const;
    RacePauseAction ConsumeAction();

private:
    RacePauseFlow flow_;
    bool open_;
    bool returnsToMainMenu_;
};
