/**
 * @file worst-case-experimentation.cc
 * @author Arthur
 * @brief This file contains the worst-case experiment.
 *
 * @date 2026-07-16
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/tsn-module.h"
#include "ns3/ethernet-module.h"
#include "ns3/traffic-generator-module.h"
#include "ns3/ethernet-header2.h"
#include <fstream>
#include <random>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WorstCaseExperimentation");

/**
 * @brief Automatically generates a random background flow on a source node.
 *
 * @param srcNode The source TSN End Station node (e.g., ES1, ES2, ES4, ES5)
 * @param srcDevice The network device associated with the source node's egress port
 * @param destNode The destination TSN End Station node (e.g., ES6, ES7)
 * @return double The generated throughput R_i of this flow in bits per second (bps)
 */
double CreateRandomBackgroundFlow(Ptr<TsnNode> srcNode, Ptr<TsnNetDevice> srcDevice, Ptr<TsnNode> destNode)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist_N(1, 10);
    std::uniform_int_distribution<int> dist_L(100, 1000);
    std::uniform_int_distribution<int> dist_priority(6, 7);

    // Randomly select flow parameters
    int Ni = dist_N(gen);
    int Li = dist_L(gen);
    int priority = dist_priority(gen);

    // Formula: T_IPG = Ni µs + U(0, Ni * 3µs)
    Time period = MicroSeconds(2.5 * Ni);
    Time jitter = MicroSeconds(1.5 * Ni);

    // Calculate theoretical throughput R_i (bps)
    double Ri_bps = (Li * 8.0) / (2.5 * Ni * 1e-6);

    // Instantiate and configure the EthernetGenerator
    Ptr<EthernetGenerator> app = CreateObject<EthernetGenerator>();
    app->Setup(srcDevice);
    app->SetAttribute("Address", Mac48AddressValue(destNode->GetAddress()));
    app->SetAttribute("PayloadSize", UintegerValue(Li));
    app->SetAttribute("BurstSize", UintegerValue(1));
    app->SetAttribute("Period", TimeValue(period));
    app->SetAttribute("Jitter", TimeValue(jitter));
    app->SetAttribute("PCP", UintegerValue(priority));
    app->SetAttribute("VlanID", UintegerValue(100)); // Default VLAN for background traffic

    // Bind the application to the source node
    srcNode->AddApplication(app);
    app->SetStartTime(Seconds(0.0));

    return Ri_bps;
}

int main(int argc, char *argv[])
{
    std::string scenario = "S1.1.1";
    double targetLoad = 0.10;
    double simTime = 1.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("scenario", "Scenario to run (S1.1.1, S1.1.2, S1.1.3, S1.1.4, S1.2.*, S2.*)", scenario);
    cmd.AddValue("load", "Target load of the transit link between 0.0 and 1.0 (e.g., 0.15 for 15%)", targetLoad);
    cmd.AddValue("simTime", "Total simulation time", simTime);
    cmd.Parse(argc, argv);

    LogComponentEnable("WorstCaseExperimentation", LOG_LEVEL_INFO);

    // Creation of the nodes
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

    Ptr<TsnNode> sw1 = CreateObject<TsnNode>();
    Names::Add("SW1", sw1);

    Ptr<TsnNode> sw2 = CreateObject<TsnNode>();
    Names::Add("SW2", sw2);

    // Creation of the ports
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

    Ptr<TsnNetDevice> sw1_p0 = CreateObject<TsnNetDevice>();
    sw1->AddDevice(sw1_p0);
    Names::Add("SW1#00", sw1_p0);

    Ptr<TsnNetDevice> sw1_p1 = CreateObject<TsnNetDevice>();
    sw1->AddDevice(sw1_p1);
    Names::Add("SW1#01", sw1_p1);

    Ptr<TsnNetDevice> sw1_p2 = CreateObject<TsnNetDevice>();
    sw1->AddDevice(sw1_p2);
    Names::Add("SW1#02", sw1_p2);

    Ptr<TsnNetDevice> sw1_p3 = CreateObject<TsnNetDevice>();
    sw1->AddDevice(sw1_p3);
    Names::Add("SW1#03", sw1_p3);

    Ptr<TsnNetDevice> sw2_p0 = CreateObject<TsnNetDevice>();
    sw2->AddDevice(sw2_p0);
    Names::Add("SW2#00", sw2_p0);

    Ptr<TsnNetDevice> sw2_p1 = CreateObject<TsnNetDevice>();
    sw2->AddDevice(sw2_p1);
    Names::Add("SW2#01", sw2_p1);

    Ptr<TsnNetDevice> sw2_p2 = CreateObject<TsnNetDevice>();
    sw2->AddDevice(sw2_p2);
    Names::Add("SW2#02", sw2_p2);

    Ptr<TsnNetDevice> sw2_p3 = CreateObject<TsnNetDevice>();
    sw2->AddDevice(sw2_p3);
    Names::Add("SW2#03", sw2_p3);

    Ptr<TsnNetDevice> sw2_p4 = CreateObject<TsnNetDevice>();
    sw2->AddDevice(sw2_p4);
    Names::Add("SW2#04", sw2_p4);

    // Clock configuration
    Ptr<Clock> clock0 = CreateObject<Clock>();
    sw1->AddClock(clock0);
    Ptr<Clock> clock1 = CreateObject<Clock>();
    sw2->AddClock(clock1);

    // Switch Device Configuration
    Ptr<SwitchNetDevice> sw1_dev = CreateObject<SwitchNetDevice>();
    sw1_dev->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(2)));
    sw1_dev->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(5)));
    sw1->AddDevice(sw1_dev);
    sw1_dev->AddSwitchPort(sw1_p0);
    sw1_dev->AddSwitchPort(sw1_p1);
    sw1_dev->AddSwitchPort(sw1_p2);
    sw1_dev->AddSwitchPort(sw1_p3);

    Ptr<SwitchNetDevice> sw2_dev = CreateObject<SwitchNetDevice>();
    sw2_dev->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(2)));
    sw2_dev->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(5)));
    sw2->AddDevice(sw2_dev);
    sw2_dev->AddSwitchPort(sw2_p0);
    sw2_dev->AddSwitchPort(sw2_p1);
    sw2_dev->AddSwitchPort(sw2_p2);
    sw2_dev->AddSwitchPort(sw2_p3);
    sw2_dev->AddSwitchPort(sw2_p4);

    // Data Rate configuration
    es1_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es2_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es3_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es4_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es5_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es6_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    es7_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    sw1_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    sw1_p1->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    sw1_p2->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    sw1_p3->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    sw2_p0->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    sw2_p1->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    sw2_p2->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    sw2_p3->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    sw2_p4->SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

    // Full-duplex channel
    Ptr<EthernetChannel> ch_es1_sw1 = CreateObject<EthernetChannel>();
    es1_p0->Attach(ch_es1_sw1);
    sw1_p0->Attach(ch_es1_sw1);

    Ptr<EthernetChannel> ch_es2_sw1 = CreateObject<EthernetChannel>();
    es2_p0->Attach(ch_es2_sw1);
    sw1_p1->Attach(ch_es2_sw1);

    Ptr<EthernetChannel> ch_es3_sw1 = CreateObject<EthernetChannel>();
    es3_p0->Attach(ch_es3_sw1);
    sw1_p2->Attach(ch_es3_sw1);

    Ptr<EthernetChannel> ch_es4_sw2 = CreateObject<EthernetChannel>();
    es4_p0->Attach(ch_es4_sw2);
    sw2_p0->Attach(ch_es4_sw2);

    Ptr<EthernetChannel> ch_es5_sw2 = CreateObject<EthernetChannel>();
    es5_p0->Attach(ch_es5_sw2);
    sw2_p1->Attach(ch_es5_sw2);

    Ptr<EthernetChannel> ch_sw1_sw2 = CreateObject<EthernetChannel>();
    sw1_p3->Attach(ch_sw1_sw2);
    sw2_p2->Attach(ch_sw1_sw2);

    Ptr<EthernetChannel> ch_es6_sw2 = CreateObject<EthernetChannel>();
    es6_p0->Attach(ch_es6_sw2);
    sw2_p3->Attach(ch_es6_sw2);

    Ptr<EthernetChannel> ch_es7_sw2 = CreateObject<EthernetChannel>();
    es7_p0->Attach(ch_es7_sw2);
    sw2_p4->Attach(ch_es7_sw2);

    // Mac address configuration
    Mac48Address es1_mac = Mac48Address("00:00:00:00:00:01");
    Mac48Address es2_mac = Mac48Address("00:00:00:00:00:02");
    Mac48Address es3_mac = Mac48Address("00:00:00:00:00:03");
    Mac48Address es4_mac = Mac48Address("00:00:00:00:00:04");
    Mac48Address es5_mac = Mac48Address("00:00:00:00:00:05");
    Mac48Address es6_mac = Mac48Address("00:00:00:00:00:06");
    Mac48Address es7_mac = Mac48Address("00:00:00:00:00:07");

    sw1_p0->SetAddress(Mac48Address("00:00:00:00:00:11"));
    sw1_p1->SetAddress(Mac48Address("00:00:00:00:00:12"));
    sw1_p2->SetAddress(Mac48Address("00:00:00:00:00:13"));
    sw1_p3->SetAddress(Mac48Address("00:00:00:00:00:14"));
    sw2_p0->SetAddress(Mac48Address("00:00:00:00:00:21"));
    sw2_p1->SetAddress(Mac48Address("00:00:00:00:00:22"));
    sw2_p2->SetAddress(Mac48Address("00:00:00:00:00:23"));
    sw2_p3->SetAddress(Mac48Address("00:00:00:00:00:24"));
    sw2_p4->SetAddress(Mac48Address("00:00:00:00:00:25"));

    es1_p0->SetAddress(es1_mac);
    es2_p0->SetAddress(es2_mac);
    es3_p0->SetAddress(es3_mac);
    es4_p0->SetAddress(es4_mac);
    es5_p0->SetAddress(es5_mac);
    es6_p0->SetAddress(es6_mac);
    es7_p0->SetAddress(es7_mac);

    // Creation of the queues
    for (uint8_t i = 0; i < 8; i++)
    {
        es1_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        es2_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        es3_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        es4_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        es5_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        es6_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        es7_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw1_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw1_p1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw1_p2->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw1_p3->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw2_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw2_p1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw2_p2->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw2_p3->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw2_p4->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // Forwarding table configuration
    sw1_dev->AddForwardingTableEntry(es6_mac, 100, {sw1_p3});
    sw1_dev->AddForwardingTableEntry(es7_mac, 100, {sw1_p3});
    sw2_dev->AddForwardingTableEntry(es6_mac, 100, {sw2_p3});
    sw2_dev->AddForwardingTableEntry(es7_mac, 100, {sw2_p4});

    // ATS configuration
    DataRate nominalAtsRate("400Mbps");
    DataRate overProvisionedAtsRate("440Mbps"); // 10% above flow rate for S1.1.2 / S1.2.2

    DataRate selectedSchedulerRate = nominalAtsRate;
    uint32_t selectedSchedulerCbs = 4000; // Default CBS value for ATS
    if (scenario == "S1.1.2" || scenario == "S1.2.2")
    {
        selectedSchedulerRate = overProvisionedAtsRate;
        selectedSchedulerCbs = 4400; // CBS value remains the same, but the rate is over-provisioned
    }

    sw1_p3->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> ats_sw1 = sw1_p3->GetAts();
    ats_sw1->SetClock(clock0);
    ats_sw1->SetPriorityActivation(5, true);
    Ptr<AtsSchedulerGroup> ats_group_sw1 = ats_sw1->GetGroupForBridge(sw1_p2, sw1_p3, 5);
    ats_group_sw1->SetAttribute("DefaultCir", DataRateValue(selectedSchedulerRate));
    ats_group_sw1->SetAttribute("DefaultCbs", UintegerValue(selectedSchedulerCbs)); // CBS fits 1 frame

    sw2_p4->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> ats_sw2 = sw2_p4->GetAts();
    ats_sw2->SetClock(clock1);
    ats_sw2->SetPriorityActivation(5, true);
    Ptr<AtsSchedulerGroup> ats_group_sw2 = ats_sw2->GetGroupForBridge(sw2_p2, sw2_p4, 5);
    ats_group_sw2->SetAttribute("DefaultCir", DataRateValue(selectedSchedulerRate));
    ats_group_sw2->SetAttribute("DefaultCbs", UintegerValue(selectedSchedulerCbs));

    // Generate random background flows for ES1, ES2, ES4, and ES5
    double linkCapacityBps = 100000000.0; // 100 Mbps
    double targetLoadBps = targetLoad * linkCapacityBps;

    // --- load of the link SW1 -> SW2 ---
    double transitLinkLoadBps = 0.0;
    while (transitLinkLoadBps < targetLoadBps)
    {
        transitLinkLoadBps += CreateRandomBackgroundFlow(es1, es1_p0, es6);
        if (transitLinkLoadBps >= targetLoadBps)
            break;

        transitLinkLoadBps += CreateRandomBackgroundFlow(es2, es2_p0, es6);
    }
    NS_LOG_INFO("Transit Link (SW1->SW2) Background Load: " << (transitLinkLoadBps / 1e6) << " Mbps");

    // --- load of the link SW2 -> ES7 ---
    bool includeSw2Background = (scenario.rfind("S1.2.", 0) == 0);
    if (includeSw2Background)
    {
        double rxLinkLoadBps = 0.0;
        while (rxLinkLoadBps < targetLoadBps)
        {
            rxLinkLoadBps += CreateRandomBackgroundFlow(es4, es4_p0, es7);
            if (rxLinkLoadBps >= targetLoadBps)
                break;

            rxLinkLoadBps += CreateRandomBackgroundFlow(es5, es5_p0, es7);
        }
        NS_LOG_INFO("Rx Link (SW2->ES7) Background Load: " << (rxLinkLoadBps / 1e6) << " Mbps");
    }

    // ATS flow generation for the critical flow from ES1 to ES7
    Ptr<EthernetGenerator> atsApp = CreateObject<EthernetGenerator>();
    atsApp->Setup(es3_p0);
    atsApp->SetAttribute("Address", Mac48AddressValue(es7_mac));
    atsApp->SetAttribute("PayloadSize", UintegerValue(478)); // 478B + 22B header = 500 Bytes Frame
    atsApp->SetAttribute("PCP", UintegerValue(5));
    atsApp->SetAttribute("VlanID", UintegerValue(100));

    // Configure the ATS Flow behavior according to the specific scenario
    if (scenario == "S1.1.1" || scenario == "S1.1.2" || scenario == "S1.2.1" || scenario == "S1.2.2")
    {
        // Purely periodic: period = 10µs, jitter = 0
        atsApp->SetAttribute("Period", TimeValue(MicroSeconds(10)));
        atsApp->SetAttribute("Jitter", TimeValue(MicroSeconds(0)));
        atsApp->SetAttribute("BurstSize", UintegerValue(1));
    }
    else if (scenario == "S1.1.3" || scenario == "S1.2.3")
    {
        // Sporadic: T_IPG = 10µs + U(0, 1µs) -> Mean = 10.5µs, Jitter = 0.5µs
        atsApp->SetAttribute("Period", TimeValue(MicroSeconds(10.5)));
        atsApp->SetAttribute("Jitter", TimeValue(MicroSeconds(0.5)));
        atsApp->SetAttribute("BurstSize", UintegerValue(1));
    }
    else if (scenario == "S1.1.4" || scenario == "S1.2.4")
    {
        // Purely periodic (period = 10µs) but 1 out of 10 packets is skipped.
        // Since EthernetGenerator does not natively support "skip 1/10",
        // we can approximate this by generating bursts of 9 packets with 10us spacing,
        // followed by a silent period of 20us (making 10 packet-cycles of 10us with 1 omitted).
        atsApp->SetAttribute("Period", TimeValue(MicroSeconds(100))); // Complete cycle period
        atsApp->SetAttribute("BurstSize", UintegerValue(9));
        atsApp->SetAttribute("InterFrame", TimeValue(MicroSeconds(10)));
        atsApp->SetAttribute("Jitter", TimeValue(MicroSeconds(0)));
    }

    atsApp->SetStartTime(Seconds(0.0));
    es3->AddApplication(atsApp);

    // Run the simulation for the specified duration
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}