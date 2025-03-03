# Embedded World 2025 - Zephyr Demo

This demo was developed for the Embedded World 2025 in Nürnberg. It showcases the capabilities of the Zephyr RTOS.

A stepper motor is constantly rotating in alternating directions. Unlike the existing step-dir implementations available in Zephyr it accelerates and decelerates when changing directions or coming to a stop. Rotation speed selection is implemented via Zephyr's input subsystem, allowing for 3 different speeds and a stop mode. The input subsystem also allows for a high degree of interchangability of input sources without changing the application code. On the the EW this is shown by using a potentiometer on one board and a series of buttons on another board.

This also demonstrates another strength of Zephyr, the same application can, without code changes, run on multiple different board and hardware configurations. In the case of this demo, it currently supports the following boards:
- RPI Pico
- Nucleo F303RE

## Required Hardware

- Nucleo F303RE OR RPI Pico board
- For the RPI Pico: a Raspberry Pi Debug Probe
- [Stepper 19 Click](https://www.mikroe.com/stepper-19-click) shield as Stepper Motor Controller
- Linear Potentiometer, e.g. [Grove Rotary Angle Sensor](https://wiki.seeedstudio.com/Grove-Rotary_Angle_Sensor/)
- Stepper Motor with 1.8°/Step i.e. 200 Steps per Rotation and 30V/2.5A maximum power requirements

## Hardware Setup

### RPI Pico

1. Connect Pico and Debug Probe like [this](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html).
2. Connect potentiometer signal pin to GPIO pin 26 and power to 3.3V and Ground.
3. For the Click 19 connect as follows:
    - STEP pin - GPIO 19
    - SCL pin - GPIO 7
    - SDA pin - GPIO 6
    - DIR pin - GPIO 20
    - RST pin - GPIO 21
    - Use 3.3V power option
    - Stepper motor power to the right terminal block
    - Stepper motor to the left terminal block (ensure that A1/A2/B1/B2 match)

### Nucleo F303RE

1. Connect board to PC via USB
2. Connect potentiometer signal pin to A0 and power to 3.3V (can be found on both CN6 and CN7 connectors, right next to each other) and Ground
3. For the Click 19 connect as follows:
    - STEP pin - A1
    - SCL pin - SCL/D15
    - SDA pin - SDA/D14
    - DIR pin - A2
    - RST pin - A3
    - Use 3.3V power option
    - Stepper motor power to the right terminal block
    - Stepper motor to the left terminal block (ensure that A1/A2/B1/B2 match)

## Project Setup

1. Create a workspace directory
2. Open terminal in workspace directory
3. Install Dependencies
    - If you have an Ubuntu older than 22.04 add kitware repository: `wget https://apt.kitware.com/kitware-archive.sh` then `sudo bash kitware-archive.sh`
    - Use Apt to install dependencies: 
    ```
    sudo apt install --no-install-recommends git cmake ninja-build gperf \
      ccache dfu-util device-tree-compiler wget \
      python3-dev python3-pip python3-setuptools python3-tk python3-wheel xz-utils file \
      make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
    ```
    - Verify minimum dependency versions
    ```
    cmake --version     --> 3.20.5
    python3 --version   --> 3.10
    dtc --version       --> 1.4.6
    ```
4. Install Zephyr and get the Demo
    - Install Python `venv` package: `sudo apt install python3-venv`
    - Create virtual Python environment: `python3 -m venv --clear --copies --prompt=".venv" .env`
    - Source virtual environment (needs to be repeated every time you start working): `source .env/bin/activate`
    - Install West: `pip3 install west`
    - Clone repository: `west init -m https://github.com/tiacsys/ew-2025-demo` then `west update`
    - Export CMake package: `west zephyr-export`
    - Install Python dependencies: `west packages pip --install`
    - Install Zephyr SDK: `west sdk install`

## Build and Flash Project

In the terminal from the previous step:
1. Build Project
    - Nucleo F303RE: `west build -p -b nucleo_f303re ew-2025-demo`
    - RPI Pico: `west build -p -b rpi_pico ew-2025-demo`
2. Flash project: `west flash`
3. Rotate potentiometer and observe the motor



