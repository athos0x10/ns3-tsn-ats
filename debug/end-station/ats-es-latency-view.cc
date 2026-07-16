/**
 * @file ats-es-latency-view.cc
 * @author Arthur
 * @date June 22, 2026
 * @brief Verification and demonstration of ATS shaping latency accuracy.
 *
 * @details This script validates the packet spacing precision of the ATS engine
 * against theoretical expectations. By capturing the precise arrival
 * times of back-to-back burst frames at the receiver using trace callbacks, it proves
 * that the inter-packet gap perfectly matches the length recovery time dictated by the CIR.
 *
 * @section arch Testbed Topology & Latency Measurement Architecture
 * @code
 * +----------------------------------+          +----------------------------------+
 * |        TSN NODE (Source)         |          |      TSN NODE (Destination)      |
 * |  [srcClock]                      |          |  [dstClock]                      |
 * |                                  |          |                                  |
 * |   +--------------------------+   |          |                                  |
 * |   |    EthernetGenerator     |   |          |                                  |
 * |   |  (Burst Size: 2 Packets) |   |          |                                  |
 * |   +------------+-------------+   |          |                                  |
 * |                |                 |          |                                  |
 * |                v                 |          |                                  |
 * |   +--------------------------+   |          |   +--------------------------+   |
 * |   |       TsnNetDevice       |   |          |   |       TsnNetDevice       |   |
 * |   |      (isAtsEnabled)      |   |          |   |    [8 Queues Standard]   |   |
 * |   |    [8 Queues + ATS]      |   |          |   +------------+-------------+   |
 * |   +------------+-------------+   |          |                |                 |
 * +----------------|-----------------+          +----------------|-----------------+
 * |                                             |
 * +------------------[ EthernetChannel ]--------+
 * |
 * v (Triggers Trace)
 * +--------------------------+
 * |     RxTraceCallback      |
 * |                          |
 * | Calculates Real Delta:   |
 * | t(Pkt1) - t(Pkt0)        |
 * | Target: ~414.4 us        |
 * +--------------------------+
 * @endcode
 */

#include "ns3/core-module.h"
#include "ns3/tsn-module.h"
#include "ns3/traffic-generator-module.h"
#include "ns3/network-module.h"
#include "ns3/ethernet-channel.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AtsLatencyView");

// Global tracking variables for simulation metrics
static Time g_firstPacketTime = Seconds(0);  ///< Arrival timestamp of the first packet
static Time g_secondPacketTime = Seconds(0); ///< Arrival timestamp of the second packet
static uint32_t g_packetCount = 0;           ///< Global received packet counter

/**
 * @brief Trace callback connected to the receiver's MacRx interface.
 * @details Captures timestamps for the two burst packets, computes the real-time
 * shaping latency delta, and logs the results against the theoretical baseline.
 * @param packet The received packet reference.
 */
void RxTraceCallback(Ptr<const Packet> packet)
{
    g_packetCount++;
    Time currentTime = Simulator::Now();
    std::cout << "[RECEIVER] Packet Received #" << g_packetCount
              << " | UID: " << packet->GetUid()
              << " | Timestamp: " << currentTime.As(Time::S) << std::endl;

    if (g_packetCount == 1)
    {
        g_firstPacketTime = currentTime;
    }
    else if (g_packetCount == 2)
    {
        g_secondPacketTime = currentTime;
        Time delta = g_secondPacketTime - g_firstPacketTime;
        std::cout << "\n==================================================" << std::endl;
        std::cout << ">> Measured Inter-Arrival Gap (Shaping Latency) : " << delta.As(Time::US) << std::endl;
        std::cout << ">> Theoretical Expected Spacing (518B @ 10Mbps) : ~414.4 us" << std::endl;
        std::cout << "==================================================\n"
                  << std::endl;
    }
}

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Enable granular ATS execution logs to follow internal leaky-bucket math
    LogComponentEnable("Ats", LOG_LEVEL_FUNCTION);
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);

    std::cout << "\n========================================================" << std::endl;
    std::cout << "Starting ATS Shaping Latency Precision Demonstration..." << std::endl;
    std::cout << "========================================================\n"
              << std::endl;

    // Node and clock initialization
    Ptr<TsnNode> srcNode = CreateObject<TsnNode>();
    Ptr<TsnNode> dstNode = CreateObject<TsnNode>();

    Ptr<Clock> srcClock = CreateObject<Clock>();
    Ptr<Clock> dstClock = CreateObject<Clock>();
    srcNode->SetMainClock(srcClock);
    dstNode->SetMainClock(dstClock);

    Ptr<TsnNetDevice> txDevice = CreateObject<TsnNetDevice>();
    srcNode->AddDevice(txDevice);
    Ptr<TsnNetDevice> rxDevice = CreateObject<TsnNetDevice>();
    dstNode->AddDevice(rxDevice);

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

    txDevice->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> atsEngine = txDevice->GetAts();
    atsEngine->SetClock(srcClock);
    atsEngine->SetPriorityActivation(0, true);
    atsEngine->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(10))); // Large ceiling to prevent drops

    // Hooking the measurement callback onto the destination MAC receive trace
    rxDevice->TraceConnectWithoutContext("MacRx", MakeCallback(&RxTraceCallback));

    Ptr<EthernetGenerator> app = CreateObject<EthernetGenerator>();
    app->Setup(txDevice);
    app->SetAttribute("BurstSize", UintegerValue(2));
    app->SetAttribute("PayloadSize", UintegerValue(500));
    app->SetAttribute("Period", TimeValue(Seconds(1)));

    srcNode->AddApplication(app);
    app->SetStartTime(MilliSeconds(0));
    app->SetStopTime(MilliSeconds(5));

    // Simulation execution window
    Simulator::Stop(MilliSeconds(100));
    Simulator::Run();
    Simulator::Destroy();

    std::cout << "\n========================================================" << std::endl;
    std::cout << "Simulation Finished. Analyze the calculated delta time!" << std::endl;
    std::cout << "========================================================\n"
              << std::endl;

    return 0;
}