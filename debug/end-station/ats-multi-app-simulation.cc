/**
 * @file ats-multi-app-simulation.cc
 * @author Arthur
 * @date June 22, 2026
 * @brief Demonstration of ATS (Asynchronous Traffic Shaping) multi-application
 * isolation within a TSN End-Station.
 * * @details This script configures a source TSN node generating two concurrent
 * asynchronous streams (Stream 10 and Stream 20) to demonstrate how the ATS
 * engine (IEEE 802.1Qcr) enforces per-stream isolation using token-bucket
 * logic, preventing traffic mutual interference without global clock synchronization.
 *
 * @section arch Detailed Network and Component Architecture
 * @code
 * +-----------------------------------------------------------------------+
 * |                            TSN NODE (Source)                          |
 * |  [srcClock]                                                           |
 * |                                                                       |
 * |   +------------------+                    +------------------+        |
 * |   |  Application 1   |                    |  Application 2   |        |
 * |   |  (Stream ID: 10) |                    |  (Stream ID: 20) |        |
 * |   +--------+---------+                    +--------+---------+        |
 * |            |                                       |                  |
 * |            v                                       v                  |
 * |  +-----------------------------------------------------------------+  |
 * |  |                         TsnNetDevice                            |  |
 * |  |                                                                 |  |
 * |  |  +-----------------------------------------------------------+  |  |
 * |  |  | ATS ENGINE (isAtsEnabled = true)                          |  |  |
 * |  |  |                                                           |  |  |
 * |  |  |  +--------------------+           +--------------------+  |  |  |
 * |  |  |  | Stream Queue #10   |           | Stream Queue #20   |  |  |  |
 * |  |  |  +---------+----------+           +---------+----------+  |  |  |
 * |  |  |            |                                |             |  |  |
 * |  |  |            v                                v             |  |  |
 * |  |  |  +--------------------+           +--------------------+  |  |  |
 * |  |  |  | Scheduler Group A  |           | Scheduler Group B  |  |  |  |
 * |  |  |  +---------+----------+           +---------+----------+  |  |  |
 * |  |  +------------|--------------------------------|-------------+  |  |
 * |  |               v                                v                |  |
 * |  |    +-------------------------------------------------------+    |  |
 * |  |    | Transmission Queues (8 Standard CoS Queues / DropTail)|    |  |
 * |  |    +---------------------------+---------------------------+    |  |
 * |  +--------------------------------|--------------------------------+  |
 * +-----------------------------------|-----------------------------------+
 *                                     |
 *                                     | [EthernetChannel]
 * +-----------------------------------|-----------------------------------+
 * |                                   v                                   |
 * |    +-------------------------------------------------------+          |
 * |    | Transmission Queues (8 Standard CoS Queues / DropTail)|          |
 * |    +------------------------------+------------------------+          |
 * |                                   |                                   |
 * |                         TSN NODE (Destination)                        |
 * |                         [dstClock]                                    |
 * +-----------------------------------------------------------------------+
 * @endcode
 */

#include "ns3/core-module.h"
#include "ns3/tsn-module.h"
#include "ns3/traffic-generator-module.h"
#include "ns3/network-module.h"
#include "ns3/ethernet-channel.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Enable detailed logging for ATS components to verify per-stream isolation
    LogComponentEnable("Ats", LOG_LEVEL_FUNCTION);
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);

    std::cout << "\n========================================================" << std::endl;
    std::cout << "Starting ATS Multi-Application End-Station Isolation Demonstration..." << std::endl;
    std::cout << "========================================================\n"
              << std::endl;

    // Node and clock initialization
    Ptr<TsnNode> srcNode = CreateObject<TsnNode>();
    Ptr<TsnNode> dstNode = CreateObject<TsnNode>();

    Ptr<Clock> srcClock = CreateObject<Clock>();
    Ptr<Clock> dstClock = CreateObject<Clock>();
    srcNode->SetMainClock(srcClock);
    dstNode->SetMainClock(dstClock);

    // Network interface creation
    Ptr<TsnNetDevice> txDevice = CreateObject<TsnNetDevice>();
    srcNode->AddDevice(txDevice);
    Ptr<TsnNetDevice> rxDevice = CreateObject<TsnNetDevice>();
    dstNode->AddDevice(rxDevice);

    // Channel attachment
    Ptr<EthernetChannel> channel = CreateObject<EthernetChannel>();
    txDevice->Attach(channel);
    rxDevice->Attach(channel);

    txDevice->SetAddress(Mac48Address::Allocate());
    rxDevice->SetAddress(Mac48Address::Allocate());

    for (int i = 0; i < 8; i++)
    {
        txDevice->SetQueue(CreateObject<DropTailQueue<Packet>>());
        rxDevice->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // ATS Configuration on transmitter interface
    txDevice->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> atsEngine = txDevice->GetAts();
    atsEngine->SetClock(srcClock);
    atsEngine->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(5)));

    // Application 1 -> Generates Stream ID #10
    Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
    app1->Setup(txDevice);
    app1->SetAttribute("StreamId", UintegerValue(10));
    app1->SetAttribute("BurstSize", UintegerValue(1));
    app1->SetAttribute("PayloadSize", UintegerValue(500));
    app1->SetAttribute("Period", TimeValue(Seconds(1)));
    srcNode->AddApplication(app1);
    app1->SetStartTime(MilliSeconds(20));
    app1->SetStopTime(MilliSeconds(25));

    // Application 2 -> Generates Stream ID #20 (Concurrent Transmission)
    Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
    app2->Setup(txDevice);
    app2->SetAttribute("StreamId", UintegerValue(20));
    app2->SetAttribute("BurstSize", UintegerValue(1));
    app2->SetAttribute("PayloadSize", UintegerValue(500));
    app2->SetAttribute("Period", TimeValue(Seconds(1)));
    srcNode->AddApplication(app2);
    app2->SetStartTime(MilliSeconds(20));
    app2->SetStopTime(MilliSeconds(25));

    // Simulation control
    Simulator::Stop(MilliSeconds(50));
    Simulator::Run();
    Simulator::Destroy();

    std::cout << "\n========================================================" << std::endl;
    std::cout << "Simulation Finished. Check logs to confirm isolated instances!" << std::endl;
    std::cout << "========================================================\n"
              << std::endl;

    return 0;
}