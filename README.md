# Embedded World 2025 - Zephyr Demo

This demo was developed for the Embedded World 2025 in Nürnberg. It showcases the capabilities of the Zephyr RTOS.

A stepper motor is constantly rotating in alternating directions. Unlike the existing step-dir implementations available in Zephyr it accelerates and decelerates when changing directions or coming to a stop. Rotation speed selection is implemented via Zephyr's input subsystem, allowing for 3 different speeds and a stop mode. The input subsystem also allows for a high degree of interchangability of input sources without changing the application code. On the the EW this is shown by using a potentiometer on one board and a series of buttons on another board.

This also demonstrates another strength of Zephyr, the same application can, without code changes, run on multiple different board and hardware configurations. In the case of this demo, it currently supports the following boards:
- RPI Pico
- Nucleo F303RE
- Nucleo F767ZI
- Mimxrt 1060 EVKB