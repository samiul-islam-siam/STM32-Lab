# STM32 Development Guide

A step-by-step guide for setting up STM32 projects using both **HAL (Hardware Abstraction Layer)** and **Bare-Metal (No HAL)** programming approaches with the **NUCLEO-F446RE** development board.

## 📋 Table of Contents

- [Prerequisites](#prerequisites)
- [HAL Programming Workflow](#hal-programming-workflow)
- [Bare-Metal Programming Workflow](#bare-metal-programming-workflow)
- [Project Workspace Location](#project-workspace-location)

## 🛠️ Prerequisites

Make sure the following tools are installed before getting started (version 2.1.0):

- [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html)
- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)
- **Hardware:** NUCLEO-F446RE development board
- **Cable:** USB Type-A to Mini-B
- **Operating System:** Windows 11

## ⚙️ HAL Programming Workflow

HAL (Hardware Abstraction Layer) programming uses STM32CubeMX to auto-generate initialization code, which is then imported into STM32CubeIDE.

### Step 1 — Generate Code with STM32CubeMX

```
STM32CubeMX
  └── From New Project
        └── Start My Project from MCU
              └── ACCESS TO MCU SELECTOR
                    └── Commercial Part Number: STM32F446RET6TR
                          └── Start Project
                                └── Initialize all peripherals with default mode? → Yes
```

### Step 2 — Configure the Project Manager

```
Project Manager
  ├── Project Name     → LAB_1B <your-project-name>
  ├── Project Location → C:\Users\USER\STM32CubeIDE\workspace_2.1.0\ <your-project-location>
  └── Toolchain / IDE  → STM32CubeIDE
        └── GENERATE CODE
```

> **Prompts during generation:**
> | Prompt | Action |
> |---|---|
> | Project Manager Settings Popup | Click **Yes** |
> | Notification | Select **Don't ask me again** |
> | Licence Agreement | Click **Agree** → **Finish** |
> | Windows Security Alert | Click **Allow** |
> | Success Dialog | Click **Close** |

### Step 3 — Import Project into STM32CubeIDE

```
STM32CubeIDE
  └── File
        └── Open Projects from File System
              └── Import Source → Select project folder → Finish
```

## 🔩 Bare-Metal Programming Workflow

Bare-metal programming gives full control over hardware without abstraction layers. Projects are created directly in STM32CubeIDE.

### Step 1

```
STM32CubeIDE
  └── File
        └── STM32 Project Create/Import
              └── Create New STM32 Project
                    └── STM32CubeIDE Empty Project
                          └── Next
                                └── MCU/MPU Selector → STM32F446RETx
                                      └── Next
                                            └── Project Name → LAB_1A <your-project-name>
                                                  └── Finish
```

### Step 2

```
1. Copy 'Drivers' folder from HAL-based project that was created. <br>
2. Paste it into current project. <br>
3. Delete the 'STM32F4xx_HAL_Driver' from 'Drivers' folder. <br>
4. In Drivers -> CMSIS -> Include and Drivers -> CMSIS -> Device -> ST -> STM32F4xx -> Include: Add/remove include path... -> OK

```

## 📁 Project Workspace Location

By default, STM32CubeIDE projects are saved at:

```
C:\Users\USER\STM32CubeIDE\workspace_2.1.0\
```

> 💡 You can change this location during the IDE's initial workspace setup or via **File → Switch Workspace**.

## 📌 Notes

- Replace `<your-project-name>` and `<your-project-location>` with your actual project name and desired save path.
- The **HAL workflow** is recommended for beginners as it auto-generates peripheral configuration code.
- The **Bare-Metal workflow** is ideal for advanced users who want full hardware control.

## 📄 License

This project is open-source. Feel free to use and modify it as needed.
