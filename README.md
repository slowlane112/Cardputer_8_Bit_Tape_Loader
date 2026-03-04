# Cardputer 8-Bit Tape Loader

Load Commodore, ZX Spectrum and MSX tape files using a Cardputer Adv.

This project is a work in progress. I assume no responsibility or liability for any errors, omissions, or outcomes resulting from the use of the information provided within this project.

<img src="assets/cardputer_8_bit_tape_loader_1.jpg" alt="Cardputer 8 Bit Tape Loader" width="600">

## Connections

G3: Commodore data pin (D-4 cassette read)<br>
G4: Commodore sense pin (F-6 cassette sense)<br>
G6: Commodore motor pin (C-3 cassette motor)<br>
G5: Audio out<br>
G15: Remote jack

**G4 - Commodore sense pin - connect via 2N3904 transistor**
Cardputer G4 -> 4k7 resistor -> 2N3904 base<br>
Commodore sense pin -> 2N3904 collector<br>
2N3904 emitter -> ground<br>
2N3904 base -> 100k resistor -> ground<br>


**G6 - Commodore motor pin - needs voltage divider**
Cardputer G6 -> 15k resistor -> Commodore motor pin<br>
Commodore motor pin -> 15k resistor -> ground

**G3 - Commodore data pin**
Cardputer G3 -> direct -> Commodore data pin

**G5 - Audio Out**
Cardputer G5 -> 100 ohm resistor -> audio jack

**G15 - Remote**
Cardputer G15 -> direct -> Remote jack
