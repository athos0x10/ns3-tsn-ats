/**
 * \file ats-bridge-aggregation.cc
 * \author Arthur
 * \date June 26, 2026
 * \brief Scratch script demonstrating the ergonomic Bridge API to aggregate
 * multiple forwarded streams into a single shared ATS instance.
 *
 * \details This script simulates a Switch architecture. It extracts interface
 * indices from the ingress and egress TsnNetDevices, maps them to a shared
 * AtsSchedulerGroup, and validates the aggregated shaping of forwarded traffic.
 */

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/drop-tail-queue.h"

#include "ns3/tsn-node.h"
#include "ns3/tsn-net-device.h"
#include "ns3/switch-net-device.h"
#include "ns3/ethernet-channel.h"
#include "ns3/ethernet-generator.h"
#include "ns3/ats.h"
#include "ns3/stream-identification-function-null.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AtsBridgeCorrectAggregation");

int main(int argc, char *argv[])
{
    LogComponentEnable("AtsBridgeCorrectAggregation", LOG_LEVEL_INFO);
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);

    Ptr<TsnNode> es1 = CreateObject<TsnNode>();
    Ptr<TsnNode> sw1 = CreateObject<TsnNode>();
    Ptr<TsnNode> es2 = CreateObject<TsnNode>();

    Ptr<TsnNetDevice> netEs1 = CreateObject<TsnNetDevice>();
    es1->AddDevice(netEs1);
    Ptr<TsnNetDevice> netEs2 = CreateObject<TsnNetDevice>();
    es2->AddDevice(netEs2);

    Ptr<TsnNetDevice> swPort1 = CreateObject<TsnNetDevice>();
    sw1->AddDevice(swPort1);
    Ptr<TsnNetDevice> swPort2 = CreateObject<TsnNetDevice>();
    sw1->AddDevice(swPort2);

    Ptr<EthernetChannel> cable1 = CreateObject<EthernetChannel>();
    netEs1->Attach(cable1);
    swPort1->Attach(cable1);

    Ptr<EthernetChannel> cable2 = CreateObject<EthernetChannel>();
    swPort2->Attach(cable2);
    netEs2->Attach(cable2);

    netEs1->SetAddress(Mac48Address("00:00:00:00:00:01"));
    Mac48Address es2Mac = Mac48Address("00:00:00:00:00:02");
    netEs2->SetAddress(es2Mac);

    Ptr<SwitchNetDevice> swEngine = CreateObject<SwitchNetDevice>();
    sw1->AddDevice(swEngine);
    swEngine->AddSwitchPort(swPort1);
    swEngine->AddSwitchPort(swPort2);
    swEngine->AddForwardingTableEntry(es2Mac, 100, {swPort2});
    swEngine->AddForwardingTableEntry(es2Mac, 200, {swPort2});

    for (int i = 0; i < 8; i++)
    {
        netEs1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netEs2->SetQueue(CreateObject<DropTailQueue<Packet>>());
        swPort1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        swPort2->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // Stream Identification
    uint16_t streamHandle1 = 10;
    Ptr<NullStreamIdentificationFunction> sif1 = CreateObject<NullStreamIdentificationFunction>();
    sif1->SetAttribute("VlanID", UintegerValue(100));
    sif1->SetAttribute("Address", AddressValue(es2Mac));
    sw1->AddStreamIdentificationFunction(streamHandle1, sif1, {swPort1}, {}, {}, {});

    uint16_t streamHandle2 = 20;
    Ptr<NullStreamIdentificationFunction> sif2 = CreateObject<NullStreamIdentificationFunction>();
    sif2->SetAttribute("VlanID", UintegerValue(200));
    sif2->SetAttribute("Address", AddressValue(es2Mac));
    sw1->AddStreamIdentificationFunction(streamHandle2, sif2, {swPort1}, {}, {}, {});

    swPort2->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> swAtsEngine = swPort2->GetAts();
    swAtsEngine->SetPriorityActivation(priority, true);

    uint8_t priority = 6;

    Ptr<AtsSchedulerGroup> bridgeGroup = swAtsEngine->GetGroupForBridge(swPort1, swPort2, priority);

    uint32_t sharedInstId = bridgeGroup->CreateAtsInstance(DataRate("15Mbps"), 32768);

    // Aggregation of the two application
    bridgeGroup->BindStreamToInstance(streamHandle1, sharedInstId);
    bridgeGroup->BindStreamToInstance(streamHandle2, sharedInstId);

    Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
    app1->Setup(netEs1);
    app1->SetAttribute("Address", AddressValue(es2Mac));
    app1->SetAttribute("BurstSize", UintegerValue(2));
    app1->SetAttribute("PayloadSize", UintegerValue(1400));
    app1->SetAttribute("Period", TimeValue(MicroSeconds(10)));
    app1->SetAttribute("PCP", UintegerValue(priority));
    app1->SetAttribute("VlanID", UintegerValue(100));
    es1->AddApplication(app1);
    app1->SetStartTime(Seconds(0));
    app1->SetStopTime(MilliSeconds(0) + MicroSeconds(15));

    Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
    app2->Setup(netEs1);
    app2->SetAttribute("Address", AddressValue(es2Mac));
    app2->SetAttribute("BurstSize", UintegerValue(2));
    app2->SetAttribute("PayloadSize", UintegerValue(1400));
    app2->SetAttribute("Period", TimeValue(MicroSeconds(10)));
    app2->SetAttribute("PCP", UintegerValue(priority));
    app2->SetAttribute("VlanID", UintegerValue(200));
    es1->AddApplication(app2);
    app2->SetStartTime(Seconds(0));
    app2->SetStopTime(MilliSeconds(0) + MicroSeconds(15));

    std::cout << "Starting Correct Bridge ATS Aggregation Simulation (ES1 -> SW1 -> ES2)..." << std::endl;
    Simulator::Stop(MilliSeconds(10));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}