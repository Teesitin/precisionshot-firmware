// The Entertainer — public-domain score excerpt.
// Scott Joplin; typeset by Chris Sawer, revised by Simon Albrecht.
// Source: https://www.mutopiaproject.org/ftp/JoplinS/entertainer/entertainer.ly
// Main theme opening; melody only, chord tops retained, ties combined.
// Tempo: quarter=120; total duration: 4000 ms.
// Score and attribution retained in sources/ and README.md.
#include "music.h"
#include "hardware.h"

namespace music {
namespace {
// Frequency in Hz, time in ms. A zero frequency is a rest.
constexpr hardware::SpeakerNote NOTES[] = {
    {294, 113}, // D4: 1/16
    {0, 12},
    {311, 113}, // D#4: 1/16
    {0, 12},
    {330, 113}, // E4: 1/16
    {0, 12},
    {523, 225}, // C5: 2/16
    {0, 25},
    {330, 113}, // E4: 1/16
    {0, 12},
    {523, 225}, // C5: 2/16
    {0, 25},
    {330, 113}, // E4: 1/16
    {0, 12},
    {523, 725}, // C5: 6/16
    {0, 25},
    {1047, 113}, // C6: 1/16
    {0, 12},
    {1175, 113}, // D6: 1/16
    {0, 12},
    {1245, 113}, // D#6: 1/16
    {0, 12},
    {1319, 113}, // E6: 1/16
    {0, 12},
    {1047, 113}, // C6: 1/16
    {0, 12},
    {1175, 113}, // D6: 1/16
    {0, 12},
    {1319, 225}, // E6: 2/16
    {0, 25},
    {988, 113}, // B5: 1/16
    {0, 12},
    {1175, 225}, // D6: 2/16
    {0, 25},
    {1047, 725}, // C6: 6/16
    {0, 25},
};
} // namespace

bool playEntertainer() {
    return hardware::startSpeakerSequence(NOTES, sizeof(NOTES) / sizeof(NOTES[0]));
}
} // namespace music
