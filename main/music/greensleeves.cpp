// Greensleeves — public-domain score excerpt.
// Traditional; typeset by Steve Dunlop.
// Source: https://www.mutopiaproject.org/ftp/Traditional/greensleeves/greensleeves.ly
// Soprano opening phrase in E minor, raised one octave; preview tempo.
// Tempo: quarter=120; total duration: 6000 ms.
// Score and attribution retained in sources/ and README.md.
#include "music.h"
#include "hardware.h"

namespace music {
namespace {
// Frequency in Hz, time in ms. A zero frequency is a rest.
constexpr hardware::SpeakerNote NOTES[] = {
    {659, 225}, // E5: 2/16
    {0, 25},
    {784, 475}, // G5: 4/16
    {0, 25},
    {880, 225}, // A5: 2/16
    {0, 25},
    {988, 350}, // B5: 3/16
    {0, 25},
    {1047, 113}, // C6: 1/16
    {0, 12},
    {988, 225}, // B5: 2/16
    {0, 25},
    {880, 475}, // A5: 4/16
    {0, 25},
    {740, 225}, // F#5: 2/16
    {0, 25},
    {587, 350}, // D5: 3/16
    {0, 25},
    {659, 113}, // E5: 1/16
    {0, 12},
    {740, 225}, // F#5: 2/16
    {0, 25},
    {784, 475}, // G5: 4/16
    {0, 25},
    {659, 225}, // E5: 2/16
    {0, 25},
    {659, 350}, // E5: 3/16
    {0, 25},
    {622, 113}, // D#5: 1/16
    {0, 12},
    {659, 225}, // E5: 2/16
    {0, 25},
    {740, 725}, // F#5: 6/16
    {0, 25},
    {494, 475}, // B4: 4/16
    {0, 25},
};
} // namespace

bool playGreensleeves() {
    return hardware::startSpeakerSequence(NOTES, sizeof(NOTES) / sizeof(NOTES[0]));
}
} // namespace music
