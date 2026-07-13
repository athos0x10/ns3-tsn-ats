/**
 * \file test_case_1.cc
 * \author Arthur
 * \brief Example of a test case for the industrial simulation.
 * \version 0.1
 * \date 2026-07-07
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/tsn-module.h"
#include "ns3/ethernet-module.h"
#include "ns3/traffic-generator-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TestCase1");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("TestCase1", LOG_LEVEL_INFO);
    LogComponentEnable("EthernetGenerator", LOG_LEVEL_INFO);

    // ---------------------
    // --- topology.json ---
    // ---------------------

    // Creation of the nodes
    // --- Switches ---

    // Create the node object and give a name.
    // It can be either an end-station or a switch
    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Names::Add("SW0", n0);

    Ptr<TsnNode> n1 = CreateObject<TsnNode>();
    Names::Add("SW1", n1);

    Ptr<TsnNode> n2 = CreateObject<TsnNode>();
    Names::Add("SW2", n2);

    Ptr<TsnNode> n3 = CreateObject<TsnNode>();
    Names::Add("SW3", n3);

    // --- End Systems ---
    Ptr<TsnNode> es0 = CreateObject<TsnNode>();
    Names::Add("ES0", es0);

    Ptr<TsnNode> es1 = CreateObject<TsnNode>();
    Names::Add("ES1", es1);

    Ptr<TsnNode> es2 = CreateObject<TsnNode>();
    Names::Add("ES2", es2);

    Ptr<TsnNode> es3 = CreateObject<TsnNode>();
    Names::Add("ES3", es3);

    Ptr<TsnNode> es4 = CreateObject<TsnNode>();
    Names::Add("ES4", es4);

    Ptr<TsnNode> es5 = CreateObject<TsnNode>();
    Names::Add("ES5", es5);

    Ptr<TsnNode> es6 = CreateObject<TsnNode>();
    Names::Add("ES6", es6);

    Ptr<TsnNode> es7 = CreateObject<TsnNode>();
    Names::Add("ES7", es7);

    // Create and add a netDevice to each node
    // --- SW0 Ports ---

    // A TsnNetDevice is a port for a TsnNode
    // So we create the object, we associate it to a TsnNode
    // And we give a name.
    // It could have been done with a loop
    Ptr<TsnNetDevice> sw0_p0 = CreateObject<TsnNetDevice>();
    n0->AddDevice(sw0_p0);
    Names::Add("SW0#00", sw0_p0);

    Ptr<TsnNetDevice> sw0_p1 = CreateObject<TsnNetDevice>();
    n0->AddDevice(sw0_p1);
    Names::Add("SW0#01", sw0_p1);

    Ptr<TsnNetDevice> sw0_p2 = CreateObject<TsnNetDevice>();
    n0->AddDevice(sw0_p2);
    Names::Add("SW0#02", sw0_p2);

    Ptr<TsnNetDevice> sw0_p3 = CreateObject<TsnNetDevice>();
    n0->AddDevice(sw0_p3);
    Names::Add("SW0#03", sw0_p3);

    Ptr<TsnNetDevice> sw0_p6 = CreateObject<TsnNetDevice>();
    n0->AddDevice(sw0_p6);
    Names::Add("SW0#06", sw0_p6);

    Ptr<TsnNetDevice> sw0_p7 = CreateObject<TsnNetDevice>();
    n0->AddDevice(sw0_p7);
    Names::Add("SW0#07", sw0_p7);

    // --- SW1 Ports ---
    Ptr<TsnNetDevice> sw1_p0 = CreateObject<TsnNetDevice>();
    n1->AddDevice(sw1_p0);
    Names::Add("SW1#00", sw1_p0);

    Ptr<TsnNetDevice> sw1_p2 = CreateObject<TsnNetDevice>();
    n1->AddDevice(sw1_p2);
    Names::Add("SW1#02", sw1_p2);

    Ptr<TsnNetDevice> sw1_p3 = CreateObject<TsnNetDevice>();
    n1->AddDevice(sw1_p3);
    Names::Add("SW1#03", sw1_p3);

    Ptr<TsnNetDevice> sw1_p6 = CreateObject<TsnNetDevice>();
    n1->AddDevice(sw1_p6);
    Names::Add("SW1#06", sw1_p6);

    Ptr<TsnNetDevice> sw1_p7 = CreateObject<TsnNetDevice>();
    n1->AddDevice(sw1_p7);
    Names::Add("SW1#07", sw1_p7);

    // --- SW2 Ports ---
    Ptr<TsnNetDevice> sw2_p2 = CreateObject<TsnNetDevice>();
    n2->AddDevice(sw2_p2);
    Names::Add("SW2#02", sw2_p2);

    Ptr<TsnNetDevice> sw2_p3 = CreateObject<TsnNetDevice>();
    n2->AddDevice(sw2_p3);
    Names::Add("SW2#03", sw2_p3);

    // --- SW3 Ports ---
    Ptr<TsnNetDevice> sw3_p2 = CreateObject<TsnNetDevice>();
    n3->AddDevice(sw3_p2);
    Names::Add("SW3#02", sw3_p2);

    Ptr<TsnNetDevice> sw3_p3 = CreateObject<TsnNetDevice>();
    n3->AddDevice(sw3_p3);
    Names::Add("SW3#03", sw3_p3);

    Ptr<TsnNetDevice> sw3_p6 = CreateObject<TsnNetDevice>();
    n3->AddDevice(sw3_p6);
    Names::Add("SW3#06", sw3_p6);

    // --- End Systems Ports (All use Port 0) ---
    Ptr<TsnNetDevice> es0_p0 = CreateObject<TsnNetDevice>();
    es0->AddDevice(es0_p0);
    Names::Add("ES0#00", es0_p0);

    Ptr<TsnNetDevice> es1_p0 = CreateObject<TsnNetDevice>();
    es1->AddDevice(es1_p0);
    Names::Add("ES1#00", es1_p0);

    Ptr<TsnNetDevice> es2_p0 = CreateObject<TsnNetDevice>();
    es2->AddDevice(es2_p0);
    Names::Add("ES2#00", es2_p0);

    Ptr<TsnNetDevice> es3_p0 = CreateObject<TsnNetDevice>();
    es3->AddDevice(es3_p0);
    Names::Add("ES3#00", es3_p0);

    Ptr<TsnNetDevice> es4_p0 = CreateObject<TsnNetDevice>();
    es4->AddDevice(es4_p0);
    Names::Add("ES4#00", es4_p0);

    Ptr<TsnNetDevice> es5_p0 = CreateObject<TsnNetDevice>();
    es5->AddDevice(es5_p0);
    Names::Add("ES5#00", es5_p0);

    Ptr<TsnNetDevice> es6_p0 = CreateObject<TsnNetDevice>();
    es6->AddDevice(es6_p0);
    Names::Add("ES6#00", es6_p0);

    Ptr<TsnNetDevice> es7_p0 = CreateObject<TsnNetDevice>();
    es7->AddDevice(es7_p0);
    Names::Add("ES7#00", es7_p0);

    // Switch device configuration
    // --- Creation and Configuration of SwitchNetDevices ---
    // SW0

    // Now, we create specificaly a switch
    // Some attributes are available you must attach a Node
    // and give all the TsnNetDevice.
    Ptr<SwitchNetDevice> sw0 = CreateObject<SwitchNetDevice>();
    sw0->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(2)));
    sw0->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(5)));
    n0->AddDevice(sw0);
    sw0->AddSwitchPort(sw0_p0);
    sw0->AddSwitchPort(sw0_p1);
    sw0->AddSwitchPort(sw0_p2);
    sw0->AddSwitchPort(sw0_p3);
    sw0->AddSwitchPort(sw0_p6);
    sw0->AddSwitchPort(sw0_p7);

    // SW1
    Ptr<SwitchNetDevice> sw1 = CreateObject<SwitchNetDevice>();
    sw1->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(2)));
    sw1->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(5)));
    n1->AddDevice(sw1);
    sw1->AddSwitchPort(sw1_p0);
    sw1->AddSwitchPort(sw1_p2);
    sw1->AddSwitchPort(sw1_p3);
    sw1->AddSwitchPort(sw1_p6);
    sw1->AddSwitchPort(sw1_p7);

    // SW2
    Ptr<SwitchNetDevice> sw2 = CreateObject<SwitchNetDevice>();
    sw2->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(2)));
    sw2->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(5)));
    n2->AddDevice(sw2);
    sw2->AddSwitchPort(sw2_p2);
    sw2->AddSwitchPort(sw2_p3);

    // SW3
    Ptr<SwitchNetDevice> sw3 = CreateObject<SwitchNetDevice>();
    sw3->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(2)));
    sw3->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(5)));
    n3->AddDevice(sw3);
    sw3->AddSwitchPort(sw3_p2);
    sw3->AddSwitchPort(sw3_p3);
    sw3->AddSwitchPort(sw3_p6);

    // Data rate configuration
    // --- 1 Gbps Ports (Switches backbone) ---

    // We could have done this before but I think it's clearer this way.
    sw0_p2->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    sw2_p2->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    sw0_p3->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    sw1_p2->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    sw1_p3->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    sw3_p2->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    sw2_p3->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    sw3_p3->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    // --- 100 Mbps Ports (End Systems & Switch Access Ports) ---
    // SW0 <-> ES
    sw0_p6->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es0_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

    sw0_p7->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es2_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

    sw0_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es4_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

    sw0_p1->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es5_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

    // SW1 <-> ES
    sw1_p6->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es3_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

    sw1_p7->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es6_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

    sw1_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es7_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

    // SW3 <-> ES
    sw3_p6->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es1_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

    // Create full-duplex channel
    // --- SW0 <-> SW2 (Delay: 44.775 us) ---

    // This is where we create the link between two ports
    Ptr<EthernetChannel> ch_sw0_sw2 = CreateObject<EthernetChannel>();
    ch_sw0_sw2->SetAttribute("Delay", TimeValue(MicroSeconds(44.775)));
    sw0_p2->Attach(ch_sw0_sw2);
    sw2_p2->Attach(ch_sw0_sw2);

    // --- SW0 <-> SW1 (Delay: 16.982 us) ---
    Ptr<EthernetChannel> ch_sw0_sw1 = CreateObject<EthernetChannel>();
    ch_sw0_sw1->SetAttribute("Delay", TimeValue(MicroSeconds(16.982)));
    sw0_p3->Attach(ch_sw0_sw1);
    sw1_p2->Attach(ch_sw0_sw1);

    // --- SW0 <-> ES0 (Delay: 7.119 us) ---
    Ptr<EthernetChannel> ch_sw0_es0 = CreateObject<EthernetChannel>();
    ch_sw0_es0->SetAttribute("Delay", TimeValue(MicroSeconds(7.119)));
    sw0_p6->Attach(ch_sw0_es0);
    es0_p0->Attach(ch_sw0_es0);

    // --- SW0 <-> ES2 (Delay: 3.037 us) ---
    Ptr<EthernetChannel> ch_sw0_es2 = CreateObject<EthernetChannel>();
    ch_sw0_es2->SetAttribute("Delay", TimeValue(MicroSeconds(3.037)));
    sw0_p7->Attach(ch_sw0_es2);
    es2_p0->Attach(ch_sw0_es2);

    // --- SW0 <-> ES4 (Delay: 2.33 us) ---
    Ptr<EthernetChannel> ch_sw0_es4 = CreateObject<EthernetChannel>();
    ch_sw0_es4->SetAttribute("Delay", TimeValue(MicroSeconds(2.33)));
    sw0_p0->Attach(ch_sw0_es4);
    es4_p0->Attach(ch_sw0_es4);

    // --- SW0 <-> ES5 (Delay: 1.791 us) ---
    Ptr<EthernetChannel> ch_sw0_es5 = CreateObject<EthernetChannel>();
    ch_sw0_es5->SetAttribute("Delay", TimeValue(MicroSeconds(1.791)));
    sw0_p1->Attach(ch_sw0_es5);
    es5_p0->Attach(ch_sw0_es5);

    // --- SW1 <-> SW3 (Delay: 15.46 us) ---
    Ptr<EthernetChannel> ch_sw1_sw3 = CreateObject<EthernetChannel>();
    ch_sw1_sw3->SetAttribute("Delay", TimeValue(MicroSeconds(15.46)));
    sw1_p3->Attach(ch_sw1_sw3);
    sw3_p2->Attach(ch_sw1_sw3);

    // --- SW1 <-> ES3 (Delay: 6.638 us) ---
    Ptr<EthernetChannel> ch_sw1_es3 = CreateObject<EthernetChannel>();
    ch_sw1_es3->SetAttribute("Delay", TimeValue(MicroSeconds(6.638)));
    sw1_p6->Attach(ch_sw1_es3);
    es3_p0->Attach(ch_sw1_es3);

    // --- SW1 <-> ES6 (Delay: 2.929 us) ---
    Ptr<EthernetChannel> ch_sw1_es6 = CreateObject<EthernetChannel>();
    ch_sw1_es6->SetAttribute("Delay", TimeValue(MicroSeconds(2.929)));
    sw1_p7->Attach(ch_sw1_es6);
    es6_p0->Attach(ch_sw1_es6);

    // --- SW1 <-> ES7 (Delay: 4.983 us) ---
    Ptr<EthernetChannel> ch_sw1_es7 = CreateObject<EthernetChannel>();
    ch_sw1_es7->SetAttribute("Delay", TimeValue(MicroSeconds(4.983)));
    sw1_p0->Attach(ch_sw1_es7);
    es7_p0->Attach(ch_sw1_es7);

    // --- SW2 <-> SW3 (Delay: 29.913 us) ---
    Ptr<EthernetChannel> ch_sw2_sw3 = CreateObject<EthernetChannel>();
    ch_sw2_sw3->SetAttribute("Delay", TimeValue(MicroSeconds(29.913)));
    sw2_p3->Attach(ch_sw2_sw3);
    sw3_p3->Attach(ch_sw2_sw3);

    // --- SW3 <-> ES1 (Delay: 2.057 us) ---
    Ptr<EthernetChannel> ch_sw3_es1 = CreateObject<EthernetChannel>();
    ch_sw3_es1->SetAttribute("Delay", TimeValue(MicroSeconds(2.057)));
    sw3_p6->Attach(ch_sw3_es1);
    es1_p0->Attach(ch_sw3_es1);

    // Allocate a Mac address and create a FIFO (for the output port)
    // for each netDevice.

    // --- Switch Ports Vector for Automation ---
    std::vector<Ptr<TsnNetDevice>> sw_ports = {
        sw0_p0, sw0_p1, sw0_p2, sw0_p3, sw0_p6, sw0_p7,
        sw1_p0, sw1_p2, sw1_p3, sw1_p6, sw1_p7,
        sw2_p2, sw2_p3,
        sw3_p2, sw3_p3, sw3_p6};

    // Configure Switch Ports: Allocate Mac Address + 8 Queues per port
    for (auto &port : sw_ports)
    {
        port->SetAddress(Mac48Address::Allocate());
        for (int i = 0; i < 8; ++i)
        {
            port->SetQueue(CreateObject<DropTailQueue<Packet>>());
        }
    }

    // --- End System Ports Vector ---
    std::vector<Ptr<TsnNetDevice>> es_ports = {
        es0_p0, es1_p0, es2_p0, es3_p0, es4_p0, es5_p0, es6_p0, es7_p0};

    // Vector to store and keep the MAC addresses of the End Systems for your applications
    std::vector<Mac48Address> esMacs(8);

    // Configure End System Ports: Allocate, save the MAC, and add 8 Queues
    for (size_t i = 0; i < es_ports.size(); ++i)
    {
        esMacs[i] = Mac48Address::Allocate();
        es_ports[i]->SetAddress(esMacs[i]);

        for (int q = 0; q < 8; ++q)
        {
            es_ports[i]->SetQueue(CreateObject<DropTailQueue<Packet>>());
        }
    }

    // -------------------
    // --- routes.json ---
    // -------------------

    // FLOW 0 (ES7 -> ES3, ES6, ES2)
    // For the forwarding table we give (destMac, VlanId, TsnNetDevice)
    sw1->AddForwardingTableEntry(esMacs[3], 1, std::vector<Ptr<NetDevice>>{sw1_p6});
    sw1->AddForwardingTableEntry(esMacs[6], 1, std::vector<Ptr<NetDevice>>{sw1_p7});
    sw1->AddForwardingTableEntry(esMacs[2], 1, std::vector<Ptr<NetDevice>>{sw1_p2});
    sw0->AddForwardingTableEntry(esMacs[2], 1, std::vector<Ptr<NetDevice>>{sw0_p7});

    // FLOW 1 (ES4 -> ES7)
    sw0->AddForwardingTableEntry(esMacs[7], 1, std::vector<Ptr<NetDevice>>{sw0_p3});
    sw1->AddForwardingTableEntry(esMacs[7], 1, std::vector<Ptr<NetDevice>>{sw1_p0});

    // FLOW 2 (ES3 -> ES4)
    sw1->AddForwardingTableEntry(esMacs[4], 1, std::vector<Ptr<NetDevice>>{sw1_p2});
    sw0->AddForwardingTableEntry(esMacs[4], 1, std::vector<Ptr<NetDevice>>{sw0_p0});

    // FLOW 3 (ES7 -> ES2, ES5)
    sw1->AddForwardingTableEntry(esMacs[2], 1, std::vector<Ptr<NetDevice>>{sw1_p2});
    sw1->AddForwardingTableEntry(esMacs[5], 1, std::vector<Ptr<NetDevice>>{sw1_p2});
    sw0->AddForwardingTableEntry(esMacs[2], 1, std::vector<Ptr<NetDevice>>{sw0_p7});
    sw0->AddForwardingTableEntry(esMacs[5], 1, std::vector<Ptr<NetDevice>>{sw0_p1});

    // FLOW 4 (ES6 -> ES3, ES0, ES4)
    sw1->AddForwardingTableEntry(esMacs[3], 1, std::vector<Ptr<NetDevice>>{sw1_p6});
    sw1->AddForwardingTableEntry(esMacs[0], 1, std::vector<Ptr<NetDevice>>{sw1_p2});
    sw1->AddForwardingTableEntry(esMacs[4], 1, std::vector<Ptr<NetDevice>>{sw1_p2});
    sw0->AddForwardingTableEntry(esMacs[0], 1, std::vector<Ptr<NetDevice>>{sw0_p6});
    sw0->AddForwardingTableEntry(esMacs[4], 1, std::vector<Ptr<NetDevice>>{sw0_p0});

    // ------------------
    // -- streams.json --
    // ------------------

    // Application Container to hold all generators
    // This container is convenient here because we do not specify a start time for each application.
    ApplicationContainer apps;

    // ==========================================
    // STREAM 0 (Source: ES7 -> Dest: ES3, ES6, ES2)
    // ISOCHRONOUS, PCP: 7, Size: 58B, Period: 1000us
    // ==========================================
    std::vector<int> stream0_dests = {3, 6, 2}; // ES3, ES6, ES2
    for (int dest_idx : stream0_dests)
    {
        Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
        app0->Setup(es7_p0); // Calqué sur le modèle fonctionnel
        app0->SetAttribute("Address", AddressValue(esMacs[dest_idx]));
        app0->SetAttribute("PayloadSize", UintegerValue(58));
        app0->SetAttribute("Period", TimeValue(MicroSeconds(1000)));
        app0->SetAttribute("PCP", UintegerValue(7));
        app0->SetAttribute("VlanID", UintegerValue(1));
        es7->AddApplication(app0);
        apps.Add(app0);
    }

    // ==========================================
    // STREAM 1 (Source: ES4 -> Dest: ES7)
    // ISOCHRONOUS, PCP: 7, Size: 68B, Period: 2000us
    // ==========================================
    Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
    app1->Setup(es4_p0);
    app1->SetAttribute("Address", AddressValue(esMacs[7])); // ES7
    app1->SetAttribute("PayloadSize", UintegerValue(68));
    app1->SetAttribute("Period", TimeValue(MicroSeconds(2000)));
    app1->SetAttribute("PCP", UintegerValue(7));
    app1->SetAttribute("VlanID", UintegerValue(1));
    es4->AddApplication(app1);
    apps.Add(app1);

    // ==========================================
    // STREAM 2 (Source: ES3 -> Dest: ES4)
    // ISOCHRONOUS, PCP: 7, Size: 64B, Period: 500us
    // ==========================================
    Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
    app2->Setup(es3_p0);
    app2->SetAttribute("Address", AddressValue(esMacs[4])); // ES4
    app2->SetAttribute("PayloadSize", UintegerValue(64));
    app2->SetAttribute("Period", TimeValue(MicroSeconds(500)));
    app2->SetAttribute("PCP", UintegerValue(7));
    app2->SetAttribute("VlanID", UintegerValue(1));
    es3->AddApplication(app2);
    apps.Add(app2);

    // ==========================================
    // STREAM 3 (Source: ES7 -> Dest: ES2, ES5)
    // BEST-EFFORT, PCP: 0, Size: 1500B, Period: null (Continuous/Asynchronous)
    // ==========================================
    std::vector<int> stream3_dests = {2, 5}; // ES2, ES5
    for (int dest_idx : stream3_dests)
    {
        Ptr<EthernetGenerator> app3 = CreateObject<EthernetGenerator>();
        app3->Setup(es7_p0); // Calqué sur le modèle fonctionnel
        app3->SetAttribute("Address", AddressValue(esMacs[dest_idx]));
        app3->SetAttribute("PayloadSize", UintegerValue(1500));
        app3->SetAttribute("Period", TimeValue(Seconds(1))); // Default period for continuous BE
        app3->SetAttribute("PCP", UintegerValue(0));
        app3->SetAttribute("VlanID", UintegerValue(1));
        es7->AddApplication(app3);
        apps.Add(app3);
    }

    // ==========================================
    // STREAM 4 (Source: ES6 -> Dest: ES3, ES0, ES4)
    // BEST-EFFORT, PCP: 0, Size: 1500B, Period: null (Continuous/Asynchronous)
    // ==========================================
    std::vector<int> stream4_dests = {3, 0, 4}; // ES3, ES0, ES4
    for (int dest_idx : stream4_dests)
    {
        Ptr<EthernetGenerator> app4 = CreateObject<EthernetGenerator>();
        app4->Setup(es6_p0);
        app4->SetAttribute("Address", AddressValue(esMacs[dest_idx]));
        app4->SetAttribute("PayloadSize", UintegerValue(1500));
        app4->SetAttribute("Period", TimeValue(Seconds(1))); // Default period for continuous BE
        app4->SetAttribute("PCP", UintegerValue(0));
        app4->SetAttribute("VlanID", UintegerValue(1));
        es6->AddApplication(app4);
        apps.Add(app4);
    }

    // Start all traffic applications at 1 second and stop at 10 seconds
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();

    // Clean up simulation state and memory
    Simulator::Destroy();

    return 0;
}