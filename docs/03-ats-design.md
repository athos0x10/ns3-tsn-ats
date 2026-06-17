# Software architecture of the Scheduler

The main goal of this file is to use what we learned in the [IEEE802.1Qcr amendment](https://ieeexplore.ieee.org/document/9253013) and summarized in the *02-ats-amendments.md* document which is in the same folder as this one.

We discuss how the ATS is implemented differently depending on the node's role: **Bridges** (Ingress Shaping via `Receive`) and **End-Stations** (Egress Shaping via `SendFrom`).

## Table of contents

* [ATS for Bridges (Ingress Shaping)](https://www.google.com/search?q=%23ats-for-bridges-ingress-shaping)
* [ATS for End-Stations (Egress Shaping & Per-Stream Isolation)](https://www.google.com/search?q=%23ats-for-end-stations-egress-shaping--per-stream-isolation)
* [ATS Internal Processing (ProcessFrame & Scheduler)](https://www.google.com/search?q=%23ats-internal-processing-processframe--scheduler)

---

## ATS for Bridges (Ingress Shaping)

When a frame arrives at the ingress port of a bridge, it is processed sequentially inside `TsnNetDevice::Receive` before being routed to an output port. The boolean `m_atsEnabledForBridge` controls this behavior.

```mermaid
graph TD
    A[Packet Arrives from Channel] --> B["TsnNetDevice::Receive() <br> (Stream ID -> PSFP -> FRER)"]
    B --> C{m_atsEnabledForBridge}
    C -- true --> D["m_atsSchedulerGroup->ProcessFrame()"]
    C -- false --> E["ForwardUp() <br> (Direct delivery to upper layer / bridge)"]
    
    style A fill:#2d3748,stroke:#4a5568,stroke-width:2px,color:#fff
    style B fill:#2d3748,stroke:#4a5568,stroke-width:2px,color:#fff
    style C fill:#4a154b,stroke:#6b21a8,stroke-width:2px,color:#fff
    style D fill:#1e3a8a,stroke:#2563eb,stroke-width:2px,color:#fff
    style E fill:#064e3b,stroke:#059669,stroke-width:2px,color:#fff

```

---

## ATS for End-Stations (Egress Shaping & Per-Stream Isolation)

For an End-Station, we do not want to introduce latency upon reception. Instead, we shape the traffic **at the emission source** inside the `TsnNetDevice::SendFrom` function, controlled by the boolean `m_atsEnabledForES`.

Furthermore, to guarantee Strict Isolation at the end-station level, the architecture enforces a **1 Stream = 1 Instance = 1 Group** mapping. Every unique application stream gets its own independent token bucket and shaping queue.

```mermaid
graph TD
    A[Application Generates Packet] --> B["TsnNetDevice::SendFrom() <br> (Extract PCP / Header Prep)"]
    B --> C{m_atsEnabledForES}
    C -- true --> D["Dynamic Mapping: <br> Stream ID -> Unique AtsSchedulerGroup"]
    C -- false --> E["Standard FIFO Queue <br> (Direct insertion into m_queues[pcp])"]
    D --> F["Per-Stream Shaper <br> m_atsStreamSchedulerGroups[streamHandle]->ProcessFrame()"]
    
    style A fill:#2d3748,stroke:#4a5568,stroke-width:2px,color:#fff
    style B fill:#2d3748,stroke:#4a5568,stroke-width:2px,color:#fff
    style C fill:#4a154b,stroke:#6b21a8,stroke-width:2px,color:#fff
    style D fill:#2d3748,stroke:#4a5568,stroke-width:2px,color:#fff
    style F fill:#1e3a8a,stroke:#2563eb,stroke-width:2px,color:#fff
    style E fill:#064e3b,stroke:#059669,stroke-width:2px,color:#fff

```

---

## ATS Internal Processing (ProcessFrame & Scheduler)

Whether triggered by a Bridge (Ingress) or an End-Station (Egress), the core execution of `ProcessFrame()` remains identical, utilizing a time-ordered `std::multiset` and a precise callback scheduling system.

### Sorting Strategy: The Custom Comparator

```cpp
struct AtsPacketInfo
{
    Ptr<Packet> packet;
    Time eligibilityTime;
    uint8_t priority;
    uint32_t streamHandle;
    Time hardwareLatency;
};

struct AtsPacketComparator
{
    bool operator()(const AtsPacketInfo a, const AtsPacketInfo b) const
    {
        // 1. Earlier eligibility times must be processed first
        if (a.eligibilityTime != b.eligibilityTime)
        {
            return a.eligibilityTime < b.eligibilityTime;
        }
        // 2. Tie-breaker: Higher priority values (PCP) come first
        return a.priority > b.priority;
    }
};

```

### ProcessFrame & Event Scheduling Pipeline

```mermaid
%%{init: {'theme': 'dark', 'themeVariables': { 'lineColor': '#6366f1' }}}%%
sequenceDiagram
    autonumber
    participant NetDevice as TsnNetDevice
    participant ATS as AtsSchedulerGroup
    participant Queue as m_atsEventQueue (multiset)
    participant Sim as ns-3 Simulator Loop

    Note over NetDevice,ATS: Execution of ProcessFrame()
    NetDevice->>ATS: ProcessFrame(packet, streamHandle)
    ATS->>ATS: Calculate Eligibility Time (Token Bucket)
    ATS->>Queue: Insert AtsPacketInfo (Auto-sorted)
    
    Note over ATS,Queue: Check if new packet is earliest event
    Queue-->>ATS: Peek front element
    
    alt New packet is the earliest event
        ATS->>Sim: Cancel previous scheduled HandleTxCallback
        ATS->>Sim: Schedule HandleTxCallback(at new eligibilityTime)
    else New packet is NOT the earliest event
        ATS->>ATS: Keep current timer running (sequential wait)
    end

    Note over Sim: ... Time elapses until Eligibility Time reached ...

    Sim->>ATS: Trigger Event: HandleTxCallback()
    ATS->>Queue: Pop front element (Earliest Packet)
    
    alt Node Role: Bridge
        ATS->>NetDevice: ForwardUp(packet)
    else Node Role: End-Station
        ATS->>NetDevice: m_queues[pcp]->Enqueue(packet)
    end

    Note over ATS,Queue: Reschedule next packet if any
    Queue-->>ATS: Is queue empty?
    alt Queue is NOT empty
        ATS->>Sim: Schedule HandleTxCallback(for new front packet)
    else Queue is empty
        ATS->>ATS: Enter Passive/Sleep Mode
    end

```