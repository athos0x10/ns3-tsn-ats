/**
 * @file packet-size-experimentation.cc
 * @author Arthur
 * @brief This program give the full experimentation to compare CBS/ATS
 * specificaly for packet size.
 * @date 2026-07-13
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

NS_LOG_COMPONENT_DEFINE("PacketSizeExperimentation");

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

void AtsTokensCallback(double newTokens)
{
    csvShaperMetric << Simulator::Now().GetSeconds() << "," << newTokens << "\n";
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
}

int main(int argc, char *argv[])
{
    std::string shaper = "CBS"; // Options: ATS or CBS
    uint8_t scenario = 1;       // Options: {1,2,3,4}

    LogComponentEnable("PacketSizeExperimentation", LOG_LEVEL_INFO);
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);

    CommandLine cmd;
    cmd.AddValue("shaper", "Choice of the shaper (ATS or CBS)", shaper);
    cmd.AddValue("scenario", "Scenario's number (1 to 4)", scenario);
    cmd.Parse(argc, argv);

    csvPackets.open("packets_metrics_s" + std::to_string(scenario) + "_" + shaper + ".csv");
    csvPackets << "Packet_UID,Arrival_Time,MacTx_Time,PhyTxBegin_Time,Queue_Delay,Eligibility_Time,Inter_Departure_Time\n";

    csvShaperMetric.open("shaper_state_s" + std::to_string(scenario) + "_" + shaper + ".csv");
    if (shaper == "CBS")
    {
        csvShaperMetric << "Time,CBS_Credit\n";
    }
    else
    {
        csvShaperMetric << "Time,ATS_Tokens\n";
    }

    // Creation of the nodes
    Ptr<TsnNode> es0 = CreateObject<TsnNode>();
    Names::Add("Sender", es0);

    Ptr<TsnNode> es1 = CreateObject<TsnNode>();
    Names::Add("Receiver", es1);

    Ptr<TsnNode> sw = CreateObject<TsnNode>();
    Names::Add("Switch", sw);

    // Clock configuration
    Ptr<Clock> clock0 = CreateObject<Clock>();
    Ptr<Clock> clock1 = CreateObject<Clock>();
    Ptr<Clock> clock2 = CreateObject<Clock>();
    es0->AddClock(clock0);
    es1->AddClock(clock1);
    sw->AddClock(clock2);

    // Creation of the ports
    Ptr<TsnNetDevice> es0_p0 = CreateObject<TsnNetDevice>();
    es0->AddDevice(es0_p0);
    Names::Add("Sender#00", es0_p0);

    Ptr<TsnNetDevice> es1_p0 = CreateObject<TsnNetDevice>();
    es1->AddDevice(es1_p0);
    Names::Add("Receiver#00", es1_p0);

    Ptr<TsnNetDevice> sw_p0 = CreateObject<TsnNetDevice>();
    sw->AddDevice(sw_p0);
    Names::Add("Switch#00", sw_p0);

    Ptr<TsnNetDevice> sw_p1 = CreateObject<TsnNetDevice>();
    sw->AddDevice(sw_p1);
    Names::Add("Switch#01", sw_p1);

    // Switch device configuration
    Ptr<SwitchNetDevice> sw_dev = CreateObject<SwitchNetDevice>();
    sw_dev->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(2)));
    sw_dev->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(5)));
    sw->AddDevice(sw_dev);
    sw_dev->AddSwitchPort(sw_p0);
    sw_dev->AddSwitchPort(sw_p1);

    // Data rate configuration
    es0_p0->SetAttribute("DataRate", DataRateValue(DataRate("90Mbps")));
    sw_p0->SetAttribute("DataRate", DataRateValue(DataRate("90Mbps")));

    es1_p0->SetAttribute("DataRate", DataRateValue(DataRate("90Mbps")));
    sw_p1->SetAttribute("DataRate", DataRateValue(DataRate("90Mbps")));

    // Create full-duplex channel
    Ptr<EthernetChannel> ch_es0_sw = CreateObject<EthernetChannel>();
    es0_p0->Attach(ch_es0_sw);
    sw_p0->Attach(ch_es0_sw);

    Ptr<EthernetChannel> ch_sw_es1 = CreateObject<EthernetChannel>();
    es1_p0->Attach(ch_sw_es1);
    sw_p1->Attach(ch_sw_es1);

    // Mac address configuration
    es0_p0->SetAddress(Mac48Address::Allocate());

    Mac48Address es1_mac = Mac48Address::Allocate();
    es1_p0->SetAddress(es1_mac);

    sw_p0->SetAddress(Mac48Address::Allocate());
    sw_p1->SetAddress(Mac48Address::Allocate());

    // Creation of the queue
    es0_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
    es1_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());
    sw_p0->SetQueue(CreateObject<DropTailQueue<Packet>>());

    if (shaper == "CBS")
    {
        Ptr<Cbs> cbs = CreateObject<Cbs>();
        cbs->SetTsnNetDevice(sw_p1);
        cbs->SetAttribute("IdleSlope", DataRateValue(DataRate("30Mbps")));
        cbs->SetAttribute("portTransmitRate", DataRateValue(DataRate("90Mb/s")));
        sw_p1->SetQueue(CreateObject<DropTailQueue<Packet>>(), cbs);
        cbs->TraceConnectWithoutContext("Credit", MakeCallback(&CbsCreditCallback));
    }
    else
    {
        sw_p1->SetAttribute("isAtsEnabled", BooleanValue(true));
        Ptr<Ats> ats = sw_p1->GetAts();
        ats->SetClock(clock2);
        ats->SetAttribute("MaxResidenceTime", TimeValue(Seconds(1)));

        Ptr<AtsSchedulerGroup> ats_group = ats->GetGroupForBridge(sw_p0, sw_p1, 0);
        ats_group->SetAttribute("DefaultCir", DataRateValue(DataRate("30Mbps")));

        uint32_t cbsBits = 1600;
        if (scenario == 2)
        {
            cbsBits = 4000;
        }
        else if (scenario == 3 || scenario == 4)
        {
            cbsBits = 3200;
        }

        ats_group->SetAttribute("DefaultCbs", UintegerValue(cbsBits));
        sw_p1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        ats_group->TraceConnectWithoutContext("EligibilityTime", MakeCallback(&AtsEligibilityCallback));

        Ptr<AtsSchedulerInstance> ats_intance = ats_group->GetInstanceForStream(10);
        ats_intance->TraceConnectWithoutContext("Tokens", MakeCallback(&AtsTokensCallback));
    }

    // Stream Identification
    Ptr<NullStreamIdentificationFunction> sif1 = CreateObject<NullStreamIdentificationFunction>();
    uint16_t streamHandle1 = 10;
    sif1->SetAttribute("VlanID", UintegerValue(100));
    sif1->SetAttribute("Address", AddressValue(es1_mac));
    sw->AddStreamIdentificationFunction(streamHandle1, sif1, {sw_p0}, {}, {}, {});

    // Application configuration
    sw_dev->AddForwardingTableEntry(es1_mac, 100, {sw_p1});

    std::string contextTx = Names::FindName(sw) + ":" + Names::FindName(sw_p1);
    sw_p1->TraceConnectWithoutContext("MacTx", MakeBoundCallback(&MacTxCallback, contextTx));
    sw_p1->TraceConnectWithoutContext("PhyTxBegin", MakeBoundCallback(&PhyTxBeginCallback, contextTx));

    if (scenario == 1 || scenario == 2)
    {
        Ptr<EthernetGenerator> app = CreateObject<EthernetGenerator>();
        app->Setup(es0_p0);
        app->SetAttribute("Address", AddressValue(es1_mac));
        app->SetAttribute("BurstSize", UintegerValue(5));
        app->SetAttribute("PayloadSize", UintegerValue(178));
        app->SetAttribute("Period", TimeValue(Seconds(1)));
        app->SetAttribute("PCP", UintegerValue(0));
        app->SetAttribute("VlanID", UintegerValue(100));
        es0->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(1.1));
    }
    else if (scenario == 3 || scenario == 4)
    {
        std::vector<uint32_t> payloads;
        if (scenario == 3)
        {
            payloads = {378, 378, 178};
        }
        else
        {
            payloads = {378, 178, 378};
        }

        Time startTime = Seconds(1.0);
        for (uint32_t payload : payloads)
        {
            Ptr<EthernetGenerator> app = CreateObject<EthernetGenerator>();
            app->Setup(es0_p0);
            app->SetAttribute("Address", AddressValue(es1_mac));
            app->SetAttribute("BurstSize", UintegerValue(1));
            app->SetAttribute("PayloadSize", UintegerValue(payload));

            app->SetAttribute("Period", TimeValue(Seconds(1)));
            app->SetAttribute("PCP", UintegerValue(0));
            app->SetAttribute("VlanID", UintegerValue(100));
            es0->AddApplication(app);

            app->SetStartTime(startTime);
            app->SetStopTime(startTime + MicroSeconds(100));
        }
    }

    // Execute the simulation
    Simulator::ScheduleDestroy(&WritePacketMetricsToCsv);
    Simulator::Run();
    Simulator::Destroy();

    csvShaperMetric.close();
    return 0;
}