# TSN Test Topologies for Evaluation

This document outlines the network topologies I used to evaluate my Time-Sensitive Networking (TSN) implementations, specifically focusing on transport industry.

---

## Topology 1: Aerospace High-Reliability Redundant Network
**Source:** [IEEE 802.1 TSN Aerospace Webinar (Jabbar, 09/2024)](https://www.ieee802.org/1/files/public/docs2024/webinar-Jabbar-TSN-Aerospace-0924.pdf)

### Description
This topology is modeled after aerospace onboard Ethernet communications requirements, where zero-packet-loss and extreme fault tolerance are mandatory. It implements a **Dual Link / Redundant Transmission** setup. 
* A Talker (source node) is connected to a Listener (destination node) via two completely disjoint network paths (Path A and Path B).
* It utilizes mechanisms akin to **IEEE 802.1CB** (Frame Replication and Elimination for Reliability - FRER). 
* The Talker replicates critical time-sensitive frames and sends them simultaneously over both paths. The Listener (or the final relay switch) identifies duplicates via sequence numbers and eliminates them.

### Purpose and Interest
* **Resilience Testing:** Allows us to observe network behavior and simulate link failures (e.g., bringing down a switch on Path A) to ensure that the stream on Path B continues uninterrupted with zero switchover time.
* **Redundancy Evaluation:** Proves that the implementation can handle hardware faults seamlessly, which is a core requirement for mission-critical TSN deployments.

```mermaid
graph TD
    subgraph "Système de Vol Redondant (Basé sur Zhao et al.)"
        S1[Capteur Vitesse] --- SW1((Switch TSN A))
        S1 --- SW2((Switch TSN B))
        
        S2[Capteur Altitude] --- SW1
        S2 --- SW2
        
        SW1 === FCC[Calculateur de Vol FCC]
        SW2 === FCC
    end

    style SW1 fill:#f96,stroke:#333
    style SW2 fill:#f96,stroke:#333
    style FCC fill:#69f,stroke:#333,stroke-width:4px
```

```mermaid
graph LR
    subgraph "Calcul Central (Brain)"
        CC[Central Compute Unit]
    end

    subgraph "Zone Avant (High Speed Backbone)"
        SW_C((Switch Central TSN))
    end

    subgraph "Zone Gateway - Avant Gauche"
        GW_FL[Zonal Gateway FL]
        Cam[Caméra ADAS - Flux Constant]
        Ctrl[Capteur Direction - Flux Critique]
    end

    subgraph "Zone Gateway - Avant Droit"
        GW_FR[Zonal Gateway FR]
        Radio[Infotainment - Best Effort]
        Lidar[Lidar - Rafales de données]
    end

    %% Connexions
    Cam --- GW_FL
    Ctrl --- GW_FL
    Radio --- GW_FR
    Lidar --- GW_FR
    
    GW_FL === SW_C
    GW_FR === SW_C
    SW_C === CC

    style SW_C fill:#f96,stroke:#333
    style CC fill:#69f,stroke:#333,stroke-width:4px
    style Ctrl fill:#f66,stroke:#333
``` 