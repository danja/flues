The job is to build an innovative performance instrument that will have the Novation Launchpad Mk3 as user interface and be implemented as an lv2 plugin. The plugin will be built around a memory map of the launchpad. There will be sequencing and instrument voices.

The instrument will make maximal use of the Launchpad's facilities including pushbuttons and multicolor LEDs.

Communication between the Launchpad and the host system will take place over MIDI according to the manual in docs/reference/launchpad.pdf

We want the system to support note on/off messages and control of instrument parameters with sequencing. There should be clear moving feedback on the Launchpad.

We want to make the system dynamic and intuitive. Use the other plugins under lv2/ as inspiration. We have a blank slate, be imaginative. Drums, synths, Euclidian sequences, TR-808 influence, modular influence, any hybrid.

Ideas might include making use of the cardinal directions NSEW. Changes near the center of the launchpad's grid may make minimal difference to the performance, changes on the extreme edges will be more significant. 

Code should follow best practices and be very modular. Header files should be used to hold system constants such as MIDI configuration so that a different control surface with different parameters make be substituted. 