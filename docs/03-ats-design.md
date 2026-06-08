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
        subgraph GroupInput1[ATS Scheduler Group: Input Port 1]
            Inst1[ATS Instance: Stream A]
            Inst2[ATS Instance: Stream B]
            Inst3[ATS Instance: Stream C<br><i>Flexible Allocation</i>]
        end
    end

    subgraph EgressPipeline[Egress Pipeline: Storage & Selection]
        SmartQueue[Smart Calendar Queue<br><i>1. Ordered by Eligibility Time<br>2. Tie-Breaker: Priority PCP/IPV</i>]
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
    
    Inst1 -->|5. Compute schedulerEligibilityTime| SmartQueue
    Inst2 -->|5. Compute schedulerEligibilityTime| SmartQueue
    Inst3 -->|5. Compute schedulerEligibilityTime| SmartQueue
    
    SmartQueue -->|6. Peek Top Frame<br>Earliest Time + Highest Priority| TxSelect
    
    TxSelect -->|7. If Now >= eligibilityTime<br>Strict Priority Order| Cable

    %% Style / Color customization
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
    %% --- Hiérarchie des Équipements Réseau ---
    class NetDevice {
        <<interface>>
    }
    class EthernetNetDevice {
    }
    class TsnNetDevice {
        - Ptr~PsfpStreamFilterTable~ m_filterTable
        - Ptr~AtsScheduler~ m_atsScheduler
        + Receive(Ptr~Packet~ packet) void
    }
    class TsnMultidropNetDevice {
    }

    NetDevice <|-- EthernetNetDevice
    EthernetNetDevice <|-- TsnNetDevice
    TsnNetDevice <|-- TsnMultidropNetDevice

    %% --- Composant d'Horloge ---
    class Clock {
        + GetLocalTime() Time
    }

    %% --- Composants PSFP Existants ---
    class StreamGateInstance {
        - uint32_t m_flowMeterIdentifier
        - uint8_t m_ipvId
    }
    class PsfpFlowMeterInstance {
        + ExecuteTokenBucket(Ptr~Packet~ packet) FlowColor
    }

    %% --- Nouvelle Architecture ATS Centralisée (802.1Qcr) ---
    class AtsScheduler {
        - Ptr~Clock~ m_clock
        - Ptr~TsnNetDevice~ m_device
        - Time m_groupEligibilityTime
        - Time m_maximumResidenceTime
        - Ptr~AtsSchedulerInstance~ m_defaultInstance
        - map~uint32_t, Ptr~AtsSchedulerInstance~~ m_streamToInstanceMap
        - multiset~Ptr~Packet~, AtsPacketCompare~ m_calendarQueue
        + ProcessFrame(Ptr~Packet~ packet, uint32_t streamHandle, uint8_t pcp) bool
        + RegisterStreamToInstance(uint32_t streamHandle, Ptr~AtsSchedulerInstance~ instance) void
    }

    class AtsSchedulerInstance {
        - uint32_t m_schedulerIdentifier
        - uint8_t m_schedulerGroupIdentifier
        - DataRate m_committedInformationRate
        - uint32_t m_committedBurstSize
        - Time m_bucketEmptyTime
        - Time m_emptyToFullDuration
        + GetCir() DataRate
        + GetCbs() uint32_t
        + GetBucketEmptyTime() Time
        + SetBucketEmptyTime(Time t) void
    }

    class AtsEligibilityTimeTag {
        - Time m_eligibilityTime
        + SetEligibilityTime(Time t) void
        + GetEligibilityTime() Time
    }

    %% --- Relations et Dépendances ---
    TsnNetDevice "1" *-- "1" AtsScheduler : contient & orchestre
    StreamGateInstance "1" --> "1" PsfpFlowMeterInstance : pointe vers

    %% L'ATS possède et gère de façon flexible les instances de stockage
    AtsScheduler "1" *-- "1" AtsSchedulerInstance : instance par défaut (PCP/Port)
    AtsScheduler "1" *-- "0..*" AtsSchedulerInstance : instances configurées (m_streamToInstanceMap)
    
    %% Gestion des paquets
    AtsScheduler ..> AtsEligibilityTimeTag : tatoue et lit les paquets dans m_calendarQueue

    %% L'horloge est centralisée au niveau du Scheduler
    AtsScheduler "1" --> "1" Clock : interroge
```