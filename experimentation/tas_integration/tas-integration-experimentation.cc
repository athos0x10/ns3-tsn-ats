/**
 * @file tas-integration-experimentation.cc
 * @author Arthur
 * @brief This file contains the experimentation to compare ATS and CBS
 * specifically with TAS integration.
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

NS_LOG_COMPONENT_DEFINE("TasIntegrationExperimentation");

struct PacketRecord
{
    uint64_t uid;
    double arrivalTime = -1.0;
    double macTxTime = -1.0;
    double transmissionTime = -1.0;
    double eligibilityTime = -1.0;
    double interDeparture = -1.0;
    double queueDelay = 0.0;
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
                   << info.interDeparture << "\n";
    }

    csvPackets.close();
    if (csvShaperMetric.is_open())
    {
        csvShaperMetric.close();
    }
}

int main(int argc, char *argv[])
{
    std::string shaper = "CBS"; // Options: ATS or CBS

    LogComponentEnable("TasIntegrationExperimentation", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.AddValue("shaper", "Choice of the shaper (ATS or CBS)", shaper);
    cmd.Parse(argc, argv);

    csvPackets.open("packets_metrics_" + shaper + ".csv");
    csvPackets << "Packet_UID,Arrival_Time,MacTx_Time,PhyTxBegin_Time,Queue_Delay,Eligibility_Time,Inter_Departure_Time\n";

    if (shaper == "CBS")
    {
        csvShaperMetric.open("shaper_state_" + shaper + ".csv");
        csvShaperMetric << "Time,CBS_Credit\n";
    }

    // Creation of the nodes
    Ptr<TsnNode> es0 = CreateObject<TsnNode>();
    Names::Add("ES0", es0);

    Ptr<TsnNode> es1 = CreateObject<TsnNode>();
    Names::Add("ES1", es1);

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

    Ptr<TsnNetDevice> sw_p0 = CreateObject<TsnNetDevice>();
    sw->AddDevice(sw_p0);
    Names::Add("SW#00", sw_p0);

    Ptr<TsnNetDevice> sw_p1 = CreateObject<TsnNetDevice>();
    sw->AddDevice(sw_p1);
    Names::Add("SW#01", sw_p1);

    // Switch Device Configuration
    Ptr<SwitchNetDevice> sw_dev = CreateObject<SwitchNetDevice>();
    sw_dev->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(2)));
    sw_dev->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(5)));
    sw->AddDevice(sw_dev);
    sw_dev->AddSwitchPort(sw_p0);
    sw_dev->AddSwitchPort(sw_p1);

    // Data Rate configuration
    es0_p0->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));
    sw_p0->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));
    es1_p0->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));
    sw_p1->SetAttribute("DataRate", DataRateValue(DataRate("120Mbps")));

    // Full-duplex channel
    Ptr<EthernetChannel> ch_es0_sw = CreateObject<EthernetChannel>();
    es0_p0->Attach(ch_es0_sw);
    sw_p0->Attach(ch_es0_sw);

    Ptr<EthernetChannel> ch_es1_sw = CreateObject<EthernetChannel>();
    es1_p0->Attach(ch_es1_sw);
    sw_p1->Attach(ch_es1_sw);

    // Mac address configuration
    es0_p0->SetAddress(Mac48Address("00:00:00:00:00:01"));
    Mac48Address es1_mac = Mac48Address("00:00:00:00:00:02");
    es1_p0->SetAddress(es1_mac);
    sw_p0->SetAddress(Mac48Address("00:00:00:00:00:11"));
    sw_p1->SetAddress(Mac48Address("00:00:00:00:00:12"));

    // Creation of the queues
    for (int i = 0; i < 8; i++)
    {
        es0_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        es1_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        sw_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    if (shaper == "CBS")
    {
        Ptr<Cbs> cbs = CreateObject<Cbs>();
        cbs->SetTsnNetDevice(sw_p1);
        cbs->SetAttribute("IdleSlope", DataRateValue(DataRate("40Mbps")));
        cbs->SetAttribute("portTransmitRate", DataRateValue(DataRate("120Mbps")));

        sw_p1->SetQueue(CreateObject<DropTailQueue<Packet>>());      // FIFO 0
        sw_p1->SetQueue(CreateObject<DropTailQueue<Packet>>(), cbs); // FIFO 1
        for (int i = 0; i < 6; i++)
        {
            sw_p1->SetQueue(CreateObject<DropTailQueue<Packet>>()); // FIFO 2 to 7
        }

        cbs->TraceConnectWithoutContext("Credit", MakeCallback(&CbsCreditCallback));
    }
    else
    {
        sw_p1->SetAttribute("isAtsEnabled", BooleanValue(true));
        Ptr<Ats> ats = sw_p1->GetAts();
        ats->SetClock(clock0);
        ats->SetAttribute("MaxResidenceTime", TimeValue(Seconds(1)));

        Ptr<AtsSchedulerGroup> ats_group = ats->GetGroupForBridge(sw_p0, sw_p1, 0);
        ats_group->SetAttribute("DefaultCir", DataRateValue(DataRate("40Mbps")));
        ats_group->SetAttribute("DefaultCbs", UintegerValue(3200));

        for (int i = 0; i < 8; i++)
        {
            sw_p1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        }

        ats_group->TraceConnectWithoutContext("EligibilityTime", MakeCallback(&AtsEligibilityCallback));
    }

    // Configure TAS schedule
    sw_p1->AddGclEntry(Time(Seconds(1.0)), 0); // All gates are close
    sw_p1->AddGclEntry(Time(Seconds(2.0)), 2); // Only the gate of the FIFO 1 is open
    sw_p1->StartTas();

    // Application configuration
    sw_dev->AddForwardingTableEntry(es1_mac, 100, {sw_p1});

    std::string contextTx = Names::FindName(sw) + ":" + Names::FindName(sw_p1);
    sw_p1->TraceConnectWithoutContext("MacTx", MakeBoundCallback(&MacTxCallback, contextTx));
    sw_p1->TraceConnectWithoutContext("PhyTxBegin", MakeBoundCallback(&PhyTxBeginCallback, contextTx));

    Ptr<EthernetGenerator> app = CreateObject<EthernetGenerator>();
    app->Setup(es0_p0);
    app->SetAttribute("Address", AddressValue(es1_mac));
    app->SetAttribute("BurstSize", UintegerValue(4));
    app->SetAttribute("PayloadSize", UintegerValue(378));
    app->SetAttribute("Period", TimeValue(Seconds(3)));
    app->SetAttribute("VlanID", UintegerValue(100));
    app->SetAttribute("PCP", UintegerValue(1));
    es0->AddApplication(app);
    app->SetStartTime(Seconds(0.0));
    app->SetStopTime(Seconds(3.0));

    // Execute the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::ScheduleDestroy(&WritePacketMetricsToCsv);
    Simulator::Run();
    Simulator::Destroy();

    csvShaperMetric.close();
    return 0;
}