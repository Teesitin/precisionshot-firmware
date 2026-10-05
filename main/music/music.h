#pragma once

// Start a tune from the main task. Returns false if another sound is playing.
// The main loop updates playback. stopSpeakerSequence() stops it early.
namespace music {
bool playEntertainer();
bool playMountainKing();
bool playFurElise();
bool playGreensleeves();
} // namespace music
