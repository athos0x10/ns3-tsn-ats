/**
 * @file worst-case-cbs.cc
 * @author Arthur
 * @brief Worst-case experimentation for TSN Family S1 using CBS (Credit-Based Shaper).
 * @date 2026-07-24
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

NS_LOG_COMPONENT_DEFINE("WorstCaseExperimentationCBS");

// --- Data Structures ---

struct PacketRecord
{
    uint64_t uid;
    double generationTime = -1.0; // ES3 Transmission Time
    double receptionTime = -1.0;  // ES7 Reception Time

    // Switch 1 Metrics
    double sw1_arrivalTime = -1.0;
    double sw1_departureTime = -1.0;

    // Switch 2 Metrics
    double sw2_arrivalTime = -1.0;
    double sw2_departureTime = -1.0;

    double interDeparture = -1.0; // Physical inter-departure interval at SW2 egress
};

std::map<uint64_t, PacketRecord> packetTable;
double lastDepartureTime = -1.0;
std::ofstream csvPackets;

// --- Callbacks ---

static void
SourceMacTxCallback(std::string context, Ptr<const Packet> p)
{
    uint64_t uid = p->GetUid();
    packetTable[uid].uid = uid;
    packetTable[uid].generationTime = Simulator::Now().GetSeconds();
}

static void
Sw1IngressCallback(std::string context, Ptr<const Packet> p)
{
    uint64_t uid = p->GetUid();
    if (packetTable.find(uid) != packetTable.end())
    {
        packetTable[uid].sw1_arrivalTime = Simulator::Now().GetSeconds();
    }
}

static void
Sw2IngressCallback(std::string context, Ptr<const Packet> p)
{
    uint64_t uid = p->GetUid();
    if (packetTable.find(uid) != packetTable.end())
    {
        packetTable[uid].sw2_arrivalTime = Simulator::Now().GetSeconds();
    }
}

static void
Sw1PhyTxBeginCallback(std::string context, Ptr<const Packet> p)
{
    uint64_t uid = p->GetUid();
    if (packetTable.find(uid) != packetTable.end())
    {
        packetTable[uid].sw1_departureTime = Simulator::Now().GetSeconds();
    }
}

static void
Sw2PhyTxBeginCallback(std::string context, Ptr<const Packet> p)
{
    uint64_t uid = p->GetUid();
    if (packetTable.find(uid) != packetTable.end())
    {
        double now = Simulator::Now().GetSeconds();
        packetTable[uid].sw2_departureTime = now;

        double interDeparture = (lastDepartureTime > 0) ? (now - lastDepartureTime) : 0.0;
        packetTable[uid].interDeparture = interDeparture;
        lastDepartureTime = now;
    }
}

static void
DestinationMacRxCallback(std::string context, Ptr<const Packet> p)
{
    uint64_t uid = p->GetUid();
    if (packetTable.find(uid) != packetTable.end())
    {
        packetTable[uid].receptionTime = Simulator::Now().GetSeconds();
    }
}

/**
 * @brief Writes CBS packet metrics into the output CSV file.
 * (Sans les colonnes d'éligibilité)
 */
void WritePacketMetricsToCsv()
{
    if (csvPackets.is_open())
    {
        csvPackets << "Packet_UID,Generation_Time,Reception_Time,End_To_End_Delay,"
                   << "SW1_Arrival,SW1_Departure,"
                   << "SW2_Arrival,SW2_Departure,"
                   << "Diff_EntrySW1_ExitSW2,"
                   << "Stay_Time_SW1,Stay_Time_SW2,Inter_Departure\n";

        for (auto const &[uid, info] : packetTable)
        {
            double e2eDelay = (info.receptionTime > 0 && info.generationTime >= 0) ? (info.receptionTime - info.generationTime) : -1.0;
            double diffEntrySW1ExitSW2 = (info.sw2_departureTime > 0 && info.sw1_arrivalTime >= 0) ? (info.sw2_departureTime - info.sw1_arrivalTime) : -1.0;
            double staySW1 = (info.sw1_departureTime > 0 && info.sw1_arrivalTime >= 0) ? (info.sw1_departureTime - info.sw1_arrivalTime) : -1.0;
            double staySW2 = (info.sw2_departureTime > 0 && info.sw2_arrivalTime >= 0) ? (info.sw2_departureTime - info.sw2_arrivalTime) : -1.0;

            csvPackets << info.uid << ","
                       << info.generationTime << ","
                       << info.receptionTime << ","
                       << e2eDelay << ","
                       << info.sw1_arrivalTime << ","
                       << info.sw1_departureTime << ","
                       << info.sw2_arrivalTime << ","
                       << info.sw2_departureTime << ","
                       << diffEntrySW1ExitSW2 << ","
                       << staySW1 << ","
                       << staySW2 << ","
                       << info.interDeparture << "\n";
        }
        csvPackets.close();
    }
}

double CreateRandomBackgroundFlow(Ptr<TsnNode> srcNode, Ptr<TsnNetDevice> srcDevice, Ptr<TsnNetDevice> destDevice, double maxAllowedBps)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_int_distribution<int> dist_N(100, 1000);
    std::uniform_int_distribution<int> dist_L(100, 1000);
    std::uniform_int_distribution<int> dist_priority(6, 7);

    int Ni, Li, priority;
    double Ri_bps;
    Time period, jitter;
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
        return 0.0;

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
    double targetLoad = 0.05;
    double simTime = 1.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("scenario", "Scenario identifier", scenario);
    cmd.AddValue("load", "Target load ratio (e.g. 0.05)", targetLoad);
    cmd.AddValue("simTime", "Total simulation duration", simTime);
    cmd.Parse(argc, argv);

    LogComponentEnable("WorstCaseExperimentationCBS", LOG_LEVEL_INFO);

    std::string packetCsvName = "packet_metrics_" + scenario + "_load_" + std::to_string(targetLoad) + "_cbs.csv";
    csvPackets.open(packetCsvName);

    // Nodes creation
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

    // Devices port creation
    Ptr<TsnNetDevice> es1_p0 = CreateObject<TsnNetDevice>();
    es1->AddDevice(es1_p0);
    Ptr<TsnNetDevice> es2_p0 = CreateObject<TsnNetDevice>();
    es2->AddDevice(es2_p0);
    Ptr<TsnNetDevice> es3_p0 = CreateObject<TsnNetDevice>();
    es3->AddDevice(es3_p0);
    Ptr<TsnNetDevice> es4_p0 = CreateObject<TsnNetDevice>();
    es4->AddDevice(es4_p0);
    Ptr<TsnNetDevice> es5_p0 = CreateObject<TsnNetDevice>();
    es5->AddDevice(es5_p0);
    Ptr<TsnNetDevice> es6_p0 = CreateObject<TsnNetDevice>();
    es6->AddDevice(es6_p0);
    Ptr<TsnNetDevice> es7_p0 = CreateObject<TsnNetDevice>();
    es7->AddDevice(es7_p0);

    Ptr<TsnNetDevice> sw1_p0 = CreateObject<TsnNetDevice>();
    sw1->AddDevice(sw1_p0);
    Ptr<TsnNetDevice> sw1_p1 = CreateObject<TsnNetDevice>();
    sw1->AddDevice(sw1_p1);
    Ptr<TsnNetDevice> sw1_p2 = CreateObject<TsnNetDevice>();
    sw1->AddDevice(sw1_p2);
    Ptr<TsnNetDevice> sw1_p3 = CreateObject<TsnNetDevice>();
    sw1->AddDevice(sw1_p3);

    Ptr<TsnNetDevice> sw2_p0 = CreateObject<TsnNetDevice>();
    sw2->AddDevice(sw2_p0);
    Ptr<TsnNetDevice> sw2_p1 = CreateObject<TsnNetDevice>();
    sw2->AddDevice(sw2_p1);
    Ptr<TsnNetDevice> sw2_p2 = CreateObject<TsnNetDevice>();
    sw2->AddDevice(sw2_p2);
    Ptr<TsnNetDevice> sw2_p3 = CreateObject<TsnNetDevice>();
    sw2->AddDevice(sw2_p3);
    Ptr<TsnNetDevice> sw2_p4 = CreateObject<TsnNetDevice>();
    sw2->AddDevice(sw2_p4);

    // Clocks configuration
    Ptr<Clock> clock0 = CreateObject<Clock>();
    sw1->AddClock(clock0);
    Ptr<Clock> clock1 = CreateObject<Clock>();
    sw2->AddClock(clock1);

    // Bridges configuration
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

    // Rates configuration
    DataRate linkRate("1000Mbps");
    es1_p0->SetAttribute("DataRate", DataRateValue(linkRate));
    es2_p0->SetAttribute("DataRate", DataRateValue(linkRate));
    es3_p0->SetAttribute("DataRate", DataRateValue(linkRate));
    es4_p0->SetAttribute("DataRate", DataRateValue(linkRate));
    es5_p0->SetAttribute("DataRate", DataRateValue(linkRate));
    es6_p0->SetAttribute("DataRate", DataRateValue(linkRate));
    es7_p0->SetAttribute("DataRate", DataRateValue(linkRate));

    sw1_p0->SetAttribute("DataRate", DataRateValue(linkRate));
    sw1_p1->SetAttribute("DataRate", DataRateValue(linkRate));
    sw1_p2->SetAttribute("DataRate", DataRateValue(linkRate));
    sw1_p3->SetAttribute("DataRate", DataRateValue(linkRate));

    sw2_p0->SetAttribute("DataRate", DataRateValue(linkRate));
    sw2_p1->SetAttribute("DataRate", DataRateValue(linkRate));
    sw2_p2->SetAttribute("DataRate", DataRateValue(linkRate));
    sw2_p3->SetAttribute("DataRate", DataRateValue(linkRate));
    sw2_p4->SetAttribute("DataRate", DataRateValue(linkRate));

    // Channels connection
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

    // MAC Setup
    Mac48Address es1_mac("00:00:00:00:00:01");
    Mac48Address es2_mac("00:00:00:00:00:02");
    Mac48Address es3_mac("00:00:00:00:00:03");
    Mac48Address es4_mac("00:00:00:00:00:04");
    Mac48Address es5_mac("00:00:00:00:00:05");
    Mac48Address es6_mac("00:00:00:00:00:06");
    Mac48Address es7_mac("00:00:00:00:00:07");

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

    // CBS Queues Setup
    DataRate nominalCbsRate("400Mbps");
    DataRate overProvisionedCbsRate("440Mbps");
    DataRate selectedSchedulerRate = nominalCbsRate;

    if (scenario == "S1.1.2" || scenario == "S1.2.2")
    {
        selectedSchedulerRate = overProvisionedCbsRate;
    }

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

        if (i != 4)
        {
            sw1_p3->SetQueue(CreateObject<DropTailQueue<Packet>>());
        }
        else
        {
            Ptr<Cbs> cbs = CreateObject<Cbs>();
            cbs->SetTsnNetDevice(sw1_p3);
            cbs->SetAttribute("IdleSlope", DataRateValue(selectedSchedulerRate));
            cbs->SetAttribute("portTransmitRate", DataRateValue(DataRate("1000Mbps")));
            sw1_p3->SetQueue(CreateObject<DropTailQueue<Packet>>(), cbs);
        }

        sw2_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw2_p1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw2_p2->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw2_p3->SetQueue(CreateObject<DropTailQueue<Packet>>());

        if (i != 4)
        {
            sw2_p4->SetQueue(CreateObject<DropTailQueue<Packet>>());
        }
        else
        {
            Ptr<Cbs> cbs = CreateObject<Cbs>();
            cbs->SetTsnNetDevice(sw2_p4);
            cbs->SetAttribute("IdleSlope", DataRateValue(selectedSchedulerRate));
            cbs->SetAttribute("portTransmitRate", DataRateValue(DataRate("1000Mbps")));
            sw2_p4->SetQueue(CreateObject<DropTailQueue<Packet>>(), cbs);
        }
    }

    // Forwarding tables
    sw1_dev->AddForwardingTableEntry(es6_mac, 100, {sw1_p3});
    sw1_dev->AddForwardingTableEntry(es7_mac, 100, {sw1_p3});
    sw2_dev->AddForwardingTableEntry(es6_mac, 100, {sw2_p3});
    sw2_dev->AddForwardingTableEntry(es7_mac, 100, {sw2_p4});

    // Background Traffic
    double linkCapacityBps = 1000000000.0;
    double targetLoadBps = targetLoad * linkCapacityBps;

    double transitLinkLoadBps = 0.0;
    while (transitLinkLoadBps < targetLoadBps)
    {
        double budget = targetLoadBps - transitLinkLoadBps;
        double flow1 = CreateRandomBackgroundFlow(es1, es1_p0, es6_p0, budget);
        if (flow1 == 0.0)
            break;
        transitLinkLoadBps += flow1;

        if (transitLinkLoadBps >= targetLoadBps)
            break;

        budget = targetLoadBps - transitLinkLoadBps;
        double flow2 = CreateRandomBackgroundFlow(es2, es2_p0, es6_p0, budget);
        if (flow2 == 0.0)
            break;
        transitLinkLoadBps += flow2;
    }

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
    }

    // CBS Critical Flow Application
    Ptr<EthernetGenerator> cbsApp = CreateObject<EthernetGenerator>();
    cbsApp->Setup(es3_p0);
    cbsApp->SetAttribute("Address", AddressValue(es7_mac));
    cbsApp->SetAttribute("PayloadSize", UintegerValue(478));
    cbsApp->SetAttribute("PCP", UintegerValue(5));
    cbsApp->SetAttribute("VlanID", UintegerValue(100));

    if (scenario == "S1.1.1" || scenario == "S1.1.2" || scenario == "S1.2.1" || scenario == "S1.2.2")
    {
        cbsApp->SetAttribute("Period", TimeValue(MicroSeconds(10)));
        cbsApp->SetAttribute("BurstSize", UintegerValue(1));
    }
    else if (scenario == "S1.1.3" || scenario == "S1.2.3")
    {
        cbsApp->SetAttribute("Period", TimeValue(NanoSeconds(10500)));
        cbsApp->SetAttribute("Jitter", TimeValue(NanoSeconds(500)));
        cbsApp->SetAttribute("BurstSize", UintegerValue(1));
    }
    else if (scenario == "S1.1.4" || scenario == "S1.2.4")
    {
        cbsApp->SetAttribute("Period", TimeValue(MicroSeconds(100)));
        cbsApp->SetAttribute("BurstSize", UintegerValue(9));
        cbsApp->SetAttribute("InterFrame", TimeValue(MicroSeconds(10)));
        cbsApp->SetAttribute("Jitter", TimeValue(MicroSeconds(0)));
    }

    cbsApp->SetStartTime(Seconds(0.0));
    es3->AddApplication(cbsApp);

    // Tracing Connections
    es3_p0->TraceConnect("MacTx", "ES3_MacTx", MakeCallback(&SourceMacTxCallback));

    sw1_p2->TraceConnect("PhyRxEnd", "SW1_Rx", MakeCallback(&Sw1IngressCallback));
    sw2_p2->TraceConnect("PhyRxEnd", "SW2_Rx", MakeCallback(&Sw2IngressCallback));

    sw1_p3->TraceConnect("PhyTxBegin", "SW1_Tx", MakeCallback(&Sw1PhyTxBeginCallback));
    sw2_p4->TraceConnect("PhyTxBegin", "SW2_Tx", MakeCallback(&Sw2PhyTxBeginCallback));

    es7_p0->TraceConnect("MacRx", "ES7_MacRx", MakeCallback(&DestinationMacRxCallback));

    // Simulation Execution
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    WritePacketMetricsToCsv();
    Simulator::Destroy();
    return 0;
}