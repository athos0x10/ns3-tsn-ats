/**
 * @file blocking_flow_experimentation.cc
 * @author Arthur
 * @brief This file contains the experience for blocking flow.
 *
 * @date 2026-07-15
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/tsn-module.h"
#include "ns3/ethernet-module.h"
#include "ns3/traffic-generator-module.h"
#include "ns3/ethernet-header2.h"
#include <fstream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BlockingFlowExperimentation");

struct PacketRecord
{
    uint64_t uid;
    double arrivalTime = -1.0;
    double macTxTime = -1.0;
    double transmissionTime = -1.0;
    double eligibilityTime = -1.0;
    double interDeparture = -1.0;
    double queueDelay = 0.0;
    uint8_t pcp = 0;
};

std::map<uint64_t, PacketRecord> packetTable;
double lastDepartureTime = -1.0;

std::ofstream csvPackets;
std::ofstream csvShaperMetric;

// --- Tracing Callbacks with Context ---

static void
MacTxCallback(std::string context, Ptr<const Packet> p)
{
    uint64_t uid = p->GetUid();
    packetTable[uid].uid = uid;
    packetTable[uid].macTxTime = Simulator::Now().GetSeconds();

    Ptr<Packet> pkt = p->Copy();
    EthernetHeader2 header;
    pkt->RemoveHeader(header);

    uint8_t pcp = header.GetPcp();
    packetTable[uid].pcp = pcp;

    TimestampTag tag;
    if (p->FindFirstMatchingByteTag(tag))
    {
        packetTable[uid].arrivalTime = tag.GetTimestamp().GetSeconds();
    }
}

static void
PhyTxBeginCallback(std::string context, Ptr<const Packet> p)
{
    uint64_t uid = p->GetUid();
    double now = Simulator::Now().GetSeconds();

    packetTable[uid].transmissionTime = now;

    if (packetTable[uid].macTxTime > 0)
    {
        packetTable[uid].queueDelay = now - packetTable[uid].macTxTime;
    }

    double interDeparture = (lastDepartureTime > 0) ? (now - lastDepartureTime) : 0.0;
    packetTable[uid].interDeparture = interDeparture;
    lastDepartureTime = now;
}

void CbsCreditCallback(double newCredit)
{
    csvShaperMetric << Simulator::Now().GetSeconds() << "," << newCredit << "\n";
}

void AtsEligibilityCallback(Ptr<const Packet> p, Time eligibilityTime)
{
    uint64_t uid = p->GetUid();
    packetTable[uid].eligibilityTime = eligibilityTime.GetSeconds();
}

void WritePacketMetricsToCsv()
{
    for (auto const &[uid, info] : packetTable)
    {
        csvPackets << info.uid << ","
                   << info.arrivalTime << ","
                   << info.macTxTime << ","
                   << info.transmissionTime << ","
                   << info.queueDelay << ","
                   << info.eligibilityTime << ","
                   << info.interDeparture << ","
                   << static_cast<int>(info.pcp) << "\n";
    }

    csvPackets.close();
}

int main(int argc, char *argv[])
{
    std::string shaper = "CBS"; // Options: ATS or CBS
    int scenario = 1;           // Options: 1 or 2

    LogComponentEnable("BlockingFlowExperimentation", LOG_LEVEL_INFO);
    // LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);

    CommandLine cmd;
    cmd.AddValue("shaper", "Choice of the shaper (ATS or CBS)", shaper);
    cmd.AddValue("scenario", "Scenario's number (1 or 2)", scenario);
    cmd.Parse(argc, argv);

    csvPackets.open("packets_metrics_s" + std::to_string(scenario) + "_" + shaper + ".csv");
    csvPackets << "Packet_UID,Arrival_Time,MacTx_Time,PhyTxBegin_Time,Queue_Delay,Eligibility_Time,Inter_Departure_Time,Priority\n";

    if (shaper == "CBS")
    {
        csvShaperMetric.open("shaper_state_s" + std::to_string(scenario) + "_" + shaper + ".csv");
        csvShaperMetric << "Time,CBS_Credit\n";
    }

    // Creation of the nodes
    Ptr<TsnNode> es0 = CreateObject<TsnNode>();
    Names::Add("ES0", es0);

    Ptr<TsnNode> es1 = CreateObject<TsnNode>();
    Names::Add("ES1", es1);

    Ptr<TsnNode> es2 = CreateObject<TsnNode>();
    Names::Add("ES2", es2);

    Ptr<TsnNode> sw = CreateObject<TsnNode>();
    Names::Add("SW", sw);

    // Clock configuration
    Ptr<Clock> clock0 = CreateObject<Clock>();
    sw->AddClock(clock0);

    // Creation of the ports
    Ptr<TsnNetDevice> es0_p0 = CreateObject<TsnNetDevice>();
    es0->AddDevice(es0_p0);
    Names::Add("ES0#00", es0_p0);

    Ptr<TsnNetDevice> es1_p0 = CreateObject<TsnNetDevice>();
    es1->AddDevice(es1_p0);
    Names::Add("ES1#00", es1_p0);

    Ptr<TsnNetDevice> es2_p0 = CreateObject<TsnNetDevice>();
    es2->AddDevice(es2_p0);
    Names::Add("ES2#00", es2_p0);

    Ptr<TsnNetDevice> sw_p0 = CreateObject<TsnNetDevice>();
    sw->AddDevice(sw_p0);
    Names::Add("SW#00", sw_p0);

    Ptr<TsnNetDevice> sw_p1 = CreateObject<TsnNetDevice>();
    sw->AddDevice(sw_p1);
    Names::Add("SW#01", sw_p1);

    Ptr<TsnNetDevice> sw_p2 = CreateObject<TsnNetDevice>();
    sw->AddDevice(sw_p2);
    Names::Add("SW#02", sw_p2);

    // Switch Device Configuration
    Ptr<SwitchNetDevice> sw_dev = CreateObject<SwitchNetDevice>();
    sw_dev->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(2)));
    sw_dev->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(5)));
    sw->AddDevice(sw_dev);
    sw_dev->AddSwitchPort(sw_p0);
    sw_dev->AddSwitchPort(sw_p1);
    sw_dev->AddSwitchPort(sw_p2);

    // Data Rate configuration
    es0_p0->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));
    sw_p0->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));
    es1_p0->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));
    sw_p1->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));
    es2_p0->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));
    sw_p2->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));

    // Full-duplex channel
    Ptr<EthernetChannel> ch_es0_sw = CreateObject<EthernetChannel>();
    es0_p0->Attach(ch_es0_sw);
    sw_p0->Attach(ch_es0_sw);

    Ptr<EthernetChannel> ch_es1_sw = CreateObject<EthernetChannel>();
    es1_p0->Attach(ch_es1_sw);
    sw_p1->Attach(ch_es1_sw);

    Ptr<EthernetChannel> ch_es2_sw = CreateObject<EthernetChannel>();
    es2_p0->Attach(ch_es2_sw);
    sw_p2->Attach(ch_es2_sw);

    // Mac address configuration
    es0_p0->SetAddress(Mac48Address("00:00:00:00:00:01"));
    es1_p0->SetAddress(Mac48Address("00:00:00:00:00:02"));
    Mac48Address es2_mac = Mac48Address("00:00:00:00:00:03");
    es2_p0->SetAddress(es2_mac);
    sw_p0->SetAddress(Mac48Address("00:00:00:00:00:11"));
    sw_p1->SetAddress(Mac48Address("00:00:00:00:00:12"));
    sw_p2->SetAddress(Mac48Address("00:00:00:00:00:13"));

    // Creation of queues (sw_p2 excluded)
    for (uint8_t pcp = 0u; pcp < 8u; pcp++)
    {
        es0_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        es1_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        es2_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw_p1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // Creation of queues for sw_p2

    // CBS will be on priority 0
    if (shaper == "CBS")
    {
        Ptr<Cbs> cbs = CreateObject<Cbs>();
        cbs->SetTsnNetDevice(sw_p2);
        cbs->SetAttribute("IdleSlope", DataRateValue(DataRate("30Mbps")));
        cbs->SetAttribute("portTransmitRate", DataRateValue(DataRate("120Mb/s")));
        sw_p2->SetQueue(CreateObject<DropTailQueue<Packet>>(), cbs);
        cbs->TraceConnectWithoutContext("Credit", MakeCallback(&CbsCreditCallback));
    }
    // Others queue are FIFO queues
    for (uint8_t pcp = (shaper == "CBS") ? 1u : 0u; pcp < 8u; pcp++)
    {
        sw_p2->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // ATS configuration
    if (shaper == "ATS")
    {
        sw_p2->SetAttribute("isAtsEnabled", BooleanValue(true));
        Ptr<Ats> ats = sw_p2->GetAts();
        ats->SetClock(clock0);
        ats->SetAttribute("MaxResidenceTime", TimeValue(Seconds(1)));
        // We assume that ES0 will be the flow blocked by the other.
        Ptr<AtsSchedulerGroup> ats_group_pcp0 = ats->GetGroupForBridge(sw_p0, sw_p2, 0);
        ats_group_pcp0->SetAttribute("DefaultCir", DataRateValue(DataRate("30Mbps")));
        ats_group_pcp0->SetAttribute("DefaultCbs", UintegerValue(3200));
        // The blocking flow
        Ptr<AtsSchedulerGroup> ats_group_pcp7 = ats->GetGroupForBridge(sw_p1, sw_p2, 7);
        ats_group_pcp7->SetAttribute("DefaultCir", DataRateValue(DataRate("100Mbps")));
        ats_group_pcp7->SetAttribute("DefaultCbs", UintegerValue(8000));
        // Tracing connection
        ats_group_pcp0->TraceConnectWithoutContext("EligibilityTime", MakeCallback(&AtsEligibilityCallback));
        ats_group_pcp7->TraceConnectWithoutContext("EligibilityTime", MakeCallback(&AtsEligibilityCallback));
    }

    // Stream Identification
    Ptr<NullStreamIdentificationFunction> sif1 = CreateObject<NullStreamIdentificationFunction>();
    uint16_t streamHandle1 = 1;
    sif1->SetAttribute("VlanID", UintegerValue(1));
    sif1->SetAttribute("Address", AddressValue(es2_mac));
    sw->AddStreamIdentificationFunction(streamHandle1, sif1, {sw_p0}, {}, {}, {});
    Ptr<NullStreamIdentificationFunction> sif2 = CreateObject<NullStreamIdentificationFunction>();
    uint16_t streamHandle2 = 2;
    sif2->SetAttribute("VlanID", UintegerValue(2));
    sif2->SetAttribute("Address", AddressValue(es2_mac));
    sw->AddStreamIdentificationFunction(streamHandle2, sif2, {sw_p1}, {}, {}, {});

    // Forxarding table configuration
    sw_dev->AddForwardingTableEntry(es2_mac, 1, {sw_p2});
    sw_dev->AddForwardingTableEntry(es2_mac, 2, {sw_p2});

    // Tracing configuration
    std::string contextTx = Names::FindName(sw) + ":" + Names::FindName(sw_p2);
    sw_p2->TraceConnectWithoutContext("MacTx", MakeBoundCallback(&MacTxCallback, contextTx));
    sw_p2->TraceConnectWithoutContext("PhyTxBegin", MakeBoundCallback(&PhyTxBeginCallback, contextTx));

    // Application configuration
    // The blocking flow
    Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
    app1->Setup(es1_p0);
    app1->SetAttribute("Address", AddressValue(es2_mac));
    app1->SetAttribute("VlanID", UintegerValue(2));
    app1->SetAttribute("PayloadSize", UintegerValue(978));
    app1->SetAttribute("BurstSize", UintegerValue(1));
    app1->SetAttribute("Period", TimeValue(Seconds(1.0)));
    app1->SetAttribute("PCP", UintegerValue(7));
    es1->AddApplication(app1);
    app1->SetStartTime(Seconds(1.0));
    app1->SetStopTime(Seconds(1.1));

    if (scenario == 1)
    {
        // Send a burst of 4 packets of size 400B
        Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
        app2->Setup(es0_p0);
        app2->SetAttribute("Address", AddressValue(es2_mac));
        app2->SetAttribute("VlanID", UintegerValue(1));
        app2->SetAttribute("PayloadSize", UintegerValue(378));
        app2->SetAttribute("BurstSize", UintegerValue(4));
        app2->SetAttribute("Period", TimeValue(Seconds(1.0)));
        app2->SetAttribute("PCP", UintegerValue(0));
        es0->AddApplication(app2);
        app2->SetStartTime(Seconds(1.00005));
        app2->SetStopTime(Seconds(1.1));
    }
    else
    {
        // Send a packet during the blocking and then a burst of 3 packets of size 400B
        Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
        app2->Setup(es0_p0);
        app2->SetAttribute("Address", AddressValue(es2_mac));
        app2->SetAttribute("VlanID", UintegerValue(1));
        app2->SetAttribute("PayloadSize", UintegerValue(378));
        app2->SetAttribute("BurstSize", UintegerValue(1));
        app2->SetAttribute("Period", TimeValue(Seconds(1.0)));
        app2->SetAttribute("PCP", UintegerValue(0));
        es0->AddApplication(app2);
        app2->SetStartTime(Seconds(1.00005));
        app2->SetStopTime(Seconds(1.1));

        Ptr<EthernetGenerator> app3 = CreateObject<EthernetGenerator>();
        app3->Setup(es0_p0);
        app3->SetAttribute("Address", AddressValue(es2_mac));
        app3->SetAttribute("VlanID", UintegerValue(1));
        app3->SetAttribute("PayloadSize", UintegerValue(378));
        app3->SetAttribute("BurstSize", UintegerValue(3));
        app3->SetAttribute("Period", TimeValue(Seconds(1.0)));
        app3->SetAttribute("PCP", UintegerValue(0));
        es0->AddApplication(app3);
        app3->SetStartTime(Seconds(1.00013667));
        app3->SetStopTime(Seconds(1.1));
    }

    // Execute the simulation
    Simulator::ScheduleDestroy(&WritePacketMetricsToCsv);
    Simulator::Run();
    Simulator::Destroy();

    csvShaperMetric.close();
    return 0;
}