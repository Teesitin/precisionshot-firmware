"""Make the four C++ beeper tunes from our note lists.

Transcribed from the retained public-domain Mutopia LilyPond scores. Tokens
use scientific pitch notation and sixteenth-note units, with ties combined.
Tempos and octaves are adapted for short beeper playback. Run from any folder.
"""
from pathlib import Path
import math

ROOT = Path(__file__).resolve().parent
SONGS = {
    'entertainer': ('The Entertainer', 'playEntertainer', 120,
        'Scott Joplin; typeset by Chris Sawer, revised by Simon Albrecht',
        'https://www.mutopiaproject.org/ftp/JoplinS/entertainer/entertainer.ly',
        'Main theme opening; melody only, chord tops retained, ties combined.',
        'D4:1 D#4:1 E4:1 C5:2 E4:1 C5:2 E4:1 C5:6 '
        'C6:1 D6:1 D#6:1 E6:1 C6:1 D6:1 E6:2 B5:1 D6:2 C6:6'),
    'mountain_king': ('In the Hall of the Mountain King', 'playMountainKing', 138,
        'Edvard Grieg; typeset by Coyau',
        'https://www.mutopiaproject.org/ftp/GriegE/O46/Dans_l_antre_du_roi_de_la_montagne/Dans_l_antre_du_roi_de_la_montagne.ly',
        'Opening four-bar theme, omitting introductory chord; raised register.',
        'B4:2 C#5:2 D5:2 E5:2 F#5:2 D5:2 F#5:4 '
        'F5:2 C#5:2 F5:4 E5:2 C5:2 E5:4 '
        'B4:2 C#5:2 D5:2 E5:2 F#5:2 D5:2 F#5:2 B5:2 '
        'A5:2 F#5:2 D5:2 F#5:2 A5:8'),
    'fur_elise': ('Fur Elise', 'playFurElise', 96,
        'Ludwig van Beethoven; typeset by Stelios Samelis',
        'https://www.mutopiaproject.org/ftp/BeethovenLv/WoO59/fur_Elise_WoO59/fur_Elise_WoO59.ly',
        'Opening right-hand phrase through the first C5; faster preview tempo.',
        'E5:1 D#5:1 E5:1 D#5:1 E5:1 B4:1 D5:1 C5:1 '
        'A4:2 R:1 C4:1 E4:1 A4:1 B4:2 R:1 E4:1 G#4:1 B4:1 C5:2'),
    'greensleeves': ('Greensleeves', 'playGreensleeves', 120,
        'Traditional; typeset by Steve Dunlop',
        'https://www.mutopiaproject.org/ftp/Traditional/greensleeves/greensleeves.ly',
        'Soprano opening phrase in E minor, raised one octave; preview tempo.',
        'E5:2 G5:4 A5:2 B5:3 C6:1 B5:2 A5:4 F#5:2 '
        'D5:3 E5:1 F#5:2 G5:4 E5:2 E5:3 D#5:1 E5:2 F#5:6 B4:4'),
}

def frequency(pitch):
    # Convert a note like A4 to its frequency. R means a rest.
    if pitch == 'R':
        return 0
    semitone = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}[pitch[0]]
    semitone += 1 if '#' in pitch else 0
    midi = 12 * (int(pitch[-1]) + 1) + semitone
    return math.floor(440 * 2 ** ((midi - 69) / 12) + 0.5)

def render(song):
    # Turn the notes into timed speaker events and write a C++ tune file.
    title, function, bpm, credit, url, adaptation, tokens = song
    rows = []
    elapsed = 0
    previous_ms = 0
    for token in tokens.split():
        pitch, units = token.split(':')
        elapsed += int(units) * 15000 / bpm
        end_ms = math.floor(elapsed + 0.5)
        duration = end_ms - previous_ms
        previous_ms = end_ms
        hz = frequency(pitch)
        gap = min(25, duration // 10) if hz else 0
        rows.append(f'    {{{hz}, {duration - gap}}}, // {pitch}: {units}/16')
        if gap:
            rows.append(f'    {{0, {gap}}},')
    return ('// ' + title + ' — public-domain score excerpt.\n'
            '// ' + credit + '.\n// Source: ' + url + '\n'
            '// ' + adaptation + '\n'
            f'// Tempo: quarter={bpm}; total duration: {previous_ms} ms.\n'
            '// Score and attribution retained in sources/ and README.md.\n'
            '#include "music.h"\n#include "hardware.h"\n\n'
            'namespace music {\nnamespace {\n'
            '// Frequency in Hz, time in ms. A zero frequency is a rest.\n'
            'constexpr hardware::SpeakerNote NOTES[] = {\n'
            + '\n'.join(rows) + '\n};\n} // namespace\n\n'
            f'bool {function}() {{\n'
            '    return hardware::startSpeakerSequence(NOTES, sizeof(NOTES) / sizeof(NOTES[0]));\n'
            '}\n} // namespace music\n')

if __name__ == '__main__':
    for name, song in SONGS.items():
        path = ROOT / (name + '.cpp')
        path.write_text(render(song), encoding='utf-8')
        print(path.name)
