# Software architecture of the Scheduler

The main goal of this file is to use what we learned in the [IEEE802.1Qcr amendment](https://ieeexplore.ieee.org/document/9253013) and summarized in the *02-ats-amendments.md* document which is in the same folder as this one. 

First we will describe the whole pipeline that a frame will go in with the ATS architecture configuration. Then we will describe in details each process mainly with UML diagram.

## Table of contents

- [Software architecture of the Scheduler](#software-architecture-of-the-scheduler)
  - [Table of contents](#table-of-contents)
  - [ATS Pipeline](#ats-pipeline)

## ATS Pipeline

Below is the conceptual pipeline a frame goes through upon arrival at the bridge.

```mermaid
graph TD
    %% Nodes Definition
    Ingress[Ingress Traffic<br><i>State: Untrusted</i>]
    
    subgraph ReceivePipeline[Ingress Pipeline: Receive Function]
        Filter[Stream & Max SDU Filters]
        Gate[Stream Gate & IPV Mapping]
        Meter[Flow Meter<br><i>MEF 10.3 Token Buckets</i>]
    end
    
    subgraph AtsCore[Core Processing: ATS Scheduler]
        subgraph Group1[ATS Scheduler Group: Priority X]
            Inst1[ATS Instance: Stream A]
            Inst2[ATS Instance: Stream B]
        end
        subgraph Group2[ATS Scheduler Group: Priority Y]
            Inst3[ATS Instance: Stream C]
        end
    end

    subgraph EgressPipeline[Egress Pipeline: Storage & Selection]
        Fifo1[Group FIFO Queue X<br><i>Ordered by Eligibility Time</i>]
        Fifo2[Group FIFO Queue Y<br><i>Ordered by Eligibility Time</i>]
        TxSelect{Transmission Selection<br><i>Strict Priority Scan</i>}
        Cable([Physical Medium / Cable])
    end

    %% Flow Connections
    Ingress -->|1. Unshaped Frame| Filter
    Filter -->|2. Valid Size| Gate
    Gate -->|3. Assign Internal Priority IPV / PCP| Meter
    
    Meter -->|4. Passed Metering<br>Frame Verified| Inst1
    Meter -.->|Alternative Route| Inst2
    Meter -.->|Alternative Route| Inst3
    
    Inst1 -->|5. Compute schedulerEligibilityTime| Fifo1
    Inst2 -->|5. Compute schedulerEligibilityTime| Fifo1
    Inst3 -->|5. Compute schedulerEligibilityTime| Fifo2
    
    Fifo1 -->|6. Peek Top Frame| TxSelect
    Fifo2 -->|6. Peek Top Frame| TxSelect
    
    TxSelect -->|7. If Now >= eligibilityTime<br>Strict Priority Order| Cable

    %% Style / Color customization (Optional but nice in Markdown)
    style Ingress fill:#f9f,stroke:#333,stroke-width:2px
    style Cable fill:#9f9,stroke:#333,stroke-width:2px
    style ReceivePipeline fill:#f5f5f5,stroke:#999,stroke-dasharray: 5 5
    style AtsCore fill:#e1f5fe,stroke:#0288d1,stroke-width:2px
    style EgressPipeline fill:#fff3e0,stroke:#f57c00,stroke-width:2px
```
