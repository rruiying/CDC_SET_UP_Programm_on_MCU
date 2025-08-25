# PSoC Creator MCU Projects

This repository contains several PSoC Creator projects for MCU development,  
including **sensor tests**, **motor tests**, and the **final integrated project**.  

The **final project** (`Endedition_on_mcu`) is designed to run together with the PC-side control software.

---

## Project Structure
- **2Motorlaufen/**  
  Test project for motor control.

- **DS18B20temperatur_sensor_test/**  
  Test project for the DS18B20 temperature sensor.

- **VL6180X_Distance_Test/**  
  Test project for the VL6180X distance sensor.

- **Endedition_on_mcu/** *(main application)*  
  Final integrated project combining motors, sensors, and communication.  
  This project works together with the PC-side software.

---

## Prerequisites
- **PSoC Creator 4.x** (same version recommended across collaborators)  
- **PSoC Programmer** (installed with Creator/KitProg3)  
- Proper board drivers installed (KitProg3 / Bootloader)

---

## How to Build
1. Clone this repository:
   '''bash
   git clone https://github.com/rruiying/CDC_SET_UP_Programm_on_MCU.git'''

2.Open the workspace (.cywrk) in PSoC Creator:
    For main usage: open Endedition_on_mcu/Endedition_on_mcu.cywrk
    For experiments: open the corresponding test project

3. In Workspace Explorer, right-click the project → Set as Active Project

4. Build:
    Build → Clean Project
    Build → Build Project

5. Programming the MCU
    Option A: Inside PSoC Creator
        Select the correct Target Device in the toolbar
        Ensure the board is connected
        Debug → Program (or right-click project → Program)

    Option B: Using PSoC Programmer
        Open PSoC Programmer
        Select interface (KitProg3) and device
        Load the generated .hex from
        ...\<project>.cydsn\CortexM0p\ARM_GCC_xxx\Debug\
        Click Program

6. PC-Side Software
    Option A: Using Option A: Serial terminal (e.g., PuTTY)
        Open the correct COM port in Port rate: 115200
        Type HELP to see available commands

    Option B: Using PC-Programm
        The Endedition_on_mcu project is designed to work together with the PC-side control application.
        Communication method: USB-CDC (virtual COM port)
        Features: exchange commands, read sensor data, control motors via PC interface
        Repository: CDC_Set_Up_Control_Programm
        Clone and build the PC-side application from the above repository to run the complete system.
