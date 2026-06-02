# Software architecture of the Scheduler

The main goal of this file is to use what we learned in the [IEEE802.1Qcr amendment](https://ieeexplore.ieee.org/document/9253013) and summarized in the *02-ats-amendments.md* document which is in the same folder as this one. 

First we will describe the whole pipeline that a frame will go in with the ATS architecture configuration. Then we will describe in details each process mainly with UML diagram.

## Table of contents

- [Software architecture of the Scheduler](#software-architecture-of-the-scheduler)
  - [Table of contents](#table-of-contents)
  - [ATS Pipeline](#ats-pipeline)
  - [What to add in the ingress pipeline](#what-to-add-in-the-ingress-pipeline)
    - [Flow meters attributes](#flow-meters-attributes)
  - [UML class](#uml-class)

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
## What to add in the ingress pipeline

Most of the work is already done, so it just need to retrieve everything we need but the main work will be to add the IPV feature which replace the priority in the frame. Of course, we need a boolean like **IpvEnabled** that we will be able to set as an attribute of the gate. Also we can improve the debug process with all the attributes that the amendment talks about (flow meters).

As we are working with **NS3**  we can reshape a bit the **SetGateAndIPV** function. Because we are working with a discrete events simulator, we might think at how the user can close or change the IPV of a gate within core functionalities. Then it would be better to maybe replace the delay period by a simple thing. We can still choose whether the gate is open or not and also the *IPV* value, but we replace the delay by the moment we want to change the state of a gate. 

```cpp
void SetGateAndIPV(StreamGateState state, uint8_t IPV, StartTime)
```

### Flow meters attributes

Most of the flow meters are already here but maybe not everything is at the right place, just to summarize :

 - Flow meters id -> done 
 - CIR (bits/s) -> done
 - CBS (bytes) -> done 
 - EIR (bits/s) -> done 
 - EBS (bytes) -> done 
 - CF (boolean) -> done
 - Color mode (boolean) -> done
 - DropOnYellow (boolean) -> done
 - MarkAllFramesRedEnable (boolean) -> done
 - MarkAllFramesRed (boolean) -> done

 ## UML class 
```mermaid
 classDiagram
    class TsnNetDevice {
        - Ptr~PsfpStreamFilterTable~ m_filterTable
        - Ptr~AtsScheduler~ m_atsScheduler
        + uint32_t MaxStreamFilterInstances$
        + uint32_t MaxStreamGateInstances$
        + uint32_t MaxFlowMeterInstances$
        + uint32_t SupportedListMax$
        + Receive(Ptr~Packet~ packet) void
    }

    class StreamGateInstance {
        - uint32_t m_flowMeterIdentifier
        - bool m_ipvEnabled
        - uint8_t m_ipvId
        + SetGateAndIPV(State, ipv, ipvEnabled, startTime) void
        + GetIpvId() uint8_t
    }

    class PsfpFlowMeterInstance {
        - uint32_t m_flowMeterIdentifier
        - uint64_t m_cir
        - uint32_t m_cbs
        - TracedValue~uint64_t~ m_redFramesCount
        + ExecuteTokenBucket(Ptr~Packet~ packet) FlowColor
    }

    class AtsScheduler {
        - map~uint16_t, Ptr~AtsSchedulerInstance~~ m_instances
        - map~uint8_t, Ptr~AtsSchedulerGroup~~ m_groups
        + uint32_t MaxSchedulerInstances$
        + uint32_t MaxSchedulerGroupInstances$
        + ProcessFrame(Ptr~Packet~ packet, uint16_t streamHandle, uint8_t ipv) void
    }

    class AtsSchedulerInstance {
        - uint64_t m_cir
        - uint32_t m_cbs
        - Time m_bucketEmptyTime
        - Time m_emptyToFullDuration
        + CalculateSchedulerEligibility(uint32_t size) Time
    }

    class AtsSchedulerGroup {
        - uint8_t m_ipv
        - Time m_groupEligibilityTime
        - Time m_maxResidenceTime
        - queue~Ptr~Packet~~ m_groupQueue
        + Enqueue(Ptr~Packet~ packet, Time schedEligTime) void
        + PeekTopPacket() Ptr~Packet~
        + DequeueTopPacket() Ptr~Packet~
    }

    class AtsEligibilityTimeTag {
        - Time m_eligibilityTime
        + SetEligibilityTime(Time t) void
        + GetEligibilityTime() Time
    }

    %% Relations de composition et d'utilisation
    TsnNetDevice "1" *-- "1" AtsScheduler : contient & orchestre
    TsnNetDevice ..> StreamGateInstance : utilise au Receive
    StreamGateInstance "1" --> "1" PsfpFlowMeterInstance : pointe vers l'ID

    %% Contraintes de cardinalité basées sur la Clause 12.31.1
    AtsScheduler "1" *-- "0..MaxSchedulerInstances" AtsSchedulerInstance : gère par streamHandle
    AtsScheduler "1" *-- "0..MaxSchedulerGroupInstances" AtsSchedulerGroup : possède par IPV
    
    AtsSchedulerGroup ..> AtsEligibilityTimeTag : tatoue les paquets stockés
```
