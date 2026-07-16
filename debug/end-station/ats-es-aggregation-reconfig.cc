/**
 * \file ats-es-aggregation-reconfig.cc
 * \author Arthur
 * \date June 26, 2026
 * \brief Scratch script demonstrating the ergonomic End-Station API to aggregate
 * multiple applications and reconfigure CIR/CBS parameters.
 *
 * \details This script targets an End-Station environment. It uses the MAC/VLAN-based
 * abstraction layer to fetch the underlying AtsSchedulerGroup, provisions a shared
 * scheduling instance, binds the stream key, and tracks the aggregated shaping.
 */

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/drop-tail-queue.h"

#include "ns3/tsn-node.h"
#include "ns3/tsn-net-device.h"
#include "ns3/ethernet-channel.h"
#include "ns3/ethernet-generator.h"
#include "ns3/ats.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AtsEsAggregationReconfig");

int main(int argc, char *argv[])
{
    LogComponentEnable("AtsEsAggregationReconfig", LOG_LEVEL_INFO);
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);

    // Create End-Station Source and Destination nodes
    Ptr<TsnNode> nSource = CreateObject<TsnNode>();
    Ptr<TsnNode> nDest = CreateObject<TsnNode>();

    // Initialize Network Interfaces
    Ptr<TsnNetDevice> netSource = CreateObject<TsnNetDevice>();
    nSource->AddDevice(netSource);
    Ptr<TsnNetDevice> netDest = CreateObject<TsnNetDevice>();
    nDest->AddDevice(netDest);

    // Link devices via an Ethernet channel
    Ptr<EthernetChannel> channel = CreateObject<EthernetChannel>();
    netSource->Attach(channel);
    netDest->Attach(channel);

    // Configure MAC addresses
    netSource->SetAddress(Mac48Address::Allocate());
    Mac48Address destMac = Mac48Address("00:00:00:00:00:AA");
    netDest->SetAddress(destMac);

    // Provision transmission queues
    for (int i = 0; i < 8; i++)
    {
        netSource->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netDest->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // Activate the ATS subsystem on the Egress End-Station port
    netSource->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> atsEngine = netSource->GetAts();

    // Define context parameters matching our streams
    uint8_t pcpPriority = 4;
    uint16_t vlanId = 100;

    std::cout
        << "[CONFIG] Retrieving Scheduler Group using the End-Station abstraction..."
        << std::endl;

    Ptr<AtsSchedulerGroup> esGroup = atsEngine->GetGroupForEndStation(destMac, vlanId, netSource);
    atsEngine->SetPriorityActivation(pcpPriority, true);

    // Modify the default factory properties of this specific group if dynamic instances spawn
    esGroup->SetAttribute("DefaultCir", DataRateValue(DataRate("15Mbps")));
    esGroup->SetAttribute("DefaultCbs", UintegerValue(32768));

    // Manually create a single, shared instance to force traffic aggregation
    std::cout << "[CONFIG] Creating an explicit shared instance for aggregated applications..." << std::endl;
    uint32_t sharedInstId = esGroup->CreateAtsInstance(DataRate("8Mbps"), 16384);

    // Bind our expected stream configuration to this explicit instance
    esGroup->BindStreamToInstanceES(destMac, vlanId, sharedInstId);

    // Application 1: (VLAN 100, PCP 4)
    Ptr<EthernetGenerator>
        app1 = CreateObject<EthernetGenerator>();
    app1->Setup(netSource);
    app1->SetAttribute("Address", AddressValue(destMac));
    app1->SetAttribute("BurstSize", UintegerValue(1));
    app1->SetAttribute("PayloadSize", UintegerValue(1400));
    app1->SetAttribute("Period", TimeValue(MicroSeconds(20)));
    app1->SetAttribute("PCP", UintegerValue(pcpPriority));
    app1->SetAttribute("VlanID", UintegerValue(vlanId));
    nSource->AddApplication(app1);
    app1->SetStartTime(Seconds(0));
    app1->SetStopTime(MilliSeconds(0) + MicroSeconds(25));

    // Application 2: (Concurrently using VLAN 100, PCP 4 to the same Destination)
    Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
    app2->Setup(netSource);
    app2->SetAttribute("Address", AddressValue(destMac));
    app2->SetAttribute("BurstSize", UintegerValue(1));
    app2->SetAttribute("PayloadSize", UintegerValue(1400));
    app2->SetAttribute("Period", TimeValue(MicroSeconds(20)));
    app2->SetAttribute("PCP", UintegerValue(pcpPriority));
    app2->SetAttribute("VlanID", UintegerValue(vlanId));
    nSource->AddApplication(app2);
    app2->SetStartTime(Seconds(0));
    app2->SetStopTime(MilliSeconds(0) + MicroSeconds(25));

    std::cout << "Starting End-Station ATS Aggregation Simulation..." << std::endl;
    Simulator::Stop(MilliSeconds(15));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}