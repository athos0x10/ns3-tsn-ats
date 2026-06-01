# 02-ats-amendments

The main goal of this file is to summarize the **IEEE 802.1Qcr** 2020 amendments brick by brick is order to understand each aspects of it.

# Table of Contents

- [02-ats-amendments](#02-ats-amendments)
- [Table of Contents](#table-of-contents)
- [1- Per-Stream Filtering and Policing (PSFP) \& Stream Gating](#1--per-stream-filtering-and-policing-psfp--stream-gating)
  - [1.1- Stream Filter (8.6.5.3)](#11--stream-filter-8653)
  - [1.2- Maximum SDU Size Filtering (Section 8.6.5.3.1)](#12--maximum-sdu-size-filtering-section-86531)
  - [1.3- Stream Gate (Section 8.6.5.4)](#13--stream-gate-section-8654)
    - [1.3.1- Core Architectural Parameters](#131--core-architectural-parameters)
    - [1.3.2- PSFP \& Control Operations](#132--psfp--control-operations)
    - [1.3.3- Defensive Features (Permanent Lockdown)](#133--defensive-features-permanent-lockdown)
    - [1.3.4- Statistical Counters](#134--statistical-counters)
  - [1.4- Flow Meter (Bandwidth Policing - Section 8.6.5.5)](#14--flow-meter-bandwidth-policing---section-8655)
    - [1.4.1- Core Architectural Parameters (MEF 10.3)](#141--core-architectural-parameters-mef-103)
    - [1.4.2- Policing Verdict \& Frame Marking](#142--policing-verdict--frame-marking)
    - [1.4.3- Defensive Features](#143--defensive-features)
    - [1.4.4- Statistical Counters](#144--statistical-counters)
  - [2.1- ATS Eligibility Time Assignment (Section 8.6.5.6)](#21--ats-eligibility-time-assignment-section-8656)
    - [2.1.1- ATS Scheduler Components (Individual / Per-Stream)](#211--ats-scheduler-components-individual--per-stream)
    - [2.1.2- ATS Scheduler Group Components (Per Port/Traffic Class)](#212--ats-scheduler-group-components-per-porttraffic-class)
    - [2.1.3- Global Variables and Tables](#213--global-variables-and-tables)
  - [Global Processing Pipeline and Project Status](#global-processing-pipeline-and-project-status)

# 1- Per-Stream Filtering and Policing (PSFP) & Stream Gating

In an Asynchronous Traffic Shaping architecture, devices operate without a globally synchronized clock. Because we cannot rely on time-triggered coordination (and on users), we must actively monitor incoming traffic.

In this section, we will go through all these policies.

## 1.1- Stream Filter (8.6.5.3)

The system analyzes the packet headers (such as Source/Destination MAC addresses, VLAN ID, and Priority) to map the packet to a specific, recognized stream handle.

It is done in the *eden-sim/contrib/tsn/model/psfp-stream-filter-instance.cc* file, in the following function:
```C
bool
StreamFilterInstance::Match(uint16_t streamHandle, uint8_t priority)
```

If it does match with the stream then we increment the counter **MatchingFramesCount**. 

## 1.2- Maximum SDU Size Filtering (Section 8.6.5.3.1)

Unlike the Time-Aware Shaper (TAS), ATS does not allocate dedicated time slots, making frame preemption complex. To guarantee low-latency bounds for high-priority traffic, large rogue packets must be blocked. The system checks if the frame size is below the maximum Service Data Unit size (*MaxSDUSize*) configured for this stream. If it exceeds this threshold, the packet is immediately dropped.

It is done in the *eden-sim/contrib/tsn/model/psfp-stream-filter-instance.cc* file, in the following function:
```C
bool
StreamFilterInstance::MaxSDUSizeFilter(Ptr<Packet> packet)
```

If the packet pass then we increment the counter **PassingSDUCount** and if not we increment **NotPassingSDUCount**

## 1.3- Stream Gate (Section 8.6.5.4)

The **Stream Gate** is a critical checkpoint positioned directly after the stream filter. It has two main purposes:
1. **Time-Based Filtering**: Discarding frames whose arrival times violate a pre-defined time schedule.
2. **Internal Priority Assignment**: Mapping the frame's original priority to an **Internal Priority Value (IPV)** (0 to 7) used for subsequent queuing and Asynchronous Traffic Shaping (ATS) delay adjustments, while retaining the original priority for the final transmission header. If configured as `Null`, the frame keeps its original priority.

Each gate instance belongs to a *Stream Gate Instance Table* (up to `MaxStreamGateInstances`) and features the following standard components:

### 1.3.1- Core Architectural Parameters
* **Stream Gate Instance Identifier**: A unique integer indexing the gate.
* **Stream Gate State (Administrative & Operational)**: Binary state determining frame forwarding:
  * `Open`: Frames are permitted to pass through.
  * `Closed`: Frames are blocked and discarded.
* **Internal Priority Value (IPV) Specification (Administrative & Operational)**: Determines if the frame's priority is overridden with a specific IPV or kept as `Null`.

### 1.3.2- PSFP & Control Operations
When Per-Stream Filtering and Policing (PSFP) is supported, a cyclic state machine executes a **Stream Gate Control List** using the following core operation:
* **`SetGateAndIPV(StreamGateState, IPV, TimeInterval, [IntervalOctetMax])`**
  * Changes the gate state and the IPV for a specific duration (`TimeInterval`).
  * *Optional* `IntervalOctetMax`: Defines the maximum number of MSDU octets allowed to pass through the gate during this interval.

### 1.3.3- Defensive Features (Permanent Lockdown)
To safeguard network resources against anomalies or babbling transmitters, the gate can permanently override its operational state and remain **Closed** until manual administrative intervention:
* **Lockdown Due to Invalid RX (`GateClosedDueToInvalidRx`)**: Triggered when a frame arrives while the gate is `Closed`. If `GateClosedDueToInvalidRxEnable` is true, the gate locks down, permanently discarding all subsequent frames.
* **Lockdown Due to Volume Overflow (`GateClosedDueToOctetsExceeded`)**: Triggered when an incoming frame size exceeds the remaining allowed bytes (`IntervalOctetsLeft`). If `GateClosedDueToOctetsExceededEnable` is true, the gate locks down and discards all subsequent frames.

### 1.3.4- Statistical Counters
The gating outcome explicitly updates the management counters located inside the associated *Stream Filter* (8.6.5.3):
* **`PassingFrameCount`**: Incremented every time a frame successfully passes through the stream gate.
* **`NotPassingFrameCount`**: Incremented every time a frame is discarded (due to a closed state, octet limit overflow, or permanent lockdown).

## 1.4- Flow Meter (Bandwidth Policing - Section 8.6.5.5)

The **Flow Meter** enforces bandwidth limits to protect network resources from misbehaving streams. It implements a simplified version of the **MEF 10.3 Dual-Rate Three-Color Marker** algorithm (Token Bucket).

Each flow meter belongs to a *Flow Meter Instance Table* (up to `MaxFlowMeterInstances`) and includes the following standard components:

### 1.4.1- Core Architectural Parameters (MEF 10.3)
* **Flow Meter Identifier**: A unique integer indexing the instance.
* **CIR / CBS**: Committed Information Rate (bps) and Committed Burst Size (octets) for guaranteed traffic profile (Green).
* **EIR / EBS**: Excess Information Rate (bps) and Excess Burst Size (octets) for allowed burst limits (Yellow).
* **Coupling Flag (CF)**: Decides if unused committed tokens overflow into the excess bucket.
* **Color Mode (CM)**: Operates as either `color-blind` (ignores prior packet color tags) or `color-aware`.

### 1.4.2- Policing Verdict & Frame Marking
Based on the available tokens, the algorithm colors the frame. The gate manages Yellow frames using the **`DropOnYellow`** boolean:
* `DropOnYellow = TRUE`: All frames marked Yellow are **discarded** immediately.
* `DropOnYellow = FALSE`: Yellow frames are permitted to pass, but their `drop_eligible` bit is set to `TRUE` (marking them for priority drop in case of downstream congestion).

### 1.4.3- Defensive Features
To halt massive traffic violations at the root, the flow meter can permanently lock down the stream and drop everything:
* **`MarkAllFramesRedEnable`**: If `TRUE`, dropping a single frame triggers a permanent lockdown.
* **`MarkAllFramesRed`**: State variable. When it transitions to `TRUE` following a violation, **all subsequent frames are permanently discarded**, regardless of actual token levels, until manual administrative reset.

### 1.4.4- Statistical Counters
* **`RedFramesCount`**: Every time the flow meter discards a frame (due to a Red verdict, a `DropOnYellow` rule, or a permanent lockdown).

## 2.1- ATS Eligibility Time Assignment (Section 8.6.5.6)

The role of the Asynchronous Traffic Shaping (ATS) scheduler is to assign an **Eligibility Time** ($t_E$) to each frame. This calculated time is subsequently used by the ATS transmission selection algorithm (8.6.8.5) to shape and smooth traffic asynchrononously.

The standard structures this mechanism using two key entities: the **Individual Scheduler** (per-stream) and the **Scheduler Group**.

### 2.1.1- ATS Scheduler Components (Individual / Per-Stream)

Each ATS scheduler instance manages a unique data stream and comprises the following:

* **Scheduler Identifier**: A unique integer indexing the instance within the global table.
* **Scheduler Group Identifier**: The identifier of the scheduler group to which this scheduler belongs.
* **CommittedBurstSizeParameter (CBS)**: The committed burst size parameter, expressed in bits.
* **CommittedInformationRate (CIR)**: The committed information rate parameter, in bits per second.
* **Bucket Empty Time (State Variable)**: An internal state variable (in seconds) representing the theoretical execution time when the stream's virtual token bucket will be completely empty.

### 2.1.2- ATS Scheduler Group Components (Per Port/Traffic Class)

Schedulers are organized into **ATS Scheduler Groups**. There is exactly **one group per reception Port per upstream traffic class**, where the latter refers to the transmitting traffic class in the device connected to the given reception Port.
Each group comprises the following:

* **MaximumResidenceTime**: A maximum residence time parameter (in nanoseconds) shared by all ATS schedulers within a scheduler group. If a frame has to wait longer than this threshold to become eligible, it is discarded.
* **Group Eligibility Time (State Variable)**: A shared internal state variable. It stores the assigned eligibility time of the last frame processed within the scheduler group.


### 2.1.3- Global Variables and Tables

* **DiscardedFramesCount**: A per-reception-port integer counter tracking the total number of frames discarded by the ATS schedulers associated with that port (e.g., due to a *MaximumResidenceTime* violation).
* The bridge component maintains three management tables: the *ATS Scheduler Instance Table*, the *ATS Scheduler Group Instance Table*, and an *ATS Scheduler Port Parameter Table* shared by all schedulers associated with a reception Port.


## Global Processing Pipeline and Project Status

Here is the complete architecture of the TSN pipeline you are building. This diagram illustrates the exact path of a packet, highlighting **what has already been implemented** (PSFP + basic Gate components) and **what needs to be coded next** (the central ATS block).

```mermaid
graph TD
    %% Style for project advancement state
    classDef done fill:#85e3b3,stroke:#27ae60,stroke-width:2px,color:#000;
    classDef todo fill:#ffcc80,stroke:#e67e22,stroke-width:2px,color:#000;
    classDef global fill:#e0e0e0,stroke:#7f8c8d,stroke-width:1px,color:#000;

    subgraph INGRESS [1. Packet Ingress: Receive Function]
        A[Packet Arrival] --> B(1.1 Stream Filter: Stream Identification)
        B --> C{1.1.2 Max SDU Size Filter}
        C -- Oversized --> Drop1[Drop & m_notPassingSDUCount++]
        C -- Valid --> D[1.2 Stream Gate]
        D --> E{1.2.1 Gate State Check}
        E -- Closed / Lockdown --> Drop2[Drop & m_notPassingFrameCount++]
        E -- Always OPEN for ATS --> F(1.2.2 IPV Assignment)
        F --> G(1.3 Flow Meter: MEF 10.3 Dual Token Bucket)
        G --> H{Color Verdict}
        H -- Red / DropOnYellow --> Drop3[Drop & m_redFrameCount++]
        H -- Green / Valid Yellow --> I[Valid & Checked Packet]
    end

    subgraph ATS_BLOCK [2. Core Processing: ATS Scheduler]
        I --> J(2.1.1 Retrieve Per-Stream ATS Scheduler)
        J --> K(Mathematical Calculation of Eligibility Time: t_e)
        K --> L{t_e - arrival_time > MaxResidenceTime ?}
        L -- Yes Deadline Exceeded --> Drop4[Drop & DiscardedFramesCount++]
        L -- No Compliant --> M(Update BucketEmptyTime & GroupEligibilityTime)
        M --> N(Enqueue into Group FIFO Queue)
    end

    subgraph EGRESS [3. Packet Egress: Transmission Selection]
        N --> O(8.6.8.5 ATS Transmission Selection)
        O --> P{Local Clock >= t_e ?}
        P -- No --> Q[Hold in Queue]
        P -- Yes --> R(Release to Physical Port)
    end

    %% Class Assignment
    class B,C,Drop1,D,E,Drop2,F,G,H,Drop3,I done;
    class J,K,L,Drop4,M,N todo;
    class O,P,Q,R global;

