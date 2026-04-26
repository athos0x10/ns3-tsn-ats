/**
 * @file aerospace-redundant.cc
 * @author Arthur
 * @brief Testbench for aerospace redundant topology simulation using ns-3.
 * * @note Refactoring: NetDevice naming convention updated to
 * dev_Source_Destination to improve readability and mapping with the physical
 * topology.
 * * @version 0.3
 * @date 2026-04-25
 */
//  ==========================================================================
//  TOPOLOGY : AEROSPACE REDUNDANT NETWORK SIMULATION
//  ==========================================================================
//
//     [ Airspeed Sensor n0]       [ Altitude Sensor n1]
//             |      \         /      |
//             |       \       /       |
//             |        \     /        |
//             |         \   /         |
//      _______v_______   \ /   _______v_______
//     |     n2          |   X |      n3       |
//     |  TSN SWITCH A |  / \  |  TSN SWITCH B |
//     |_______________| /   \ |_______________|
//             |         \   /         |
//             |          \ /          |
//             |___________|___________|
//                         |
//               __________v__________
//              |          n4        |
//              |   FLIGHT CONTROL   |
//              |   COMPUTER (FCC)   |
//              |____________________|
//
//  ==========================================================================

#include "ns3/core-module.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/node.h"
#include "ns3/simulator.h"

#include "ns3/ethernet-channel.h"
#include "ns3/ethernet-generator.h"
#include "ns3/ethernet-net-device.h"
#include "ns3/switch-net-device.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RedundantSimulation");

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
static void MacTxCallback(std::string context, Ptr<const Packet> p) {
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
static void MacRxCallback(std::string context, Ptr<const Packet> p) {
  NS_LOG_INFO((Simulator::Now()).As(Time::S)
              << " \t" << context << " : Pkt #" << p->GetUid() << " received!");
}

int main(int argc, char *argv[]) {
  // Enable logging (for debug)
  LogComponentEnable("RedundantSimulation", LOG_LEVEL_INFO);

  // Node creation
  Ptr<Node> nAS0 = CreateObject<Node>();
  Names::Add("AS0", nAS0);
  Ptr<Node> nAS1 = CreateObject<Node>();
  Names::Add("AS1", nAS1);
  Ptr<Node> nSW1 = CreateObject<Node>();
  Names::Add("SW1", nSW1);
  Ptr<Node> nSW2 = CreateObject<Node>();
  Names::Add("SW2", nSW2);
  Ptr<Node> nFCC = CreateObject<Node>();
  Names::Add("FCC", nFCC);

  // NetDevice Creation : End-Systems
  // AS0 (Airspeed Sensor)
  Ptr<EthernetNetDevice> dev_AS0_SW1 = CreateObject<EthernetNetDevice>();
  nAS0->AddDevice(dev_AS0_SW1);
  Ptr<EthernetNetDevice> dev_AS0_SW2 = CreateObject<EthernetNetDevice>();
  nAS0->AddDevice(dev_AS0_SW2);

  // AS1 (Altitude Sensor)
  Ptr<EthernetNetDevice> dev_AS1_SW1 = CreateObject<EthernetNetDevice>();
  nAS1->AddDevice(dev_AS1_SW1);
  Ptr<EthernetNetDevice> dev_AS1_SW2 = CreateObject<EthernetNetDevice>();
  nAS1->AddDevice(dev_AS1_SW2);

  // FCC (Flight Control Computer)
  Ptr<EthernetNetDevice> dev_FCC_SW1 = CreateObject<EthernetNetDevice>();
  nFCC->AddDevice(dev_FCC_SW1);
  Ptr<EthernetNetDevice> dev_FCC_SW2 = CreateObject<EthernetNetDevice>();
  nFCC->AddDevice(dev_FCC_SW2);

  // NetDevice Creation : Switches
  // SW1 (Switch A)
  Ptr<EthernetNetDevice> dev_SW1_AS0 = CreateObject<EthernetNetDevice>();
  nSW1->AddDevice(dev_SW1_AS0);
  Ptr<EthernetNetDevice> dev_SW1_AS1 = CreateObject<EthernetNetDevice>();
  nSW1->AddDevice(dev_SW1_AS1);
  Ptr<EthernetNetDevice> dev_SW1_FCC = CreateObject<EthernetNetDevice>();
  nSW1->AddDevice(dev_SW1_FCC);

  // SW2 (Switch B)
  Ptr<EthernetNetDevice> dev_SW2_AS0 = CreateObject<EthernetNetDevice>();
  nSW2->AddDevice(dev_SW2_AS0);
  Ptr<EthernetNetDevice> dev_SW2_AS1 = CreateObject<EthernetNetDevice>();
  nSW2->AddDevice(dev_SW2_AS1);
  Ptr<EthernetNetDevice> dev_SW2_FCC = CreateObject<EthernetNetDevice>();
  nSW2->AddDevice(dev_SW2_FCC);

  // Connectivity (Channels)
  DataRate linkRate("100Mb/s");

  // Path to SW1
  Ptr<EthernetChannel> ch_AS0_SW1 = CreateObject<EthernetChannel>();
  dev_AS0_SW1->Attach(ch_AS0_SW1);
  dev_SW1_AS0->Attach(ch_AS0_SW1);

  Ptr<EthernetChannel> ch_AS1_SW1 = CreateObject<EthernetChannel>();
  dev_AS1_SW1->Attach(ch_AS1_SW1);
  dev_SW1_AS1->Attach(ch_AS1_SW1);

  Ptr<EthernetChannel> ch_FCC_SW1 = CreateObject<EthernetChannel>();
  dev_FCC_SW1->Attach(ch_FCC_SW1);
  dev_SW1_FCC->Attach(ch_FCC_SW1);

  // Path to SW2
  Ptr<EthernetChannel> ch_AS0_SW2 = CreateObject<EthernetChannel>();
  dev_AS0_SW2->Attach(ch_AS0_SW2);
  dev_SW2_AS0->Attach(ch_AS0_SW2);

  Ptr<EthernetChannel> ch_AS1_SW2 = CreateObject<EthernetChannel>();
  dev_AS1_SW2->Attach(ch_AS1_SW2);
  dev_SW2_AS1->Attach(ch_AS1_SW2);

  Ptr<EthernetChannel> ch_FCC_SW2 = CreateObject<EthernetChannel>();
  dev_FCC_SW2->Attach(ch_FCC_SW2);
  dev_SW2_FCC->Attach(ch_FCC_SW2);

  // Switch Stacks
  // SW1
  Ptr<SwitchNetDevice> sw1 = CreateObject<SwitchNetDevice>();
  nSW1->AddDevice(sw1);
  sw1->AddSwitchPort(dev_SW1_AS0);
  sw1->AddSwitchPort(dev_SW1_AS1);
  sw1->AddSwitchPort(dev_SW1_FCC);

  Ptr<SwitchNetDevice> sw2 = CreateObject<SwitchNetDevice>();
  nSW2->AddDevice(sw2);
  sw2->AddSwitchPort(dev_SW2_AS0);
  sw2->AddSwitchPort(dev_SW2_AS1);
  sw2->AddSwitchPort(dev_SW2_FCC);

  // Mac addresses
  dev_AS0_SW1->SetAddress(Mac48Address::Allocate());
  dev_AS0_SW2->SetAddress(Mac48Address::Allocate());
  dev_AS1_SW1->SetAddress(Mac48Address::Allocate());
  dev_AS1_SW2->SetAddress(Mac48Address::Allocate());
  dev_FCC_SW1->SetAddress(Mac48Address::Allocate());
  dev_FCC_SW2->SetAddress(Mac48Address::Allocate());

  // Queues Setup
  // Note: Standard Ethernet FIFO for now.
  Ptr<DropTailQueue<Packet>> queue;
  std::vector<Ptr<EthernetNetDevice>> allDevices = {
      dev_AS0_SW1, dev_AS0_SW2, dev_AS1_SW1, dev_AS1_SW2,
      dev_FCC_SW1, dev_FCC_SW2, dev_SW1_AS0, dev_SW1_AS1,
      dev_SW1_FCC, dev_SW2_AS0, dev_SW2_AS1, dev_SW2_FCC};
  // We use two queues one for each priority
  for (int i = 0; i < 2; i++) {
    for (auto d : allDevices) {
      queue = CreateObject<DropTailQueue<Packet>>();
      d->SetQueue(queue);
    }
  }

  // Forwarding Tables
  // Routing traffic to FCC MAC address
  Mac48Address macFCC1 = Mac48Address::ConvertFrom(dev_FCC_SW1->GetAddress());
  Mac48Address macFCC2 = Mac48Address::ConvertFrom(dev_FCC_SW2->GetAddress());
  sw1->AddForwardingTableEntry(macFCC1, 1, {dev_SW1_FCC});
  sw2->AddForwardingTableEntry(macFCC2, 1, {dev_SW2_FCC});

  // Application : AS0 -> FCC via SW1
  Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
  app0->Setup(dev_AS0_SW1);
  app0->SetAttribute("Address", AddressValue(macFCC1));
  app0->SetAttribute("BurstSize", UintegerValue(2));
  app0->SetAttribute("PayloadSize", UintegerValue(1400));
  app0->SetAttribute("Period", TimeValue(Seconds(5)));
  app0->SetAttribute("InterFrame", TimeValue(MilliSeconds(20)));
  app0->SetAttribute("Jitter", TimeValue(MilliSeconds(50)));
  app0->SetAttribute("Offset", TimeValue(Seconds(1)));
  app0->SetAttribute("VlanID", UintegerValue(1));
  app0->SetAttribute("PCP", UintegerValue(1));
  app0->SetAttribute("DEI", UintegerValue(0));
  nAS0->AddApplication(app0);
  app0->SetStartTime(Seconds(0));
  app0->SetStopTime(Seconds(10));

  // Tracing
  // Note : only debug lecture for now
  dev_AS0_SW1->TraceConnectWithoutContext(
      "MacTx", MakeBoundCallback(&MacTxCallback, "AS0_TX"));
  dev_FCC_SW1->TraceConnectWithoutContext(
      "MacRx", MakeBoundCallback(&MacRxCallback, "FCC_RX_VIA_SW1"));

  // Execute the simulation
  NS_LOG_INFO("Start of the simulation");
  Simulator::Stop(Seconds(10));
  Simulator::Run();
  Simulator::Destroy();
  NS_LOG_INFO("End of the simulation");
  return 0;
}