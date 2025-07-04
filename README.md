# Operating-System
# AirControlX - Automated Air Traffic Control System 🛫

AirControlX is a multithreaded, simulation-based Air Traffic Control System built in C/C++. It models real-world ATC operations such as runway allocation, flight scheduling, emergency handling, and real-time monitoring, using Operating System concepts like threading, synchronization, process management, scheduling, and IPC.

## Table of Contents

•	Project Overview
•	Core Concepts Used
•	Features
•	How to Run
•	Sample Output(CLI)
•	Acknowledgements



###  Project Overview
AirControlX simulates an intelligent and automated airspace management system. The core system comprises multiple flight threads requesting landings or takeoffs, a central ATC controller assigning runways, and subsystems managing events like emergencies and violations.

### Core Concepts Used
- Multithreading (`pthread`)
- Mutexes and semaphores
- Priority scheduling
- Process management and IPC
- Event-driven simulation
- CLI-based interaction

### Key Features
1)	Flight Creation & Entry: Batch mode & real-time input
2)	Runway Management: Separate queues for arrivals and departures with priority handling
3)	Violations & Faults: Speed checks, brake failures, emergency landings
4)	Dynamic Scheduling: SJF/SRTF/RR-based flight handling
5)	Subsystems Integration:
    a)	ATC Controller
    b)	Airline Portal
    c)	AVN Coordination
    d)	Simulated Payment System
6)	Real-Time Monitoring: Live status updates and queue logs

### How to Run

#### Prerequisites
- GCC Compiler
- Linux (or WSL)
- Make

####  Build
bash
make all

#### Run simulation
./aircontrolx

### Sample Output(CLI)

![image](https://github.com/user-attachments/assets/81d538b2-95b9-45dc-9cc3-fed26defac47)

##  Acknowledgements

•	Operating Systems Faculty – For guidance on threading, synchronization, and IPC  
•	POSIX Threads (pthreads) – For enabling multithreaded simulation  
•	GNU GCC Compiler – For building and testing the system  
•	Linux Terminal Utilities – For process and signal handling  
•	C Programming Community – For invaluable resources and best practices  
•	GitHub – For version control and collaborative development  
•	OpenAI (ChatGPT) – For technical assistance and debugging support  

