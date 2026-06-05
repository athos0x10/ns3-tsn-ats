// ==========================================================================
//
//       +-------------+                   +-------------+
//       |  Device A   |                   |  Device B   |
//       |   (end)     |                   |   (end)     |
//       +------+------+                   +------+------+
//              |   ^                             |   ^
//   PhyLinkA_  |   |  PhyLinkA_       PhyLinkB_  |   |  PhyLinkB_
//   SW0_AB_up  |   |  SW0_AB_down     SW0_AB_up  |   |  SW0_AB_down
//              v   |                             v   |
//       +------+---+-----------------------------+---+--+
//       |                                               |
//       |                    SW0_0                      |
//       |                  (switch)                     |
//       |                                               |
//       +--------------------+---+----------------------+
//                            |   ^
//                 PhyLinkC_  |   |  PhyLinkC_
//                 SW0_AB_up  |   |  SW0_AB_down
//                            v   |
//                     +------+---+--+
//                     |  Device C   |
//                     |   (end)     |
//                     +-------------+
//
// ==========================================================================

#include "ns3/core-module.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/node.h"
#include "ns3/simulator.h"

#include "ns3/ethernet-channel.h"
#include "ns3/ethernet-generator.h"
#include "ns3/ethernet-net-device.h"
#include "ns3/switch-net-device.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FirstNetwork");

//  --------------------------------------------------------------------------
//  NOTE: The two following functions are a re-implementation of the logic found
//  in the EDEN-Sim project (IRT Saint Exupéry). Source:
//  https://sahara.irt-saintexupery.com/embedded-systems/eden-sim
//  --------------------------------------------------------------------------

/**
 * @brief Callback function use to log tx events on Ethernet Devices.
 *
 * @param context Context string to identify the source of the event (e.g.,
 * device name).
 * @param p Pointer to the packet being transmitted.
 */
static void MacTxCallback(std::string context, Ptr<const Packet> p)
{
    NS_LOG_INFO((Simulator::Now()).As(Time::S)
                << " \t" << context << " : Pkt #" << p->GetUid() << " sent!");
}

/**
 * @brief Callback function use to log rx events on Ethernet Devices.
 *
 * @param context Context string to identify the source of the event (e.g.,
 * device name).
 * @param p Pointer to the packet being received.
 */
static void MacRxCallback(std::string context, Ptr<const Packet> p)
{
    NS_LOG_INFO((Simulator::Now()).As(Time::S)
                << " \t" << context << " : Pkt #" << p->GetUid() << " received!");
}

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("FirstNetwork", LOG_LEVEL_INFO);

    // Node creation
    Ptr<Node> A = CreateObject<Node>();
    Names::Add("A", A);
    Ptr<Node> B = CreateObject<Node>();
    Names::Add("B", B);
    Ptr<Node> C = CreateObject<Node>();
    Names::Add("C", C);
    Ptr<Node> SW0_0 = CreateObject<Node>();
    Names::Add("SW0_0", SW0_0);

    // NetDevice Creation: End Stations
    // A
    Ptr<EthernetNetDevice> dev_A_SW0 = CreateObject<EthernetNetDevice>();
    A->AddDevice(dev_A_SW0);
    // B
    Ptr<EthernetNetDevice> dev_B_SW0 = CreateObject<EthernetNetDevice>();
    B->AddDevice(dev_B_SW0);
    // C
    Ptr<EthernetNetDevice> dev_C_SW0 = CreateObject<EthernetNetDevice>();
    C->AddDevice(dev_C_SW0);

    // NetDevice Creation: Switch
    Ptr<EthernetNetDevice> dev_SW0_0_A = CreateObject<EthernetNetDevice>();
    SW0_0->AddDevice(dev_SW0_0_A);
    Ptr<EthernetNetDevice> dev_SW0_0_B = CreateObject<EthernetNetDevice>();
    SW0_0->AddDevice(dev_SW0_0_B);
    Ptr<EthernetNetDevice> dev_SW0_0_C = CreateObject<EthernetNetDevice>();
    SW0_0->AddDevice(dev_SW0_0_C);

    // Connectivity (Channels)
    DataRate linkRate("1000Mb/s");

    // A <-> SW0_0
    Ptr<EthernetChannel> ch_A_SW0_0 = CreateObject<EthernetChannel>();
    dev_A_SW0->Attach(ch_A_SW0_0);
    dev_SW0_0_A->Attach(ch_A_SW0_0);

    // B <-> SW0_0
    Ptr<EthernetChannel> ch_B_SW0_0 = CreateObject<EthernetChannel>();
    dev_B_SW0->Attach(ch_B_SW0_0);
    dev_SW0_0_B->Attach(ch_B_SW0_0);

    // C <-> SW0_0
    Ptr<EthernetChannel> ch_C_SW0_0 = CreateObject<EthernetChannel>();
    dev_C_SW0->Attach(ch_C_SW0_0);
    dev_SW0_0_C->Attach(ch_C_SW0_0);

    // Switch Stacks
    Ptr<SwitchNetDevice> sw0_0 = CreateObject<SwitchNetDevice>();
    SW0_0->AddDevice(sw0_0);
    sw0_0->AddSwitchPort(dev_SW0_0_A);
    sw0_0->AddSwitchPort(dev_SW0_0_B);
    sw0_0->AddSwitchPort(dev_SW0_0_C);

    // Mac addresses
    dev_A_SW0->SetAddress(Mac48Address::Allocate());
    dev_B_SW0->SetAddress(Mac48Address::Allocate());
    dev_C_SW0->SetAddress(Mac48Address::Allocate());

    // Queues Setup
    Ptr<DropTailQueue<Packet>> queue;
    std::vector<Ptr<EthernetNetDevice>> allDevices = {dev_A_SW0, dev_B_SW0, dev_C_SW0};
    for (auto dev : allDevices)
    {
        for (int i = 0; i < 8; i++)
        {
            queue = CreateObject<DropTailQueue<Packet>>();
            dev->SetQueue(queue);
        }
    }

    // Forwarding table setup
    Mac48Address macA = Mac48Address::ConvertFrom(dev_A_SW0->GetAddress());
    Mac48Address macB = Mac48Address::ConvertFrom(dev_B_SW0->GetAddress());
    Mac48Address macC = Mac48Address::ConvertFrom(dev_C_SW0->GetAddress());
    sw0_0->AddForwardingTableEntry(macA, 1, {dev_SW0_0_A});
    sw0_0->AddForwardingTableEntry(macB, 1, {dev_SW0_0_B});
    sw0_0->AddForwardingTableEntry(macC, 1, {dev_SW0_0_C});

    // Application setup

    // A -> C (4500 bytes, period 10000 ms)
    Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
    app0->Setup(dev_A_SW0);
    app0->SetAttribute("BurstSize", UintegerValue(4500));
    app0->SetAttribute("Period", TimeValue(MilliSeconds(10000)));
    app0->SetAttribute("Address", AddressValue(macC));
    A->AddApplication(app0);
    app0->SetStartTime(Seconds(0));
    app0->SetStopTime(Seconds(30));

    // A -> C (7500 bytes, period 20000 ms)
    Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
    app1->Setup(dev_A_SW0);
    app1->SetAttribute("BurstSize", UintegerValue(7500));
    app1->SetAttribute("Period", TimeValue(MilliSeconds(20000)));
    app1->SetAttribute("Address", AddressValue(macC));
    A->AddApplication(app1);
    app1->SetStartTime(Seconds(0));
    app1->SetStopTime(Seconds(30));

    // A -> B (9000 bytes, period 20000 ms)
    Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
    app2->Setup(dev_A_SW0);
    app2->SetAttribute("BurstSize", UintegerValue(9000));
    app2->SetAttribute("Period", TimeValue(MilliSeconds(20000)));
    app2->SetAttribute("Address", AddressValue(macB));
    A->AddApplication(app2);
    app2->SetStartTime(Seconds(0));
    app2->SetStopTime(Seconds(30));

    // B -> C (3000 bytes, period 10000 ms)
    Ptr<EthernetGenerator> app3 = CreateObject<EthernetGenerator>();
    app3->Setup(dev_B_SW0);
    app3->SetAttribute("BurstSize", UintegerValue(3000));
    app3->SetAttribute("Period", TimeValue(MilliSeconds(10000)));
    app3->SetAttribute("Address", AddressValue(macC));
    B->AddApplication(app3);
    app3->SetStartTime(Seconds(0));
    app3->SetStopTime(Seconds(30));

    // Traces Setup
    dev_A_SW0->TraceConnectWithoutContext("MacTx", MakeBoundCallback(&MacTxCallback, Names::FindName(dev_A_SW0)));
    dev_A_SW0->TraceConnectWithoutContext("MacRx", MakeBoundCallback(&MacRxCallback, Names::FindName(dev_A_SW0)));
    dev_B_SW0->TraceConnectWithoutContext("MacTx", MakeBoundCallback(&MacTxCallback, Names::FindName(dev_B_SW0)));
    dev_B_SW0->TraceConnectWithoutContext("MacRx", MakeBoundCallback(&MacRxCallback, Names::FindName(dev_B_SW0)));
    dev_C_SW0->TraceConnectWithoutContext("MacTx", MakeBoundCallback(&MacTxCallback, Names::FindName(dev_C_SW0)));
    dev_C_SW0->TraceConnectWithoutContext("MacRx", MakeBoundCallback(&MacRxCallback, Names::FindName(dev_C_SW0)));

    // Execute the simulation
    NS_LOG_INFO("Start of the simulation");
    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("End of the simulation");
    return 0;
}
