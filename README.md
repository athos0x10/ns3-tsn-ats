# IEEE 802.1Qcr Asynchronous Traffic Shaping Implementation in NS-3

## Project Overview

This repository contains the development and evaluation of an **Asynchronous Traffic Shaping (ATS)** scheduler, as defined by the **IEEE 802.1Qcr** standard, implemented within the **NS-3** network simulator.

This project was part of my internship in the **Electronic Systems group at Eindhoven University of Technology (TU/e)**.

## Key Implementation Objectives

* Implement the Asynchronous Traffic Shaping (ATS) logic.
* Evaluate end-to-end latency and jitter compared to non-shaping environments and others TSN shapers.

## Repository Structure

* */src*: C++ source code for the ATS scheduler.
* */scratch*: Simulation scenarios and test scripts.
* */results*: Output data, PCAP captures, and analysis plots.
* */docs*: Technical notes, references to the 802.1Qcr standard, and the final internship report.

---

## Prerequisites & Installation

> [!IMPORTANT]
> To run this project successfully, **eden-sim** and **ns-3.40** must be installed on your system.
> If you prefer a turnkey, containerized environment to avoid operating system issues, a fully guided solution to containerize the entire setup using Docker is available in ns3-setum.md.

### Using the installation script

An automation script (`install.sh`) is provided at the root of the project to deploy the code to your simulation workspace (compatible with **macOS** and **Linux**):

1. **Standard Deployment**: Copies the ATS source code directly into your `eden-sim` repository.
2. **Full Deployment**: Copies the source to `eden-sim`, then syncs the updated `contrib/` packages directly into your global `ns-3.40` installation directory.
3. **Fallback feature**: If `eden-sim` cannot be detected on your computer, the script will automatically offer to clone it straight into your `/tmp` folder to allow the installation process to proceed.