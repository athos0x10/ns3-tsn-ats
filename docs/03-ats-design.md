# Software architecture of the Scheduler

The main goal of this file is to use what we learned in the [IEEE802.1Qcr amendment](https://ieeexplore.ieee.org/document/9253013) and summarized in the *02-ats-amendments.md* document which is in the same folder as this one.

We discuss how the ATS is implemented differently depending on the node's role: **Bridges** (Ingress Shaping via `Receive`) and **End-Stations** (Egress Shaping via `SendFrom`).

## Table of contents

- [Software architecture of the Scheduler](#software-architecture-of-the-scheduler)
  - [Table of contents](#table-of-contents)
  - [ATS for Bridges](#ats-for-bridges)
    - [Group and Instance Identification](#group-and-instance-identification)
    - [Frame Processing \& Lifecycle Diagram for bridges](#frame-processing--lifecycle-diagram-for-bridges)
    - [Unified Bridge API Functions](#unified-bridge-api-functions)
  - [ATS for End-Stations](#ats-for-end-stations)
    - [Group and Instance Identification](#group-and-instance-identification-1)
    - [Frame Processing \& Lifecycle Diagram for End-Stations](#frame-processing--lifecycle-diagram-for-end-stations)
  - [Unified End-Station API Functions](#unified-end-station-api-functions)


## ATS for Bridges

When a frame arrives at the ingress port of a bridge, it is processed sequentially before being routed to an output queue. The boolean variable `m_atsEnabled` controls this behavior.

### Group and Instance Identification

1. **The ATS Group (`AtsSchedulerGroup`)**: It isolates traffic based on its physical routing path through the switch and its class of service. A group is uniquely identified by the following triplet:

$$Group\_ID = (Input\_Port, Output\_Port, Output\_Queue)$$


2. **The ATS Instance (Individual Shaper)**: Inside a single group, traffic is divided into token bucket sub-instances. By default, each distinct stream maps to its own instance based on its Layer 2 flow profile:

$$Instance\_ID = (DestMac, VlanId)$$



> **Dynamic Allocation & Aggregation**: If no configuration is pre-provisioned, the ATS subsystem automatically instantiates groups and instances on the fly using defaults (`CBS = 16384 bytes`, `CIR = 10 Mbps`, and `MaxResidenceTime = 1s`). During simulation setup, you can pre-create a bridge group and call `BindStreamToInstance` to aggregate multiple streams (e.g., different VLANs tracking the same path) onto a shared bandwidth envelope.

### Frame Processing & Lifecycle Diagram for bridges

The following Mermaid diagram traces the precise execution flow of a forwarded frame inside the Bridge ATS architecture, spanning from ingress arrival down to execution scheduling.

```mermaid
graph TD
    %% Nodes
    A[Frame Arrives at Bridge Ingress Port] --> B[Ats::EnqueueFrame]
    
    subgraph AtsEngine [Ats Subsystem]
        B --> C[Set internalId = priority]
        C --> D[Ats::GetGroup inputPortId, outputPortId, internalId]
        D -->|Not Found| E[Dynamically Instantiate AtsSchedulerGroup<br>Set Default MaxResidenceTime]
        D -->|Found| F[Retrieve Existing AtsSchedulerGroup]
        E --> G[targetGroup->ProcessFrame]
        F --> G
    end

    subgraph GroupEngine [AtsSchedulerGroup Context]
        G --> H[targetGroup->GetInstanceForStream vlanId, destMac]
        H -->|Not Found| I[Dynamically Create Token Bucket Instance<br>Set Default CIR / CBS]
        H -->|Found| J[Retrieve Shared / Explicit Token Bucket Instance]
        I --> K[Compute SchedulerEligibilityTime based on CIR/CBS]
        J --> K
        
        K --> L{Is Residence Delay > MaxResidenceTime?}
        L -->|Yes| M[DROP FRAME]
        L -->|No| N{Is EligibilityTime <= CurrentTime?}
    end

    subgraph Transmission [Egress Queuing & Execution]
        N -->|Yes| O[Mark Immediately Eligible<br>Inject Directly into Priority Output Queue]
        N -->|No| P[Schedule Future Transmission Event<br>Insert into Calendar Queue]
    end

    %% Styles
    classDef default fill:#f9f9f9,stroke:#333,stroke-width:1px;
    classDef action fill:#e1f5fe,stroke:#0288d1,stroke-width:1px;
    classDef decision fill:#fff9c4,stroke:#fbc02d,stroke-width:1px;
    classDef drop fill:#ffebee,stroke:#c62828,stroke-width:1px;
    
    class B,D,G,H action;
    class L,N decision;
    class M drop;

```

### Unified Bridge API Functions

To safely interact with this architecture without manipulating raw map indices or magic hash tokens, use the following core methods:

* **Group Retrieval**: `Ptr<AtsSchedulerGroup> Ats::GetGroupForBridge(Ptr<TsnNetDevice> ingressDevice, Ptr<TsnNetDevice> egressDevice, uint8_t priority);`
*Extracts interface indices directly from device pointers to prevent indexing mismatches.*
* **Instance Provisioning**: `uint32_t CreateAtsInstance(DataRate cir, uint32_t cbs);`
*Allocates a customized shaping rate inside a specific group.*
* **Stream Aggregation**: `bool BindStreamToInstance(StreamKey streamKey, uint32_t instanceId);`
*Binds a `{VlanId, DestMac}` profile to an existing token bucket instance to enforce shared shaping.*

## ATS for End-Stations

For an End-Station, we do not want to introduce latency upon reception. Instead, we shape the traffic **at the emission source** right before transmission, controlled by the boolean `m_atsEnabled`.

### Group and Instance Identification

Unlike bridges which group traffic by input-output port pairs, an End-Station maps shaping mechanisms directly to network stream profiles. By default, the mapping adheres to a strict one-to-one relationship:

$$\text{One Stream} = \text{One Group} = \text{One Instance}$$

Thus, for each unique **StreamKey** $(DestMac, VlanId)$, a unique dedicated scheduler group is allocated, housing a single corresponding token bucket instance inside it.

### Frame Processing & Lifecycle Diagram for End-Stations

The main architectural shift here is how the `internalId` is derived. Because there is no physical ingress port (`inputPortId` defaults to `Ats::LOCAL_INPUT_PORT`), the unique internal index is computed by shifting the `VlanId` and XORing it with a 32-bit hash generated from the lower bytes of the destination MAC address.

```mermaid
graph TD
    %% Nodes
    A[Local Application Generates Frame] --> B[Ats::EnqueueFrame]
    
    subgraph AtsEngine [Ats Subsystem]
        B --> C[Extract VlanId & DestMac from Frame En-tete]
        C --> D["Compute internalId:<br>(VlanId << 16) ^ MacHash"]
        D --> E[Ats::GetGroup LOCAL_INPUT_PORT, egressPortId, internalId]
        E -->|Not Found| F[Dynamically Instantiate AtsSchedulerGroup<br>Set Default MaxResidenceTime]
        E -->|Found| G[Retrieve Existing AtsSchedulerGroup]
        F --> H[targetGroup->ProcessFrame]
        G --> H
    end

    subgraph GroupEngine [AtsSchedulerGroup Context]
        H --> I[targetGroup->GetInstanceForStream vlanId, destMac]
        I -->|Not Found| J[Dynamically Create Token Bucket Instance<br>Set Default CIR / CBS]
        I -->|Found| K[Retrieve Dedicated Token Bucket Instance]
        J --> L[Compute SchedulerEligibilityTime based on CIR/CBS]
        K --> L
        
        L --> M{Is Residence Delay > MaxResidenceTime?}
        M -->|Yes| N[DROP FRAME]
        M -->|No| O{Is EligibilityTime <= CurrentTime?}
    end

    subgraph Transmission [Egress Queuing & Execution]
        O -->|Yes| P[Mark Immediately Eligible<br>Inject Directly into Priority Output Queue]
        O -->|No| Q[Schedule Future Transmission Event<br>Insert into Calendar Queue]
    end

    %% Styles
    classDef default fill:#f9f9f9,stroke:#333,stroke-width:1px;
    classDef action fill:#e1f5fe,stroke:#0288d1,stroke-width:1px;
    classDef decision fill:#fff9c4,stroke:#fbc02d,stroke-width:1px;
    classDef drop fill:#ffebee,stroke:#c62828,stroke-width:1px;
    
    class B,D,E,I action;
    class M,O decision;
    class N drop;

```

## Unified End-Station API Functions

To safely interact with this architecture without manipulating raw map indices or magic hash tokens, use the following core methods:

* **Group Retrieval**: `Ptr<AtsSchedulerGroup> Ats::GetGroupForEndStation(Mac48Address destMac, uint16_t vlanId, Ptr<TsnNetDevice> egressDevice);`
*Abstracts the internal `LOCAL_INPUT_PORT` parameter, automatically computes the MAC/VLAN XOR hash identity, and queries the standard ns-3 interface indices safely.*
* **Instance Provisioning**: `uint32_t CreateAtsInstance(DataRate cir, uint32_t cbs);`
*Allocates a customized shaping rate inside the specific stream group.*