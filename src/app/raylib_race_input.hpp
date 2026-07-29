#pragma once

#include "../race/race_input.hpp"

// Samples the current Raylib devices into the plain race input contract. The
// application owns this adapter so race simulation remains device-independent.
RaceInput ReadRaylibRaceInput();
