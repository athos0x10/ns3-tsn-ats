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

// --- Structures and Global Variables for Tracing ---

struct PacketRecord
{
    uint64_t uid;
    double generationTime = -1.0;   // Time when packet is sent by ES3 (MacTx)
    double eligibilityTime = -1.0;  // Time when packet becomes eligible in ATS
    double transmissionTime = -1.0; // Time when physical transmission starts (PhyTxBegin)
    double receptionTime = -1.0;    // Time when packet is received by ES7 (MacRx)
    double endToEndDelay = -1.0;    // receptionTime - generationTime
    double atsQueueingDelay = -1.0; // transmissionTime - eligibilityTime
    double interDeparture = -1.0;   // Time between consecutive physical departures
};

std::map<uint64_t, PacketRecord> packetTable;
double lastDepartureTime = -1.0;

std::ofstream csvPackets;
std::ofstream csvShaperMetric;

// --- Optimized Tracing Callbacks ---

/**
 * @brief Callback when a packet is transmitted from the MAC layer (Source ES3).
 */
static void
SourceMacTxCallback(std::string context, Ptr<const Packet> p)
{
    uint64_t uid = p->GetUid();
    packetTable[uid].uid = uid;
    packetTable[uid].generationTime = Simulator::Now().GetSeconds();

    // NS_LOG_INFO(Simulator::Now().As(Time::S) << " \t" << context << " : Critical Pkt #" << uid << " generated/sent!");
}

/**
 * @brief Callback when physical transmission starts (to catch precise departure and calculate delays).
 */
static void
PhyTxBeginCallback(std::string context, Ptr<const Packet> p)
{
    uint64_t uid = p->GetUid();

    // Only trace packets we already track (the critical flows)
    if (packetTable.find(uid) != packetTable.end())
    {
        double now = Simulator::Now().GetSeconds();
        packetTable[uid].transmissionTime = now;

        // Calculate ATS queueing delay: transmissionTime - eligibilityTime
        if (packetTable[uid].eligibilityTime > 0)
        {
            packetTable[uid].atsQueueingDelay = now - packetTable[uid].eligibilityTime;
        }

        // Calculate Inter-departure time
        double interDeparture = (lastDepartureTime > 0) ? (now - lastDepartureTime) : 0.0;
        packetTable[uid].interDeparture = interDeparture;
        lastDepartureTime = now;
    }
}

/**
 * @brief Callback triggered when ATS determines a packet's eligibility time.
 */
void AtsEligibilityCallback(Ptr<const Packet> p, Time eligibilityTime)
{
    uint64_t uid = p->GetUid();
    if (packetTable.find(uid) != packetTable.end())
    {
        packetTable[uid].eligibilityTime = eligibilityTime.GetSeconds();
    }
}

/**
 * @brief Callback when a packet is received at the destination (ES7).
 * Filters incoming traffic to only log packets coming from ES3.
 */
static void
DestinationMacRxCallback(std::string context, Ptr<const Packet> p)
{
    // Extract Ethernet Header to verify the MAC Source
    EthernetHeader2 header;
    Ptr<Packet> packetCopy = p->Copy();
    packetCopy->RemoveHeader(header);

    Mac48Address expectedSrc("00:00:00:00:00:03"); // ES3 MAC address

    if (header.GetSrc() == expectedSrc)
    {
        uint64_t uid = p->GetUid();
        double now = Simulator::Now().GetSeconds();

        packetTable[uid].receptionTime = now;

        if (packetTable[uid].generationTime > 0)
        {
            packetTable[uid].endToEndDelay = now - packetTable[uid].generationTime;
        }

        // NS_LOG_INFO(Simulator::Now().As(Time::S) << " \t" << context << " : Critical Pkt #" << uid << " received at destination!");
    }
}

/**
 * @brief Writes the clean, non-virtual physical metrics into the CSV.
 */
void WritePacketMetricsToCsv()
{
    if (csvPackets.is_open())
    {
        // CSV Header
        csvPackets << "Packet_UID,Generation_Time,Eligibility_Time,Transmission_Time,Reception_Time,End_To_End_Delay,ATS_Queueing_Delay,Inter_Departure\n";

        for (auto const &[uid, info] : packetTable)
        {
            csvPackets << info.uid << ","
                       << info.generationTime << ","
                       << info.eligibilityTime << ","
                       << info.transmissionTime << ","
                       << info.receptionTime << ","
                       << info.endToEndDelay << ","
                       << info.atsQueueingDelay << ","
                       << info.interDeparture << "\n";
        }
        csvPackets.close();
    }

    if (csvShaperMetric.is_open())
    {
        csvShaperMetric.close();
    }
}

/**
 * @brief Automatically generates a random background flow on a source node.
 *
 * @param srcNode The source TSN End Station node (e.g., ES1, ES2, ES4, ES5)
 * @param srcDevice The network device associated with the source node's egress port
 * @param destNode The destination TSN End Station node (e.g., ES6, ES7)
 * @param maxAllowedBps Maximum acceptable throughput to avoid saturating the link
 * @return double The generated throughput R_i of this flow in bits per second (bps), or 0.0 if unable to fit
 */
double CreateRandomBackgroundFlow(Ptr<TsnNode> srcNode, Ptr<TsnNetDevice> srcDevice, Ptr<TsnNetDevice> destDevice, double maxAllowedBps)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist_N(1, 10);
    std::uniform_int_distribution<int> dist_L(100, 1000);
    std::uniform_int_distribution<int> dist_priority(6, 7);

    int Ni;
    int Li;
    int priority;
    double Ri_bps;
    Time period;
    Time jitter;

    int attempts = 0;

    do
    {
        Ni = dist_N(gen);
        Li = dist_L(gen);
        priority = dist_priority(gen);

        period = NanoSeconds(static_cast<int64_t>(2500 * Ni));
        jitter = NanoSeconds(static_cast<int64_t>(1500 * Ni));

        double averageIntervalSec = period.GetSeconds();
        Ri_bps = (Li * 8.0) / averageIntervalSec;
        attempts++;
    } while (Ri_bps > maxAllowedBps && attempts < 100000);

    if (Ri_bps > maxAllowedBps)
    {
        return 0.0;
    }

    Ptr<EthernetGenerator> app = CreateObject<EthernetGenerator>();
    app->Setup(srcDevice);
    app->SetAttribute("Address", AddressValue(Mac48Address::ConvertFrom(destDevice->GetAddress())));
    app->SetAttribute("PayloadSize", UintegerValue(Li));
    app->SetAttribute("BurstSize", UintegerValue(1));
    app->SetAttribute("Period", TimeValue(period));
    app->SetAttribute("Jitter", TimeValue(jitter));
    app->SetAttribute("PCP", UintegerValue(priority));
    app->SetAttribute("VlanID", UintegerValue(100));

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

    std::string packetCsvName = "packet_metrics_" + scenario + "_load_" + std::to_string(targetLoad) + ".csv";
    csvPackets.open(packetCsvName);

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

    // Connect ATS Eligibility tracing
    ats_sw1->TraceConnectWithoutContext("EligibilityTime", MakeCallback(&AtsEligibilityCallback));
    ats_sw2->TraceConnectWithoutContext("EligibilityTime", MakeCallback(&AtsEligibilityCallback));

    // Generate random background flows for ES1, ES2, ES4, and ES5
    double linkCapacityBps = 100000000.0; // 100 Mbps
    double targetLoadBps = targetLoad * linkCapacityBps;

    // --- load of the link SW1 -> SW2 ---
    double transitLinkLoadBps = 0.0;
    while (transitLinkLoadBps < targetLoadBps)
    {
        // remaining budget to not exceed targetLoadBps
        double budget = targetLoadBps - transitLinkLoadBps;
        double flow1 = CreateRandomBackgroundFlow(es1, es1_p0, es6_p0, budget);
        if (flow1 == 0.0)
            break; // Cannot add more traffic without exceeding budget
        transitLinkLoadBps += flow1;

        if (transitLinkLoadBps >= targetLoadBps)
            break;

        budget = targetLoadBps - transitLinkLoadBps;
        double flow2 = CreateRandomBackgroundFlow(es2, es2_p0, es6_p0, budget);
        if (flow2 == 0.0)
            break; // Cannot add more traffic without exceeding budget
        transitLinkLoadBps += flow2;
    }
    NS_LOG_INFO("Transit Link (SW1->SW2) Background Load: " << (transitLinkLoadBps / 1e6) << " Mbps");

    // --- load of the link SW2 -> ES7 ---
    bool includeSw2Background = (scenario.rfind("S1.2.", 0) == 0);
    if (includeSw2Background)
    {
        double rxLinkLoadBps = 0.0;
        while (rxLinkLoadBps < targetLoadBps)
        {
            double budget = targetLoadBps - rxLinkLoadBps;
            double flow1 = CreateRandomBackgroundFlow(es4, es4_p0, es7_p0, budget);
            if (flow1 == 0.0)
                break;
            rxLinkLoadBps += flow1;

            if (rxLinkLoadBps >= targetLoadBps)
                break;

            budget = targetLoadBps - rxLinkLoadBps;
            double flow2 = CreateRandomBackgroundFlow(es5, es5_p0, es7_p0, budget);
            if (flow2 == 0.0)
                break;
            rxLinkLoadBps += flow2;
        }
        NS_LOG_INFO("Rx Link (SW2->ES7) Background Load: " << (rxLinkLoadBps / 1e6) << " Mbps");
    }

    // ATS flow generation for the critical flow from ES1 to ES7
    Ptr<EthernetGenerator> atsApp = CreateObject<EthernetGenerator>();
    atsApp->Setup(es3_p0);
    atsApp->SetAttribute("Address", AddressValue(es7_mac));
    atsApp->SetAttribute("PayloadSize", UintegerValue(478)); // 478B + 22B header = 500 Bytes Frame
    atsApp->SetAttribute("PCP", UintegerValue(5));
    atsApp->SetAttribute("VlanID", UintegerValue(100));

    // Configure the ATS Flow behavior according to the specific scenario
    if (scenario == "S1.1.1" || scenario == "S1.1.2" || scenario == "S1.2.1" || scenario == "S1.2.2")
    {
        // Purely periodic: period = 10µs, jitter = 0
        NS_LOG_INFO("ATS Flow configured as purely periodic with period = 10µs and jitter = 0");
        atsApp->SetAttribute("Period", TimeValue(MicroSeconds(10)));
        atsApp->SetAttribute("BurstSize", UintegerValue(1));
    }
    else if (scenario == "S1.1.3" || scenario == "S1.2.3")
    {
        // Sporadic: T_IPG = 10µs + U(0, 1µs) -> Mean = 10.5µs, Jitter = 0.5µs
        atsApp->SetAttribute("Period", TimeValue(NanoSeconds(10500))); // 10.5 us
        atsApp->SetAttribute("Jitter", TimeValue(NanoSeconds(500)));   // 0.5 us
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

    // --- Connect Physical & MAC Tracing for End-to-End Metrics ---

    // Trace the source (ES3) MacTx to find out exactly when the host starts sending the packet
    es3_p0->TraceConnect("MacTx", "ES3_Port0", MakeCallback(&SourceMacTxCallback));

    // Trace physical departures on switches to match eligibility times and compute real queueing times
    sw1_p3->TraceConnect("PhyTxBegin", "SW1_Port3", MakeCallback(&PhyTxBeginCallback));
    sw2_p4->TraceConnect("PhyTxBegin", "SW2_Port4", MakeCallback(&PhyTxBeginCallback));

    // Trace the destination (ES7) MacRx to register exact arrival time and compute E2E delay
    es7_p0->TraceConnect("MacRx", "ES7_Port0", MakeCallback(&DestinationMacRxCallback));

    // Run the simulation for the specified duration
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    WritePacketMetricsToCsv();
    Simulator::Destroy();
    return 0;
}