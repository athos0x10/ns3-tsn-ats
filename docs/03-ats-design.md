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
    %% --- Matériel / Port Réseau ---
    class TsnNetDevice {
        - Ptr~PsfpStreamFilterTable~ m_filterTable
        - Ptr~Tas~ m_tas
        - std::vector~Ptr~TsnTransmissionSelectionAlgo~~ m_txAlgos
        + Receive(Ptr~Packet~ packet) void
        + SendFrom(Ptr~Packet~ packet, ...) bool
        + CheckForReadyPacket() void
    }

    %% --- Composant d'Horloge ---
    class Clock {
        + GetLocalTime() Time
    }

    %% --- Composants d'Arbitrage et Portes (TAS - 802.1Qbv) ---
    class Tas {
        - std::vector~Ptr~TransmissionGate~~ m_transmissionGates
        - std::vector~GclEntry~ m_GateControlList
        - Callback~void~ GateUpdateCallback
        + IsSendable(Ptr~Packet~ p, ...) bool
        - UpdateGates(bool clockUpdate) void
    }

    %% --- Abstraction de Sélection de Transmission (TSA) ---
    class TsnTransmissionSelectionAlgo {
        <<abstract>>
        # Ptr~TsnNetDevice~ m_net
        # Ptr~Queue~Packet~~ m_queue
        # Callback~void~ ReadyToTransmitCallback
        + SetTsnNetDevice(Ptr~TsnNetDevice~ net) void
        + virtual IsReadyToTransmit() bool
        + virtual TransmitStart(Ptr~Packet~ p, Time txTime) void
    }

    %% --- Spécification ATS (802.1Qcr) sous forme de TSA ---
    class AtsTransmissionSelectionAlgo {
        - Ptr~AtsScheduler~ m_atsScheduler
        + IsReadyToTransmit() bool override
        + TransmitStart(Ptr~Packet~ p, Time txTime) void override
    }

    class AtsScheduler {
        - Ptr~Clock~ m_clock
        - Ptr~TsnNetDevice~ m_netDevice
        - Time m_groupEligibilityTime
        - Time m_maximumResidenceTime
        - Ptr~AtsSchedulerInstance~ m_defaultInstance
        - map~uint32_t, Ptr~AtsSchedulerInstance~~ m_streamHandlerToInstanceMap
        - multiset~Ptr~Packet~, AtsPacketCompare~ m_calendarQueue
        - EventId m_nextAtsTransmissionEvent
        + ProcessPacket(Ptr~Packet~ packet, uint32_t streamHandler, uint8_t pcp) bool
        + RegisterStreamToInstance(uint32_t streamHandler, uint32_t instanceId) bool
        + PeekTopPacket() Ptr~Packet~
        + DequeueTopPacket() void
        + GetNextEligibilityTime() Time
        - TriggerQueueCheck() void
    }

    class AtsSchedulerInstance {
        - uint32_t m_schedulerIdentifier
        - DataRate m_committedInformationRate
        - uint32_t m_committedBurstSize
        - Time m_bucketEmptyTime
        + GetCir() DataRate
        + GetCbs() uint32_t
        + GetBucketEmptyTime() Time
        + SetBucketEmptyTime(Time t) void
    }

    class AtsEligibilityTimeTag {
        - Time m_eligibilityTime
        - uint8_t m_pcp
        + SetEligibilityTime(Time t) void
        + GetEligibilityTime() Time
        + SetPcp(uint8_t pcp) void
        + GetPcp() uint8_t
    }

    %% --- Liaisons, Héritages et Dépendances ---
    
    %% Héritage du sélecteur de transmission
    TsnTransmissionSelectionAlgo <|-- AtsTransmissionSelectionAlgo : Spécifie / Hérite
    
    %% Composition et Agrégation au sein du Port (NetDevice)
    TsnNetDevice "1" *-- "1" Tas : Possède & interroge pour envoi
    TsnNetDevice "1" *-- "0..*" TsnTransmissionSelectionAlgo : Gère l'arbitrage des queues
    
    %% Architecture Interne ATS
    AtsTransmissionSelectionAlgo "1" --> "1" AtsScheduler : Délègue la vérification de la queue temporelle
    AtsScheduler "1" *-- "1" AtsSchedulerInstance : Instance par défaut
    AtsScheduler "1" *-- "0..*" AtsSchedulerInstance : Instances spécifiques (m_streamHandlerToInstanceMap)
    
    %% Tags et Horloge
    AtsScheduler ..> AtsEligibilityTimeTag : Inspecte et Tatoue les paquets dans m_calendarQueue
    AtsScheduler "1" --> "1" Clock : Utilise comme source de temps matériel unique

    %% Cycle de Rappel (Callbacks de réveil de simulation)
    Tas ..> TsnNetDevice : Déclenche GateUpdateCallback -> CheckForReadyPacket()
    AtsScheduler ..> TsnNetDevice : Réveille via TriggerQueueCheck() à l'échéance d'un timer
```