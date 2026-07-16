/**
 * @file flow-multiplexing-experimentation-s34.cc
 * @author Arthur
 * @brief This program give the experiementation to compare ATS/CBS
 * specificaly for flow multiplexing (2 flows).
 *
 * @date 2026-07-14
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

NS_LOG_COMPONENT_DEFINE("FlowMultiplexingExperimentationS34");

struct PacketRecord
{
    uint64_t uid;
    double arrivalTime = -1.0;
    double macTxTime = -1.0;
    double transmissionTime = -1.0;
    double eligibilityTime = -1.0;
    double interDeparture = -1.0;
    double queueDelay = 0.0;
    uint16_t vlanId = 0;
};

std::map<uint64_t, PacketRecord> packetTable;
double lastDepartureTime = -1.0;

std::ofstream csvPackets;

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

    uint16_t vlanId = header.GetVid();
    packetTable[uid].vlanId = vlanId;

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
                   << info.vlanId << "\n";
    }

    csvPackets.close();
}

int main(int argc, char *argv[])
{
    std::string scenario = "FLOW"; // Options: FLOW or GLOBAL

    LogComponentEnable("FlowMultiplexingExperimentationS34", LOG_LEVEL_INFO);
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);

    CommandLine cmd;
    cmd.AddValue("scenario", "Choice of the scenario (FLOW or GLOBAL)", scenario);
    cmd.Parse(argc, argv);

    csvPackets.open("packets_metrics_" + scenario + ".csv");
    csvPackets << "Packet_UID,Arrival_Time,MacTx_Time,PhyTxBegin_Time,Queue_Delay,Eligibility_Time,Inter_Departure_Time,VlanId\n";

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

    // Switch device configuration
    Ptr<SwitchNetDevice> sw_dev = CreateObject<SwitchNetDevice>();
    sw_dev->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(2)));
    sw_dev->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(5)));
    sw->AddDevice(sw_dev);
    sw_dev->AddSwitchPort(sw_p0);
    sw_dev->AddSwitchPort(sw_p1);
    sw_dev->AddSwitchPort(sw_p2);

    // Date rate configuration
    es0_p0->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));
    sw_p0->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));
    es1_p0->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));
    sw_p1->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));
    es2_p0->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));
    sw_p2->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));

    // Creation of the channels
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

    // Creation of the queues
    es0_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
    es1_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
    es2_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
    sw_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
    sw_p1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    sw_p2->SetQueue(CreateObject<DropTailQueue<Packet>>());

    // Shaper configuration
    sw_p2->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> ats = sw_p2->GetAts();
    ats->SetPriorityActivation(0, true);
    ats->SetAttribute("MaxResidenceTime", TimeValue(Seconds(1)));
    ats->SetClock(clock0);

    if (scenario == "GLOBAL")
    {
        Ptr<AtsSchedulerGroup> ats_group = ats->GetGroupForBridge(sw_p0, sw_p2, 0);
        uint32_t instance_A = ats_group->CreateAtsInstance(DataRate("20Mbps"), 2000);
        uint32_t instance_B = ats_group->CreateAtsInstance(DataRate("30Mbps"), 2000);
        ats_group->BindStreamToInstance(1, instance_A);
        ats_group->BindStreamToInstance(2, instance_B);
        ats_group->TraceConnectWithoutContext("EligibilityTime", MakeCallback(&AtsEligibilityCallback));
    }
    else
    {
        Ptr<AtsSchedulerGroup> ats_group_p0 = ats->GetGroupForBridge(sw_p0, sw_p2, 0);
        Ptr<AtsSchedulerGroup> ats_group_p1 = ats->GetGroupForBridge(sw_p1, sw_p2, 0);
        ats_group_p0->SetAttribute("DefaultCir", DataRateValue(DataRate("20Mbps")));
        ats_group_p0->SetAttribute("DefaultCbs", UintegerValue(1600));
        ats_group_p0->TraceConnectWithoutContext("EligibilityTime", MakeCallback(&AtsEligibilityCallback));
        ats_group_p1->SetAttribute("DefaultCir", DataRateValue(DataRate("30Mbps")));
        ats_group_p1->SetAttribute("DefaultCbs", UintegerValue(1600));
        ats_group_p1->TraceConnectWithoutContext("EligibilityTime", MakeCallback(&AtsEligibilityCallback));
    }

    // Stream Identification
    if (scenario == "GLOBAL")
    {
        Ptr<NullStreamIdentificationFunction> sif1 = CreateObject<NullStreamIdentificationFunction>();
        uint16_t streamHandle1 = 1;
        sif1->SetAttribute("VlanID", UintegerValue(1));
        sif1->SetAttribute("Address", AddressValue(es2_mac));
        sw->AddStreamIdentificationFunction(streamHandle1, sif1, {sw_p0}, {}, {}, {});
        Ptr<NullStreamIdentificationFunction> sif2 = CreateObject<NullStreamIdentificationFunction>();
        uint16_t streamHandle2 = 2;
        sif2->SetAttribute("VlanID", UintegerValue(2));
        sif2->SetAttribute("Address", AddressValue(es2_mac));
        sw->AddStreamIdentificationFunction(streamHandle2, sif2, {sw_p0}, {}, {}, {});
    }
    else
    {
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
    }

    // Forwarding table configuration
    sw_dev->AddForwardingTableEntry(es2_mac, 1, {sw_p2});
    sw_dev->AddForwardingTableEntry(es2_mac, 2, {sw_p2});

    // Tracing configuration
    std::string contextTx = Names::FindName(sw) + ":" + Names::FindName(sw_p2);
    sw_p2->TraceConnectWithoutContext("MacTx", MakeBoundCallback(&MacTxCallback, contextTx));
    sw_p2->TraceConnectWithoutContext("PhyTxBegin", MakeBoundCallback(&PhyTxBeginCallback, contextTx));

    // Application configuration
    if (scenario == "FLOW")
    {
        Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
        app1->Setup(es0_p0);
        app1->SetAttribute("Address", AddressValue(es2_mac));
        app1->SetAttribute("VlanID", UintegerValue(1));
        app1->SetAttribute("PayloadSize", UintegerValue(178));
        app1->SetAttribute("BurstSize", UintegerValue(1));
        app1->SetAttribute("Period", TimeValue(Seconds(0.00008)));
        es0->AddApplication(app1);
        app1->SetStartTime(Seconds(1.0));
        app1->SetStopTime(Seconds(1.00009));

        Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
        app2->Setup(es1_p0);
        app2->SetAttribute("Address", AddressValue(es2_mac));
        app2->SetAttribute("VlanID", UintegerValue(2));
        app2->SetAttribute("PayloadSize", UintegerValue(178));
        app2->SetAttribute("BurstSize", UintegerValue(1));
        app2->SetAttribute("Period", TimeValue(Seconds(0.00008)));
        es1->AddApplication(app2);
        app2->SetStartTime(Seconds(1.00004));
        app2->SetStopTime(Seconds(1.00013));

        Ptr<EthernetGenerator> app3 = CreateObject<EthernetGenerator>();
        app3->Setup(es1_p0);
        app3->SetAttribute("Address", AddressValue(es2_mac));
        app3->SetAttribute("VlanID", UintegerValue(2));
        app3->SetAttribute("PayloadSize", UintegerValue(178));
        app3->SetAttribute("BurstSize", UintegerValue(1));
        app3->SetAttribute("Period", TimeValue(Seconds(1.0)));
        es1->AddApplication(app3);
        app3->SetStartTime(Seconds(1.00012));
        app3->SetStopTime(Seconds(1.00021));
    }
    else
    {
        Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
        app1->Setup(es0_p0);
        app1->SetAttribute("Address", AddressValue(es2_mac));
        app1->SetAttribute("VlanID", UintegerValue(1));
        app1->SetAttribute("PayloadSize", UintegerValue(178));
        app1->SetAttribute("BurstSize", UintegerValue(1));
        app1->SetAttribute("Period", TimeValue(Seconds(1.0)));
        es0->AddApplication(app1);
        app1->SetStartTime(Seconds(1.0));
        app1->SetStopTime(Seconds(1.1));

        Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
        app2->Setup(es0_p0);
        app2->SetAttribute("Address", AddressValue(es2_mac));
        app2->SetAttribute("VlanID", UintegerValue(2));
        app2->SetAttribute("PayloadSize", UintegerValue(178));
        app2->SetAttribute("BurstSize", UintegerValue(1));
        app2->SetAttribute("Period", TimeValue(Seconds(1.0)));
        es0->AddApplication(app2);
        app2->SetStartTime(Seconds(1.0));
        app2->SetStopTime(Seconds(1.1));

        Ptr<EthernetGenerator> app3 = CreateObject<EthernetGenerator>();
        app3->Setup(es0_p0);
        app3->SetAttribute("Address", AddressValue(es2_mac));
        app3->SetAttribute("VlanID", UintegerValue(1));
        app3->SetAttribute("PayloadSize", UintegerValue(178));
        app3->SetAttribute("BurstSize", UintegerValue(1));
        app3->SetAttribute("Period", TimeValue(Seconds(1.0)));
        es0->AddApplication(app3);
        app3->SetStartTime(Seconds(1.0));
        app3->SetStopTime(Seconds(1.1));

        Ptr<EthernetGenerator> app4 = CreateObject<EthernetGenerator>();
        app4->Setup(es0_p0);
        app4->SetAttribute("Address", AddressValue(es2_mac));
        app4->SetAttribute("VlanID", UintegerValue(2));
        app4->SetAttribute("PayloadSize", UintegerValue(178));
        app4->SetAttribute("BurstSize", UintegerValue(2));
        app4->SetAttribute("Period", TimeValue(Seconds(1.0)));
        es0->AddApplication(app4);
        app4->SetStartTime(Seconds(1.0));
        app4->SetStopTime(Seconds(1.1));
    }

    // Execution of the simulation
    Simulator::ScheduleDestroy(&WritePacketMetricsToCsv);
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}