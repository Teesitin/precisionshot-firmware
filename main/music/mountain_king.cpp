// In the Hall of the Mountain King — public-domain score excerpt.
// Edvard Grieg; typeset by Coyau.
// Source: https://www.mutopiaproject.org/ftp/GriegE/O46/Dans_l_antre_du_roi_de_la_montagne/Dans_l_antre_du_roi_de_la_montagne.ly
// Opening four-bar theme, omitting introductory chord; raised register.
// Tempo: quarter=138; total duration: 6957 ms.
// Score and attribution retained in sources/ and README.md.
#include "music.h"
#include "hardware.h"

namespace music {
namespace {
// Frequency in Hz, time in ms. A zero frequency is a rest.
constexpr hardware::SpeakerNote NOTES[] = {
    {494, 196}, // B4: 2/16
    {0, 21},
    {554, 197}, // C#5: 2/16
    {0, 21},
    {587, 196}, // D5: 2/16
    {0, 21},
    {659, 197}, // E5: 2/16
    {0, 21},
    {740, 196}, // F#5: 2/16
    {0, 21},
    {587, 196}, // D5: 2/16
    {0, 21},
    {740, 410}, // F#5: 4/16
    {0, 25},
    {698, 197}, // F5: 2/16
    {0, 21},
    {554, 196}, // C#5: 2/16
    {0, 21},
    {698, 410}, // F5: 4/16
    {0, 25},
    {659, 196}, // E5: 2/16
    {0, 21},
    {523, 196}, // C5: 2/16
    {0, 21},
    {659, 410}, // E5: 4/16
    {0, 25},
    {494, 197}, // B4: 2/16
    {0, 21},
    {554, 196}, // C#5: 2/16
    {0, 21},
    {587, 196}, // D5: 2/16
    {0, 21},
    {659, 197}, // E5: 2/16
    {0, 21},
    {740, 196}, // F#5: 2/16
    {0, 21},
    {587, 197}, // D5: 2/16
    {0, 21},
    {740, 196}, // F#5: 2/16
    {0, 21},
    {988, 196}, // B5: 2/16
    {0, 21},
    {880, 197}, // A5: 2/16
    {0, 21},
    {740, 196}, // F#5: 2/16
    {0, 21},
    {587, 197}, // D5: 2/16
    {0, 21},
    {740, 196}, // F#5: 2/16
    {0, 21},
    {880, 845}, // A5: 8/16
    {0, 25},
};
} // namespace

bool playMountainKing() {
    return hardware::startSpeakerSequence(NOTES, sizeof(NOTES) / sizeof(NOTES[0]));
}
} // namespace music
