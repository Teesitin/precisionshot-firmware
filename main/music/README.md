# Debug tunes

These four files play short, single-note versions of public-domain tunes.
Both speakers play the same notes through the LM386 on GPIO44.

| Debug button | Tune / function | Length |
|---|---|---:|
| Confetti Chaos | The Entertainer / `playEntertainer()` | 4 seconds |
| Cosmic Orbit | Greensleeves / `playGreensleeves()` | 6 seconds |
| Jelly Bounce | Fur Elise / `playFurElise()` | 3.4 seconds |
| Warp Speed | Mountain King / `playMountainKing()` | 7 seconds |

The main loop handles note timing, so touch and Bluetooth keep working.
Closing an animation stops its sound. A tune returns false if another sound
is already playing; starting a debug animation stops the previous sound first.
Call sound functions from the main task after hardware setup.

## Sources and credits

Each Mutopia score below is marked public domain by its typesetter. We kept
those original files and notices in `sources/`. Our versions leave out the
accompaniment, adjust the tempo or octave, and add short rests between notes.

- [The Entertainer](https://www.mutopiaproject.org/cgibin/piece-info.cgi?id=263): Scott Joplin; Chris Sawer, revised by Simon Albrecht.
- [Mountain King](https://www.mutopiaproject.org/ftp/GriegE/O46/Dans_l_antre_du_roi_de_la_montagne/Dans_l_antre_du_roi_de_la_montagne.ly): Edvard Grieg; Coyau.
- [Fur Elise](https://www.mutopiaproject.org/ftp/BeethovenLv/WoO59/fur_Elise_WoO59/fur_Elise_WoO59.ly): Ludwig van Beethoven; Stelios Samelis.
- [Greensleeves](https://www.mutopiaproject.org/ftp/Traditional/greensleeves/greensleeves.ly): Traditional; Steve Dunlop.

`generate_excerpts.py` stores our note choices and makes the C++ tables again.
Run it with Python if the notes change. Each event stores frequency in Hz and
duration in milliseconds; frequency 0 means silence. The original sheet music
is kept as reference and is not loaded by the board.
