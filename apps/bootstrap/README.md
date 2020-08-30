# Bootstrap

OLC CODEJAM 2020 entry

Hello 0xA5A5A5A5, we have a very important job for you.

As you know, The Great Machine is the computer which runs the simulation which we call "the universe". While preparing his next YouTube video, "Code-It-Yourself: Unvierse Simulator", Javidx9 has overloaded The Great Machine's power circuits and the inception protection mechanism has been triggered to shutdown The Great Machine, but not before significant damage was done to several critical circuits. The system watchdog, an opto-mechanical Turing machine, has taken control and will direct you in your quest to bootstrap The Great Machine, and bring the universe back online.

Should you fail in your task then your entire department's funding will be pulled and given to that obnoxious moron Jenkins on the second floor, and you and your colleagues will be assigned to other duties. So the stakes aren't really that high. You might as well give it a shot though. You've got until next Tuesday.


## Ideas

* Narrative arc is you bringing on-line core parts of a microcomputer:
    * System clock
    * ALU
    * Registers
    * RAM
    * Instruction fetch and decode
    * CPU control lines
    * Address and data buses

* A puzzle game - puzzle pieces present a turing complete machine. Bringing each part online means solving the relevant puzzle. Puzzles could be building representations of realistic circuits. "Puzzle" element would be constraints due to space for layout, available parts, etc. Something like zachtronic's engineer of the people.

* Action platform game - anything can be a platform game, just add theme.

* Production/Automation/Base building game - at the start of the game the watchdog instructs the player to do everything manually to execute short sequences of code (6502 based pseudo assembly). The player has to set the state of address lines and pulse the clock to read instructions and data, they have to perform the instruction and set the appropriate address, data and control lines with the results and pulse the clock, all manually. As they progress they gain access to components which can be assembled to perform process automatically (and faster). Progress gates require 

* Programming game - player codes each part of the microcomputer using a simple assembly language.