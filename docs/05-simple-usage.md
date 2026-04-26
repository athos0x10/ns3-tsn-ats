# Simulation Automation Pipeline

This document outlines the perspective architecture of my automated simulation framework. The goal of this pipeline is to separate the simulation configuration from the core C++ engine, allowing for scalable, reproducible, and user-friendly testing campaigns without requiring C++ recompilation for every change.

## Architecture Overview

```mermaid
flowchart TD
    subgraph "1. User Input (Human Readable)"
        CFG[\"Configuration File<br>(e.g., config.yaml or .json)<br>- Delays<br>- Traffic Types<br>- Ethernet vs TSN"/]
    end

    subgraph "2. Orchestration (Python)"
        PY_RUN("Orchestration Script<br>(run_campaign.py)")
        CFG --> PY_RUN
        note1["Checks errors<br>Prepares directories<br>Generates CLI arguments"] -.-> PY_RUN
    end

    subgraph "3. Simulation Engine (C++ / ns-3)"
        NS3{{"ns-3 Binary<br>(aerospace-redundant)"}}
        PY_RUN -- "Executes ns-3 with parameters" --> NS3
        NS3 -- "Simulates" --> NS3
    end

    subgraph "4. Outputs (Raw Data)"
        TRC[("Trace Files<br>(.tr, .pcap)")]
        CSV[("Aggregated Results<br>(metrics.csv)")]
        NS3 --> TRC
        NS3 --> CSV
    end

    subgraph "5. Post-Processing & Data Viz (Python)"
        PY_PLOT("Analysis Script<br>(plot_results.py)")
        TRC --> PY_PLOT
        CSV --> PY_PLOT
        PY_PLOT --> GRAPH["Graphs<br>(Jitter, Max Latency, etc.)"]
    end
    
    style CFG fill:#fff4dd,stroke:#333,stroke-width:2px
    style PY_RUN fill:#d4e1f5,stroke:#333
    style NS3 fill:#f96,stroke:#333,stroke-width:3px
    style PY_PLOT fill:#d4e1f5,stroke:#333
    style GRAPH fill:#e2f0cb,stroke:#333
```

## Pipeline Workflow

### 1. User Input (`config.json` / `config.yaml`)
A human-readable text file used to define the parameters of the simulation. This allows non-experts to run complex network scenarios by simply adjusting key-value pairs (e.g., switching between `Ethernet` and `TSN`, modifying link delays, or defining traffic criticality).

### 2. Orchestration (`run_campaign.py`)
A Python script acts as the campaign manager. It parses the configuration file, performs sanity checks on the inputs, creates isolated output directories for each run, and translates the settings into Command Line Interface (CLI) arguments to be passed to the ns-3 engine.

### 3. Simulation Engine (`aerospace-redundant.cc`)
The core ns-3 C++ executable. This component is completely agnostic and contains no hardcoded values. It receives its context via CLI arguments, executes the discrete-event simulation, and routes traffic through the defined multi-hop topology.

### 4. Outputs (Raw Data)
The simulation generates raw data files without performing heavy statistical calculations during runtime. Outputs typically include network traces (`.pcap` for packet inspection, `.tr` for ns-3 ASCII traces) and basic metric logs (`.csv`).

### 5. Post-Processing & Data Visualization (`plot_results.py`)
A final Python script processes the raw outputs. Using data science libraries (like `pandas` and `matplotlib`), it calculates complex metrics (e.g., worst-case latency, jitter distribution) and generates graphical plots to evaluate the performance of standard Ethernet versus TSN scheduling.

## Key Benefits
* **Scalability:** Easily run batches of simulations (e.g., 50 variations of cross-traffic delays) unattended.
* **Reproducibility:** Every simulation run is tied to a specific configuration file, making it easy to replicate results for scientific publications.
* **Accessibility:** Team members can evaluate the network architecture without needing to understand ns-3 C++ internals.
