#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/command-line.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/drop-tail-queue.h"
#include <bitset>

#include "ns3/ethernet-channel.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/tsn-module.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ethernet-generator.h"

/**
 * \file
 *
 * Example of the use of tsn-node.cc tsn-net-device.cc ethernet-channel.cc
 * on a network composed of two end-stations connected by a 100Mb/s
 * full duplex link with TAS on ES1 port
 *  ES1 ====== ES2
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Example");

// A callback to log the pkt reception
static void
MacRxCallback(std::string context, Ptr<const Packet> p)
{
    NS_LOG_INFO((Simulator::Now()).As(Time::S) << " \t" << context << " : Pkt #" << p->GetUid() << " received !");
}

// A callback to log the pkt emission
static void
PhyTxCallback(std::string context, Ptr<const Packet> p)
{
    NS_LOG_INFO((Simulator::Now()).As(Time::S) << " \t" << context << " : Pkt #" << p->GetUid() << " begin transmission !");
}

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("Example", LOG_LEVEL_INFO);
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);
    LogComponentEnable("EthernetGenerator", LOG_LEVEL_INFO);
    LogComponentEnable("Clock", LOG_LEVEL_INFO);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create two nodes
    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Names::Add("ES1", n0);
    Ptr<TsnNode> n1 = CreateObject<TsnNode>();
    Names::Add("ES2", n1);

    // Clock configuration
    Ptr<Clock> clock0 = CreateObject<Clock>();
    Ptr<Clock> clock1 = CreateObject<Clock>();
    n0->AddClock(clock0);
    n1->AddClock(clock1);

    // Create and add a netDevice to each node
    Ptr<TsnNetDevice> net0 = CreateObject<TsnNetDevice>();
    net0->SetAttribute("DataRate", DataRateValue(DataRate("100Mb/s")));
    n0->AddDevice(net0);
    Names::Add("ES1#01", net0);
    Ptr<TsnNetDevice> net1 = CreateObject<TsnNetDevice>();
    net1->SetAttribute("DataRate", DataRateValue(DataRate("100Mb/s")));
    n1->AddDevice(net1);
    Names::Add("ES2#01", net1);

    // Create a Ethernet Channel and attach it two the two netDevices
    Ptr<EthernetChannel> channel = CreateObject<EthernetChannel>();
    net0->Attach(channel);
    net1->Attach(channel);

    // Allocate a Mac address
    net0->SetAddress(Mac48Address::Allocate());
    net1->SetAddress(Mac48Address::Allocate());

    // Create and add eight FIFO on each net device
    for (int i = 0; i < 8; i++)
    {
        net0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // Activate the ATS subsystem on the Egress End-Station port
    net0->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> atsEngine = net0->GetAts();
    atsEngine->SetClock(clock0);
    atsEngine->SetAttribute("MaxResidenceTime", TimeValue(Seconds(1)));

    // Application description
    Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
    app0->Setup(net0);
    app0->SetAttribute("BurstSize", UintegerValue(1));
    app0->SetAttribute("PayloadSize", UintegerValue(1400));
    app0->SetAttribute("Period", TimeValue(MilliSeconds(15)));
    app0->SetAttribute("PCP", UintegerValue(1));
    app0->SetAttribute("VlanID", UintegerValue(100));
    n0->AddApplication(app0);
    app0->SetStartTime(Seconds(0));
    app0->SetStopTime(Seconds(4));

    // Callback to display the packet transmitted and received log
    net0->TraceConnectWithoutContext("PhyTxBegin", MakeBoundCallback(&PhyTxCallback, Names::FindName(n0) + ":" + Names::FindName(net0)));
    net1->TraceConnectWithoutContext("MacRx", MakeBoundCallback(&MacRxCallback, Names::FindName(n1) + ":" + Names::FindName(net1)));

    // Execute the simulation
    Simulator::Stop(MilliSeconds(80));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}