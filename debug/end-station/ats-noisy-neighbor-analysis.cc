/**
 * @file ats-noisy-neighbor-analysis.cc
 * @author Arthur
 * @date June 23, 2026
 * @brief Terminal analysis script for ATS Stream ID segregation (Noisy Neighbor isolation).
 */

#include "ns3/ethernet-channel.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/tsn-module.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ethernet-generator.h"
#include "ns3/ethernet-header2.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AtsNoisyNeighborAnalysis");

uint32_t g_stream10RxCount = 0;
uint32_t g_stream20RxCount = 0;

/**
 * @brief Callback connected to the receiver net device to log and sort incoming traffic.
 * @param p The received packet.
 */
void RecordRxTraffic(Ptr<const Packet> p)
{
    EthernetHeader2 header;
    Ptr<Packet> copy = p->Copy();
    copy->RemoveHeader(header);

    uint32_t pSize = p->GetSize();

    if (pSize == 522) // App 1 (Noisy)
    {
        g_stream10RxCount++;
        std::cout << "[ANALYSIS-RX] Stream 10 (Noisy) packet arrived at: "
                  << Simulator::Now().GetMicroSeconds() << " us" << std::endl;
    }
    else if (pSize == 322) // App 2 (Compliant) -> Differentiated by size
    {
        g_stream20RxCount++;
        std::cout << "[ANALYSIS-RX] Stream 20 (Compliant) packet arrived at: "
                  << Simulator::Now().GetMicroSeconds() << " us" << std::endl;
    }
}

int main(int argc, char *argv[])
{
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Ptr<TsnNode> n1 = CreateObject<TsnNode>();

    Ptr<Clock> clock0 = CreateObject<Clock>();
    Ptr<Clock> clock1 = CreateObject<Clock>();
    n0->SetMainClock(clock0);
    n1->SetMainClock(clock1);

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

    for (int i = 0; i < 8; i++)
    {
        net0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    net0->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> ats = net0->GetAts();
    ats->SetClock(clock0);
    ats->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(1)));

    net1->TraceConnectWithoutContext("MacRx", MakeCallback(&RecordRxTraffic));

    // -------------------------------------------------------------------------
    // APPLICATION 1: Stream 10 - The Noisy Neighbor (Payload = 500 -> Total Size = 522)
    // -------------------------------------------------------------------------
    Ptr<EthernetGenerator> appMalicious = CreateObject<EthernetGenerator>();
    appMalicious->Setup(net0);
    appMalicious->SetAttribute("StreamId", UintegerValue(10));
    appMalicious->SetAttribute("BurstSize", UintegerValue(6));
    appMalicious->SetAttribute("PayloadSize", UintegerValue(500));
    appMalicious->SetAttribute("Period", TimeValue(MicroSeconds(10)));
    appMalicious->SetAttribute("VlanID", UintegerValue(1));
    appMalicious->SetAttribute("PCP", UintegerValue(5));
    n0->AddApplication(appMalicious);

    // -------------------------------------------------------------------------
    // APPLICATION 2: Stream 20 - The Compliant Flow (Payload = 300 -> Total Size = 322)
    // -------------------------------------------------------------------------
    Ptr<EthernetGenerator> appCompliant = CreateObject<EthernetGenerator>();
    appCompliant->Setup(net0);
    appCompliant->SetAttribute("StreamId", UintegerValue(20));
    appCompliant->SetAttribute("BurstSize", UintegerValue(2));
    appCompliant->SetAttribute("PayloadSize", UintegerValue(300));
    appCompliant->SetAttribute("Period", TimeValue(MicroSeconds(10)));
    appCompliant->SetAttribute("VlanID", UintegerValue(1));
    appCompliant->SetAttribute("PCP", UintegerValue(5));
    n0->AddApplication(appCompliant);

    Time startTime = MilliSeconds(0);
    appMalicious->SetStartTime(startTime);
    appMalicious->SetStopTime(startTime + MicroSeconds(5));

    appCompliant->SetStartTime(startTime);
    appCompliant->SetStopTime(startTime + MicroSeconds(5));

    std::cout << "Starting ATS Segregation Analysis Script (Simultaneous burst at t=0ms)..." << std::endl;
    Simulator::Stop(MilliSeconds(50));
    Simulator::Run();

    std::cout << "\n========================================================" << std::endl;
    std::cout << "             ATS TRAFFIC ISOLATION ANALYSIS              " << std::endl;
    std::cout << "========================================================" << std::endl;
    std::cout << "Stream 10 (Noisy Neighbor) [522B] -> Received: " << g_stream10RxCount << " / 6 frames." << std::endl;
    std::cout << "Stream 20 (Compliant Flow) [322B] -> Received: " << g_stream20RxCount << " / 2 frames." << std::endl;
    std::cout << "========================================================" << std::endl;

    Simulator::Destroy();
    return 0;
}