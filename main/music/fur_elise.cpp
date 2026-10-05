// Fur Elise — public-domain score excerpt.
// Ludwig van Beethoven; typeset by Stelios Samelis.
// Source: https://www.mutopiaproject.org/ftp/BeethovenLv/WoO59/fur_Elise_WoO59/fur_Elise_WoO59.ly
// Opening right-hand phrase through the first C5; faster preview tempo.
// Tempo: quarter=96; total duration: 3438 ms.
// Score and attribution retained in sources/ and README.md.
#include "music.h"
#include "hardware.h"

namespace music {
namespace {
// Frequency in Hz, time in ms. A zero frequency is a rest.
constexpr hardware::SpeakerNote NOTES[] = {
    {659, 141}, // E5: 1/16
    {0, 15},
    {622, 142}, // D#5: 1/16
    {0, 15},
    {659, 141}, // E5: 1/16
    {0, 15},
    {622, 141}, // D#5: 1/16
    {0, 15},
    {659, 141}, // E5: 1/16
    {0, 15},
    {494, 142}, // B4: 1/16
    {0, 15},
    {587, 141}, // D5: 1/16
    {0, 15},
    {523, 141}, // C5: 1/16
    {0, 15},
    {440, 288}, // A4: 2/16
    {0, 25},
    {0, 156}, // R: 1/16
    {262, 141}, // C4: 1/16
    {0, 15},
    {330, 141}, // E4: 1/16
    {0, 15},
    {440, 142}, // A4: 1/16
    {0, 15},
    {494, 287}, // B4: 2/16
    {0, 25},
    {0, 156}, // R: 1/16
    {330, 142}, // E4: 1/16
    {0, 15},
    {415, 141}, // G#4: 1/16
    {0, 15},
    {494, 141}, // B4: 1/16
    {0, 15},
    {523, 288}, // C5: 2/16
    {0, 25},
};
} // namespace

bool playFurElise() {
    return hardware::startSpeakerSequence(NOTES, sizeof(NOTES) / sizeof(NOTES[0]));
}
} // namespace music
