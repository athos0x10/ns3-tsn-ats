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
