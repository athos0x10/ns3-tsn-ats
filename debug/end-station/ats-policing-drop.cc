/**
 * @file ats-policing-drop.cc
 * @author Arthur
 * @date June 22, 2026
 * @brief Demonstration of ATS (Asynchronous Traffic Shaping) policing and frame drop mechanisms.
 *
 * @details This script simulates a dense burst of 5 frames to observe how the ATS
 * engine (IEEE 802.1Qcr) handles non-compliant traffic. By enforcing a strict 1ms
 * MaxResidenceTime ceiling, late-eligible packets inside the shaper bucket are discarded
 * to guarantee deterministic delay bounds for upstream traffic.
 *
 * @section arch Component Architecture & Policing Logic
 * @code
 * +-------------------------------------------------------------------------------+
 * |                                TSN NODE (Source)                              |
 * |  [srcClock]                                                                   |
 * |                                                                               |
 * |   +------------------------+                                                  |
 * |   |   EthernetGenerator    |                                                  |
 * |   |  (Burst Flash: 5 Pkts) |                                                  |
 * |   +-----------+------------+                                                  |
 * |               | (Simultaneous Arrival at t=10ms)                              |
 * |               v                                                               |
 * |  +-------------------------------------------------------------------------+  |
 * |  |                            TsnNetDevice                                 |  |
 * |  |  [DataRate = 1Gbps]                                                     |  |
 * |  |                                                                         |  |
 * |  |  +-------------------------------------------------------------------+  |  |
 * |  |  | ATS ENGINE (isAtsEnabled = true)                                  |  |  |
 * |  |  | [MaxResidenceTime = 1ms]                                          |  |  |
 * |  |  |                                                                   |  |  |
 * |  |  | Pkt 0 -> Delay: 414.4 us  (<= 1ms) -------------------> [ALLOW]   |  |  |
 * |  |  | Pkt 1 -> Delay: 828.8 us  (<= 1ms) -------------------> [ALLOW]   |  |  |
 * |  |  | Pkt 2 -> Delay: 1243.2 us (> 1ms)  --[MaxResidence]--> [DROP!]    |  |  |
 * |  |  | Pkt 3 -> Delay: 1657.6 us (> 1ms)  --[MaxResidence]--> [DROP!]    |  |  |
 * |  |  | Pkt 4 -> Delay: 2072.0 us (> 1ms)  --[MaxResidence]--> [DROP!]    |  |  |
 * |  |  +-------------------------------------------------------------------+  |  |
 * |  |                                                                         |  |
 * |  |    +-------------------------------------------------------+            |  |
 * |  |    | Transmission Queues (8 Standard CoS Queues / DropTail)|            |  |
 * |  |    +---------------------------+---------------------------+            |  |
 * |  +--------------------------------|----------------------------------------+  |
 * +-----------------------------------|-------------------------------------------+
 *                                     |
 *                                     v [EthernetChannel]
 *                   (Only 2 compliant packets transmitted)
 * @endcode
 */

#include "ns3/ethernet-channel.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/tsn-module.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ethernet-generator.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable debug logs to observe dynamic token bucket calculations and drops
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Node and clock initialization
    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Ptr<TsnNode> n1 = CreateObject<TsnNode>();

    Ptr<Clock> clock0 = CreateObject<Clock>();
    Ptr<Clock> clock1 = CreateObject<Clock>();
    n0->SetMainClock(clock0);
    n1->SetMainClock(clock1);
    n0->AddClock(clock0);
    n1->AddClock(clock1);
    n0->setActiveClock(0);
    n1->setActiveClock(0);

    // High line-rate NetDevice configuration to isolate shaping latency from transmission limits
    Ptr<TsnNetDevice> net0 = CreateObject<TsnNetDevice>();
    n0->AddDevice(net0);
    Ptr<TsnNetDevice> net1 = CreateObject<TsnNetDevice>();
    n1->AddDevice(net1);

    net0->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    net1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    Ptr<EthernetChannel> channel = CreateObject<EthernetChannel>();
    net0->Attach(channel);
    net1->Attach(channel);

    net0->SetAddress(Mac48Address::Allocate());
    net1->SetAddress(Mac48Address::Allocate());

    // Mandatory 8-queue structure initialization for TSN scheduling
    for (int i = 0; i < 8; i++)
    {
        net0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // Activation and Configuration of the ATS Core Engine
    net0->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> ats = net0->GetAts();
    ats->SetClock(clock0);
    ats->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(1))); // 1ms maximum residence ceiling

    // Traffic Generator setup (Dense 5-packet Flash Burst)
    Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
    app0->Setup(net0);
    app0->SetAttribute("BurstSize", UintegerValue(5));
    app0->SetAttribute("PayloadSize", UintegerValue(500));
    app0->SetAttribute("Period", TimeValue(MicroSeconds(10)));
    app0->SetAttribute("VlanID", UintegerValue(1));
    app0->SetAttribute("PCP", UintegerValue(5));
    n0->AddApplication(app0);

    // Immediate operational window cutoff to enforce only a single burst event
    app0->SetStartTime(MilliSeconds(10));
    app0->SetStopTime(MilliSeconds(10) + MicroSeconds(2));

    // Simulation Execution
    NS_LOG_UNCOND("Starting ATS Debug Simulation...");
    Simulator::Stop(MilliSeconds(50));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_UNCOND("Simulation Finished.");

    return 0;
}