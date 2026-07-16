/**
 * \file ats-bridge-single.cc
 * \author Arthur
 * \date June 23, 2026
 * \brief Analysis script for ATS transit inside an IEEE 802.1Qcr TSN SwitchNetDevice.
 *
 * \details This script validates the integration of the ATS engine inside standard
 * ns-3 SwitchNetDevice ports.
 * - ESsource sends an initial back-to-back burst of frames (VlanId 100).
 * - SW1 handles incoming frames, reads the L2 forwarding table entries, and multiplexes
 * the stream onto the egress port (net2_2) where ATS shaping is enabled.
 * - ESdest captures the shaped traffic to verify line-rate spacing compliance.
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

NS_LOG_COMPONENT_DEFINE("AtsBridgeForwardingAnalysis");

uint32_t g_listenerRxCount = 0;
std::vector<Time> g_rxTimes;

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
    LogComponentEnable("AtsBridgeForwardingAnalysis", LOG_LEVEL_INFO);
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
    netDest->SetAddress(Mac48Address::Allocate());
    sw1->SetAddress(Mac48Address::Allocate());

    for (int i = 0; i < 8; i++)
    {
        netSource->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netDest->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_2->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    sw1->AddForwardingTableEntry(Mac48Address("ff:ff:ff:ff:ff:ff"), 100, {netSw1_2});

    // Stream Identification
    Ptr<NullStreamIdentificationFunction> sif0 = CreateObject<NullStreamIdentificationFunction>();
    uint16_t StreamHandle = 10;
    sif0->SetAttribute("VlanID", UintegerValue(100));
    sif0->SetAttribute("Address", AddressValue(Mac48Address("ff:ff:ff:ff:ff:ff")));
    nSw1->AddStreamIdentificationFunction(StreamHandle, sif0, {netSw1_1}, {}, {}, {});

    netSw1_2->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> atsEngine = netSw1_2->GetAts();
    atsEngine->SetClock(clockSw1);
    atsEngine->SetPriorityActivation(1, true);
    atsEngine->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(5)));

    netDest->TraceConnectWithoutContext("MacRx", MakeBoundCallback(&MacRxCallback, "ESdest"));

    Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
    app0->Setup(netSource);
    app0->SetAttribute("BurstSize", UintegerValue(1));         // 1-frame evaluation burst
    app0->SetAttribute("PayloadSize", UintegerValue(1400));    // Standard large payload size
    app0->SetAttribute("Period", TimeValue(MicroSeconds(10))); // High rate burst arrival sequence
    app0->SetAttribute("PCP", UintegerValue(1));
    app0->SetAttribute("VlanID", UintegerValue(100));
    nSource->AddApplication(app0);
    app0->SetStartTime(MilliSeconds(0));
    app0->SetStopTime(MilliSeconds(0) + MicroSeconds(25));

    std::cout << "Starting ATS SwitchNetDevice Transit Verification..." << std::endl;
    Simulator::Stop(MilliSeconds(20));
    Simulator::Run();

    std::cout << "\n========================================================" << std::endl;
    std::cout << "             ATS SWITCH TRANSIT RESULTS                 " << std::endl;
    std::cout << "========================================================" << std::endl;
    std::cout << "Total Packets Received: " << g_listenerRxCount << " / 3" << std::endl;

    if (g_rxTimes.size() >= 3)
    {
        double delta1 = (g_rxTimes[1] - g_rxTimes[0]).GetMicroSeconds();
        double delta2 = (g_rxTimes[2] - g_rxTimes[1]).GetMicroSeconds();
        std::cout << "Inter-packet spacing (P0 -> P1): " << delta1 << " us" << std::endl;
        std::cout << "Inter-packet spacing (P1 -> P2): " << delta2 << " us" << std::endl;
    }
    std::cout << "========================================================" << std::endl;

    Simulator::Destroy();
    return 0;
}