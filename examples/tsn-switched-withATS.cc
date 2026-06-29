#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/command-line.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/trace-helper.h"

#include "ns3/ethernet-channel.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/tsn-module.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ethernet-generator.h"

/**
 * \file
 *
 * Example of the use of ATS on a network composed of three end-stations
 * and connected by a 1Gb/s full duplex network through a switch. SW1 is the
 * the only one with ATS enble on his output port.
 *  ES1 === SW1 === ES2
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Example");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("Example", LOG_LEVEL_INFO);
    LogComponentEnable("TsnNode", LOG_LEVEL_INFO);
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes
    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Names::Add("ES1", n0);
    Ptr<TsnNode> n1 = CreateObject<TsnNode>();
    Names::Add("ES2", n1);
    Ptr<TsnNode> n2 = CreateObject<TsnNode>();
    Names::Add("SW1", n2);

    // Clock configuration
    Ptr<Clock> clock0 = CreateObject<Clock>();
    Ptr<Clock> clock1 = CreateObject<Clock>();
    Ptr<Clock> clock2 = CreateObject<Clock>();
    n0->SetMainClock(clock0);
    n1->SetMainClock(clock1);
    n2->SetMainClock(clock2);
    n0->AddClock(clock0);
    n1->AddClock(clock1);
    n2->AddClock(clock2);
    n0->setActiveClock(0);
    n1->setActiveClock(0);
    n2->setActiveClock(0);

    // Create and add a netDevice to each node
    Ptr<TsnNetDevice> net0 = CreateObject<TsnNetDevice>();
    net0->SetAttribute("DataRate", DataRateValue(DataRate("1Gb/s")));
    n0->AddDevice(net0);
    Names::Add("ES1#01", net0);
    Ptr<TsnNetDevice> net1 = CreateObject<TsnNetDevice>();
    net1->SetAttribute("DataRate", DataRateValue(DataRate("1Gb/s")));
    n1->AddDevice(net1);
    Names::Add("ES2#01", net1);

    Ptr<TsnNetDevice> net2_0 = CreateObject<TsnNetDevice>();
    net2_0->SetAttribute("DataRate", DataRateValue(DataRate("1Gb/s")));
    n2->AddDevice(net2_0);
    Names::Add("SW1#01", net2_0);
    Ptr<TsnNetDevice> net2_1 = CreateObject<TsnNetDevice>();
    net2_1->SetAttribute("DataRate", DataRateValue(DataRate("1Gb/s")));
    n2->AddDevice(net2_1);
    Names::Add("SW1#02", net2_1);

    for (int i = 0; i < 8; i++)
    {
        net0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net2_0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net2_1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // Create and add a switch net device to the switch node
    Ptr<SwitchNetDevice> sw = CreateObject<SwitchNetDevice>();
    sw->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(10)));
    sw->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(10)));
    n2->AddDevice(sw);
    sw->AddSwitchPort(net2_0);
    sw->AddSwitchPort(net2_1);

    // Create Ethernet Channel and attach it to the netDevices
    Ptr<EthernetChannel> l0 = CreateObject<EthernetChannel>();
    l0->SetAttribute("Delay", TimeValue(Time(NanoSeconds(200))));
    net0->Attach(l0);
    net2_0->Attach(l0);
    Ptr<EthernetChannel> l1 = CreateObject<EthernetChannel>();
    l1->SetAttribute("Delay", TimeValue(Time(NanoSeconds(200))));
    net1->Attach(l1);
    net2_1->Attach(l1);

    // Allocate a Mac address
    net0->SetAddress(Mac48Address::Allocate());
    net1->SetAddress(Mac48Address::Allocate());
    sw->SetAddress(Mac48Address::Allocate());
    net2_0->SetAddress(Mac48Address::Allocate());
    net2_1->SetAddress(Mac48Address::Allocate());

    // Configure ATS
    net2_1->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> ats = net2_1->GetAts();
    ats->SetClock(clock2);
    ats->SetAttribute("MaxResidenceTime", TimeValue(Seconds(1)));

    Ptr<AtsSchedulerGroup> ats_group = ats->GetGroupForBridge(net2_0, net2_1, 2);
    ats_group->SetAttribute("DefaultCir", DataRateValue(DataRate("10Mbps")));
    ats_group->SetAttribute("DefaultCbs", UintegerValue(16384));

    // Appliction configuration
    // Fix: Declare macDest before configuring the application
    Mac48Address macDest = Mac48Address::ConvertFrom(net1->GetAddress());
    sw->AddForwardingTableEntry(macDest, 100, {net2_1});
    Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
    app0->Setup(net0);
    app0->SetAttribute("Address", AddressValue(macDest));
    app0->SetAttribute("BurstSize", UintegerValue(1));
    app0->SetAttribute("PayloadSize", UintegerValue(1400)); // Total frame = 1422 bytes
    app0->SetAttribute("Period", TimeValue(MilliSeconds(5)));
    app0->SetAttribute("PCP", UintegerValue(2));
    app0->SetAttribute("VlanID", UintegerValue(100));
    n0->AddApplication(app0);
    app0->SetStartTime(MilliSeconds(0));
    app0->SetStopTime(MilliSeconds(20));

    // Execute the simulation
    Simulator::Stop(Seconds(3));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}