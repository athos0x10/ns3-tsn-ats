# IEEE 802.1Qcr Asynchronous Traffic Shaping Implementation in NS-3

## Project Overview

This repository contains the development and evaluation of an **Asynchronous Traffic Shaping (ATS)** scheduler, as defined by the **IEEE 802.1Qcr** standard, implemented within the **NS-3** network simulator.

This project was part of my internship in the **Electronic Systems group at Eindhoven University of Technology (TU/e)**.

## Key Implementation Objectives

* Implement the Asynchronous Traffic Shaping (ATS) logic.
* Develop a new *QueueDisc subclass* within the traffic-control module in ns3.
* Evaluate end-to-end latency and jitter compared to non-shaping environments (Best Effort interference) and others TSN shapers.
  
## Repository Structure

* */src*: C++ source code for the ATS scheduler.
* */scratch*: Simulation scenarios and test scripts.
* */results*: Output data, PCAP captures, and analysis plots.
* */docs*: Technical notes, references to the 802.1Qcr standard, and the final internship report.

## Getting Started

### 1. Installation
Clone this repository into the `contrib` directory of your NS-3 installation:
```bash
cd ns-3-dev/contrib
git clone https://github.com/your-username/ns3-tsn-ats.git 
```

### 2. Configure and Build NS-3

From the `ns-3-dev` root directory, run the following commands to configure the project with debug symbols and build the modules:
```bash
# Configure the project
./ns3 configure --build-profile=debug --enable-examples --enable-tests

# Build the project (this will include our tsn-ats module)
./ns3 build
```

### 3. Run a Sample Simulation

To verify the implementation, you can run the provided test scenario located in the `scratch` folder:
```bash
./ns3 run scratch/ats-test-scenario
```