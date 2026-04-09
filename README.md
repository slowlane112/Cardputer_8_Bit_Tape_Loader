# Cardputer 8-Bit Tape Loader

Load Commodore, ZX Spectrum, MSX, Acorn and BBC Micro tape files using a Cardputer Adv.

This project is a work in progress. I assume no responsibility or liability for any errors, omissions, or outcomes resulting from the use of the information provided within this project.

<img src="assets/cardputer_8_bit_tape_loader_1.jpg" alt="Cardputer 8 Bit Tape Loader" width="600">

## Wiring Diagram<br />
<img src="assets/wiring_diagram.png" alt="Wiring Diagram" width="1000">

## Usage<br />
The app lets you browse and select tape files from the SD card.<br />
All tape files must be uncompressed tape images.<br />

### File Browser<br />
Arrow Up: Move up<br />
Arrow Down: Move down<br />
Enter: Select<br />
Backspace: Back<br />
Letter: Move to next directory or file beginning with that letter.<br />

### Tape Player<br />
1: Play<br />
2: Stop<br />
3: Reset<br />
Backspace: Exit<br />

### Commodore<br />
Supports tap files.<br />
Press M to enable/disable motor control.<br />

### ZX Spectrum<br />
Supports tap and tzx files.<br />
Press Arrow Left and Arrow Right to toggle between 48K and 128K mode.<br />

### MSX<br />
Supports cas files.<br />
Press R to enable/disable remote control.<br />

### Acorn and BBC Micro<br />
Supports uef and hq files.<br />
Press R to enable/disable remote control.<br />

<br />Make sure uef files are uncompressed. Some uef files are gzip‑compressed and will need to be decompressed using a tool like gzip.<br />
