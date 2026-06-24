/**
 * \file ats-bridge-ingress-isolation-analysis.cc
 * \author Arthur
 * \date June 24, 2026
 * \brief Analysis script evaluating ATS scheduler group behavior for traffic
 * originating from different ingress ports.
 *
 * \details This script validates whether the ATS implementation groups frames
 * strictly by Traffic Class (Per-Priority aggregation) or separates them based
 * on the ingress interface (Per-Port-Per-Priority isolation).
 * * Topology:
 * - ESsource1 (Port 1) ---> [ SW1 ] ---> ESdest (Port 3)
 * - ESsource2 (Port 2) -------^ (ATS Enabled on Port 3 Egress)
 *
 * Both sources inject synchronous bursts with identical VlanIDs and PCPs.
 * By inspecting the AtsSchedulerGroup memory addresses and inter-packet arrival
 * spacings at ESdest, we can determine the segregation policy.
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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AtsBridgeIngressIsolationAnalysis");

uint32_t g_listenerRxCount = 0;
std::vector<Time> g_rxTimes;
std::vector<uint32_t> g_rxUids;
std::vector<Mac48Address> g_rxSrcMacs;

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
    g_rxSrcMacs.push_back(ethHeader.GetSrc());

    std::cout << "[DEST-RX] Frame Received | Context: " << context
              << " | Packet UID: " << p->GetUid()
              << " | From Src MAC: " << ethHeader.GetSrc()
              << " | VID: " << ethHeader.GetVid()
              << " | PCP: " << (uint32_t)ethHeader.GetPcp()
              << " | Arrival Time: " << Simulator::Now().GetMicroSeconds() << " us" << std::endl;
}

int main(int argc, char *argv[])
{
    // Enable core logging components to trace memory instances
    LogComponentEnable("AtsBridgeIngressIsolationAnalysis", LOG_LEVEL_INFO);
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);
    LogComponentEnable("SwitchNetDevice", LOG_LEVEL_INFO);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create 2 Source End-Stations, 1 Destination End-Station, and 1 Switch
    Ptr<TsnNode> nSource1 = CreateObject<TsnNode>();
    Names::Add("ESsource1", nSource1);
    Ptr<TsnNode> nSource2 = CreateObject<TsnNode>();
    Names::Add("ESsource2", nSource2);
    Ptr<TsnNode> nDest = CreateObject<TsnNode>();
    Names::Add("ESdest", nDest);
    Ptr<TsnNode> nSw1 = CreateObject<TsnNode>();
    Names::Add("SW1", nSw1);

    // Setup Clocks
    Ptr<Clock> clockSrc1 = CreateObject<Clock>();
    Ptr<Clock> clockSrc2 = CreateObject<Clock>();
    Ptr<Clock> clockSw1 = CreateObject<Clock>();
    Ptr<Clock> clockDest = CreateObject<Clock>();
    nSource1->SetMainClock(clockSrc1);
    nSource2->SetMainClock(clockSrc2);
    nSw1->SetMainClock(clockSw1);
    nDest->SetMainClock(clockDest);

    // Instantiate TSN Network Interfaces
    Ptr<TsnNetDevice> netSource1 = CreateObject<TsnNetDevice>();
    nSource1->AddDevice(netSource1);
    Names::Add("ESsource1#Port0", netSource1);

    Ptr<TsnNetDevice> netSource2 = CreateObject<TsnNetDevice>();
    nSource2->AddDevice(netSource2);
    Names::Add("ESsource2#Port0", netSource2);

    Ptr<TsnNetDevice> netDest = CreateObject<TsnNetDevice>();
    nDest->AddDevice(netDest);
    Names::Add("ESdest#Port0", netDest);

    // Switch Ports (Port 1 & 2: Ingress, Port 3: Egress)
    Ptr<TsnNetDevice> netSw1_1 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_1);
    Names::Add("SW1#Port1", netSw1_1);

    Ptr<TsnNetDevice> netSw1_2 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_2);
    Names::Add("SW1#Port2", netSw1_2);

    Ptr<TsnNetDevice> netSw1_3 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_3);
    Names::Add("SW1#Port3", netSw1_3);

    // Set Link Speeds to 1 Gbps
    netSource1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSource2->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_2->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_3->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netDest->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    // Connect Channels
    Ptr<EthernetChannel> chA = CreateObject<EthernetChannel>();
    netSource1->Attach(chA);
    netSw1_1->Attach(chA);

    Ptr<EthernetChannel> chB = CreateObject<EthernetChannel>();
    netSource2->Attach(chB);
    netSw1_2->Attach(chB);

    Ptr<EthernetChannel> chC = CreateObject<EthernetChannel>();
    netSw1_3->Attach(chC);
    netDest->Attach(chC);

    // Setup Switch Bridging Engine
    Ptr<SwitchNetDevice> sw1 = CreateObject<SwitchNetDevice>();
    sw1->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(10)));
    sw1->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(10)));
    nSw1->AddDevice(sw1);
    sw1->AddSwitchPort(netSw1_1);
    sw1->AddSwitchPort(netSw1_2);
    sw1->AddSwitchPort(netSw1_3);

    // Allocate MAC Addresses
    Mac48Address macSrc1 = Mac48Address::Allocate();
    Mac48Address macSrc2 = Mac48Address::Allocate();
    Mac48Address macDest = Mac48Address::Allocate();
    netSource1->SetAddress(macSrc1);
    netSource2->SetAddress(macSrc2);
    netDest->SetAddress(macDest);
    sw1->SetAddress(Mac48Address::Allocate());

    // Initialize Transmission Queues
    for (int i = 0; i < 8; i++)
    {
        netSource1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSource2->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netDest->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_2->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_3->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // Configure Forwarding Table Entries (FDB) for VLAN 100 pointing to Egress Port 3
    sw1->AddForwardingTableEntry(macDest, 100, {netSw1_3});

    // Enable ATS on Switch Shared Egress Port (Port 3)
    netSw1_3->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> atsEngine = netSw1_3->GetAts();
    atsEngine->SetClock(clockSw1);
    atsEngine->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(20)));

    // Connect Listener
    netDest->TraceConnectWithoutContext("MacRx", MakeBoundCallback(&MacRxCallback, "ESdest"));

    // =========================================================================
    // TRAFFIC PROFILE SETUP: Parallel Bursts from Separate Ingress Interfaces
    // =========================================================================

    // App 1 on ESsource1 (Sends 2 frames via Port 1, VLAN 100, PCP 3)
    Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
    app1->Setup(netSource1);
    app1->SetAttribute("Address", AddressValue(macDest));
    app1->SetAttribute("BurstSize", UintegerValue(2));
    app1->SetAttribute("PayloadSize", UintegerValue(1400)); // Total frame = 1422 bytes
    app1->SetAttribute("Period", TimeValue(MicroSeconds(5)));
    app1->SetAttribute("PCP", UintegerValue(3));
    app1->SetAttribute("VlanID", UintegerValue(100));
    nSource1->AddApplication(app1);
    app1->SetStartTime(MilliSeconds(0));
    app1->SetStopTime(MicroSeconds(2));

    // App 2 on ESsource2 (Sends 2 frames via Port 2, VLAN 100, PCP 3)
    // Identical Priority and VLAN to test if the Switch merges them or splits them
    Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
    app2->Setup(netSource2);
    app2->SetAttribute("Address", AddressValue(macDest));
    app2->SetAttribute("BurstSize", UintegerValue(2));
    app2->SetAttribute("PayloadSize", UintegerValue(1400)); // Total frame = 1422 bytes
    app2->SetAttribute("Period", TimeValue(MicroSeconds(5)));
    app2->SetAttribute("PCP", UintegerValue(3));
    app2->SetAttribute("VlanID", UintegerValue(100));
    nSource2->AddApplication(app2);
    app2->SetStartTime(MilliSeconds(0));
    app2->SetStopTime(MicroSeconds(2));

    std::cout << "Starting ATS Ingress Port Separation Verification..." << std::endl;
    Simulator::Stop(MilliSeconds(50));
    Simulator::Run();

    std::cout << "\n========================================================" << std::endl;
    std::cout << "             ATS INGRESS TRANSIT RESULTS                " << std::endl;
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