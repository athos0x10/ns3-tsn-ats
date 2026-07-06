/**
 * \file ats-bridge-multiplexing.cc
 * \author Arthur
 * \date June 24, 2026
 * \brief Analysis script for multi-application multiplexing inside an IEEE 802.1Qcr TSN Switch.
 *
 * \details This script validates multiplexed ATS shaping inside standard ns-3 SwitchNetDevice ports.
 * - ESsource executes two separate applications simultaneously.
 * - Both applications inject high-rate bursts into SW1 on Port 1.
 * - SW1 switches and aggregates both applications onto Port 2 (egress) where ATS is enabled.
 * - ESdest captures the aggregated traffic to verify that the shaper spaces the interleaved frames.
 */

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/command-line.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/drop-tail-queue.h"

#include "ns3/tsn-node.h"
#include "ns3/tsn-net-device.h"
#include "ns3/switch-net-device.h"
#include "ns3/ethernet-channel.h"
#include "ns3/ethernet-generator.h"
#include "ns3/ethernet-header2.h"
#include "ns3/ats.h"
#include "ns3/stream-identification-function-null.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AtsBridgeMultiplexingAnalysis");

uint32_t g_listenerRxCount = 0;
std::vector<Time> g_rxTimes;
std::vector<uint32_t> g_rxUids;

/**
 * \brief Callback tracking packet arrivals at the destination Listener End-Station.
 * \param context Bound context indicating node and interface.
 * \param p The received packet.
 */
static void
MacRxCallback(std::string context, Ptr<const Packet> p)
{
    g_listenerRxCount++;
    g_rxTimes.push_back(Simulator::Now());
    g_rxUids.push_back(p->GetUid());

    Ptr<Packet> originalPacket = p->Copy();
    EthernetHeader2 ethHeader;
    originalPacket->RemoveHeader(ethHeader);

    std::cout << "[DEST-RX] Frame Received | Context: " << context
              << " | Packet UID: " << p->GetUid()
              << " | VID: " << ethHeader.GetVid()
              << " | Arrival Time: " << Simulator::Now().GetMicroSeconds() << " us" << std::endl;
}

int main(int argc, char *argv[])
{
    // Enable core logging components
    LogComponentEnable("AtsBridgeMultiplexingAnalysis", LOG_LEVEL_INFO);
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);
    LogComponentEnable("SwitchNetDevice", LOG_LEVEL_INFO);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create End-Stations and Switch nodes
    Ptr<TsnNode> nSource = CreateObject<TsnNode>();
    Names::Add("ESsource", nSource);
    Ptr<TsnNode> nDest = CreateObject<TsnNode>();
    Names::Add("ESdest", nDest);
    Ptr<TsnNode> nSw1 = CreateObject<TsnNode>();
    Names::Add("SW1", nSw1);

    Ptr<Clock> clockSource = CreateObject<Clock>();
    Ptr<Clock> clockSw1 = CreateObject<Clock>();
    Ptr<Clock> clockDest = CreateObject<Clock>();
    nSource->SetMainClock(clockSource);
    nSw1->SetMainClock(clockSw1);
    nDest->SetMainClock(clockDest);

    Ptr<TsnNetDevice> netSource = CreateObject<TsnNetDevice>();
    nSource->AddDevice(netSource);
    Names::Add("ESsource#01", netSource);

    Ptr<TsnNetDevice> netDest = CreateObject<TsnNetDevice>();
    nDest->AddDevice(netDest);
    Names::Add("ESdest#01", netDest);

    Ptr<TsnNetDevice> netSw1_1 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_1);
    Names::Add("SW1#Port1", netSw1_1);

    Ptr<TsnNetDevice> netSw1_2 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_2);
    Names::Add("SW1#Port2", netSw1_2);

    netSource->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_2->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netDest->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    Ptr<EthernetChannel> chA = CreateObject<EthernetChannel>();
    netSource->Attach(chA);
    netSw1_1->Attach(chA);

    Ptr<EthernetChannel> chB = CreateObject<EthernetChannel>();
    netSw1_2->Attach(chB);
    netDest->Attach(chB);

    Ptr<SwitchNetDevice> sw1 = CreateObject<SwitchNetDevice>();
    sw1->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(10)));
    sw1->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(10)));
    nSw1->AddDevice(sw1);
    sw1->AddSwitchPort(netSw1_1);
    sw1->AddSwitchPort(netSw1_2);

    netSource->SetAddress(Mac48Address::Allocate());
    Mac48Address macDest = Mac48Address::Allocate();
    netDest->SetAddress(macDest);
    sw1->SetAddress(Mac48Address::Allocate());

    for (int i = 0; i < 8; i++)
    {
        netSource->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netDest->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_2->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    sw1->AddForwardingTableEntry(macDest, 100, {netSw1_2});
    sw1->AddForwardingTableEntry(macDest, 110, {netSw1_2});

    // Stream identification
    Ptr<NullStreamIdentificationFunction> sif1 = CreateObject<NullStreamIdentificationFunction>();
    uint16_t streamHandle1 = 10;
    sif1->SetAttribute("VlanID", UintegerValue(100));
    sif1->SetAttribute("Address", AddressValue(macDest));
    nSw1->AddStreamIdentificationFunction(streamHandle1, sif1, {netSw1_1}, {}, {}, {});

    Ptr<NullStreamIdentificationFunction> sif2 = CreateObject<NullStreamIdentificationFunction>();
    uint16_t streamHandle2 = 20;
    sif2->SetAttribute("VlanID", UintegerValue(110));
    sif2->SetAttribute("Address", AddressValue(macDest));
    nSw1->AddStreamIdentificationFunction(streamHandle2, sif2, {netSw1_1}, {}, {}, {});

    // Configure ATS Engine on Switch Egress Port (Port 2)
    netSw1_2->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> atsEngine = netSw1_2->GetAts();
    atsEngine->SetClock(clockSw1);
    atsEngine->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(20)));

    netDest->TraceConnectWithoutContext("MacRx", MakeBoundCallback(&MacRxCallback, "ESdest"));

    // Application 1 (Generates Burst of 2 frames, VLAN 100, PCP 1)
    Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
    app1->Setup(netSource);
    app1->SetAttribute("Address", AddressValue(netDest->GetAddress()));
    app1->SetAttribute("BurstSize", UintegerValue(2));
    app1->SetAttribute("PayloadSize", UintegerValue(1400)); // Frame size = 1422 bytes
    app1->SetAttribute("Period", TimeValue(MicroSeconds(5)));
    app1->SetAttribute("PCP", UintegerValue(1));
    app1->SetAttribute("VlanID", UintegerValue(100));
    nSource->AddApplication(app1);
    app1->SetStartTime(MilliSeconds(0));
    app1->SetStopTime(MicroSeconds(2));

    // Application 2 (Generates Burst of 2 frames at the exact same time, VLAN 110, PCP 1)
    Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
    app2->Setup(netSource);
    app2->SetAttribute("Address", AddressValue(netDest->GetAddress()));
    app2->SetAttribute("BurstSize", UintegerValue(2));
    app2->SetAttribute("PayloadSize", UintegerValue(1400)); // Frame size = 1422 bytes
    app2->SetAttribute("Period", TimeValue(MicroSeconds(5)));
    app2->SetAttribute("PCP", UintegerValue(1));
    app2->SetAttribute("VlanID", UintegerValue(110));
    nSource->AddApplication(app2);
    app2->SetStartTime(MilliSeconds(0));
    app2->SetStopTime(MicroSeconds(2));

    std::cout << "Starting ATS Switch Multiplexing Verification..." << std::endl;
    Simulator::Stop(MilliSeconds(50));
    Simulator::Run();

    std::cout << "\n========================================================" << std::endl;
    std::cout << "             ATS MULTIPLEXING TRANSIT RESULTS           " << std::endl;
    std::cout << "========================================================" << std::endl;
    std::cout << "Total Packets Received: " << g_listenerRxCount << " / 4" << std::endl;

    for (size_t i = 1; i < g_rxTimes.size(); ++i)
    {
        double delta = (g_rxTimes[i] - g_rxTimes[i - 1]).GetMicroSeconds();
        std::cout << "Inter-packet spacing (P" << g_rxUids[i - 1] << " -> P" << g_rxUids[i] << "): "
                  << delta << " us" << std::endl;
    }
    std::cout << "========================================================" << std::endl;

    Simulator::Destroy();
    return 0;
}