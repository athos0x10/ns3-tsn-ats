/**
 * @file example-network-callback.cc
 * @author Arthur
 * @brief Network example from the tutorial with callback.
 *
 * @date 2026-07-29
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/tsn-module.h"
#include "ns3/ethernet-module.h"
#include "ns3/traffic-generator-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("Chapter 3");

// A callback to log the pkt emission
static void
MacTxCallback(std::string context, Ptr<const Packet> p)
{
    Ptr<Packet> pkt = p->Copy();
    EthernetHeader2 ethHeader;
    pkt->RemoveHeader(ethHeader);
    if (ethHeader.GetVid() == 1)
    {
        NS_LOG_INFO((Simulator::Now()).As(Time::S) << " \t" << context << " : Pkt #" << p->GetUid() << "(" << p->GetSize() << "bytes) sent!");
    }
}

// A callback to log the pkt reception
static void
MacRxCallback(std::string context, Ptr<const Packet> p)
{
    Ptr<Packet> pkt = p->Copy();
    EthernetHeader2 ethHeader;
    pkt->RemoveHeader(ethHeader);
    if (ethHeader.GetVid() == 1)
    {
        NS_LOG_INFO((Simulator::Now()).As(Time::S) << " \t" << context << " : Pkt #" << p->GetUid() << "(" << p->GetSize() << "bytes) received!");
    }
}

// A callback to log clock offset after correction
static void
ClockAfterCorrectionCallback(std::string context, Time clockValue)
{
    NS_LOG_INFO("[GPTP] At " << Simulator::Now() << " on " << context << " clock value after correction = " << clockValue.GetNanoSeconds() << "ns (error = " << (Simulator::Now() - clockValue).GetNanoSeconds() << "ns)");
}

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("Chapter 3", LOG_LEVEL_INFO);

    // Create four nodes
    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Names::Add("ES1", n0);
    Ptr<TsnNode> n1 = CreateObject<TsnNode>();
    Names::Add("ES2", n1);
    Ptr<TsnNode> n2 = CreateObject<TsnNode>();
    Names::Add("ES3", n2);
    Ptr<TsnNode> n3 = CreateObject<TsnNode>();
    Names::Add("SW", n3);

    // Create and add clocks to TsnNodes
    Ptr<Clock> c0 = CreateObject<Clock>(); // perfect clock because Grandmaster
    n0->SetMainClock(c0);
    Ptr<ConstantDriftClock> c1 = CreateObject<ConstantDriftClock>();
    c1->SetAttribute("InitialOffset", TimeValue(Seconds(20)));
    c1->SetAttribute("DriftRate", DoubleValue(-50));
    c1->SetAttribute("Granularity", TimeValue(NanoSeconds(10)));
    n1->SetMainClock(c1);
    Ptr<ConstantDriftClock> c2 = CreateObject<ConstantDriftClock>();
    c2->SetAttribute("InitialOffset", TimeValue(Seconds(3)));
    c2->SetAttribute("DriftRate", DoubleValue(2));
    c2->SetAttribute("Granularity", TimeValue(NanoSeconds(10)));
    n2->SetMainClock(c2);
    Ptr<ConstantDriftClock> c3 = CreateObject<ConstantDriftClock>();
    c3->SetAttribute("InitialOffset", TimeValue(Seconds(0.5)));
    c3->SetAttribute("DriftRate", DoubleValue(-25));
    c3->SetAttribute("Granularity", TimeValue(NanoSeconds(10)));
    n3->SetMainClock(c3);

    // Create and add a netDevice to each end-station node
    Ptr<TsnNetDevice> net0 = CreateObject<TsnNetDevice>();
    net0->SetAttribute("DataRate", DataRateValue(DataRate("100Mb/s")));
    n0->AddDevice(net0);
    Names::Add("ES1#01", net0);
    Ptr<TsnNetDevice> net1 = CreateObject<TsnNetDevice>();
    net1->SetAttribute("DataRate", DataRateValue(DataRate("100Mb/s")));
    n1->AddDevice(net1);
    Names::Add("ES2#01", net1);
    Ptr<TsnNetDevice> net2 = CreateObject<TsnNetDevice>();
    net2->SetAttribute("DataRate", DataRateValue(DataRate("100Mb/s")));
    n2->AddDevice(net2);
    Names::Add("ES3#01", net2);
    // Create and add a netDevice to each switch port
    Ptr<TsnNetDevice> swnet0 = CreateObject<TsnNetDevice>();
    swnet0->SetAttribute("DataRate", DataRateValue(DataRate("100Mb/s")));
    n3->AddDevice(swnet0);
    Names::Add("SW#01", swnet0);
    Ptr<TsnNetDevice> swnet1 = CreateObject<TsnNetDevice>();
    swnet1->SetAttribute("DataRate", DataRateValue(DataRate("100Mb/s")));
    n3->AddDevice(swnet1);
    Names::Add("SW#02", swnet1);
    Ptr<TsnNetDevice> swnet2 = CreateObject<TsnNetDevice>();
    swnet2->SetAttribute("DataRate", DataRateValue(DataRate("100Mb/s")));
    n3->AddDevice(swnet2);
    Names::Add("SW#03", swnet2);

    // Create Ethernet Channels and connect switch to the end-stations
    Ptr<EthernetChannel> channel0 = CreateObject<EthernetChannel>();
    channel0->SetAttribute("Delay", TimeValue(NanoSeconds(50)));
    net0->Attach(channel0);
    swnet0->Attach(channel0);
    Ptr<EthernetChannel> channel1 = CreateObject<EthernetChannel>();
    channel1->SetAttribute("Delay", TimeValue(NanoSeconds(75)));
    net1->Attach(channel1);
    swnet1->Attach(channel1);
    Ptr<EthernetChannel> channel2 = CreateObject<EthernetChannel>();
    channel2->SetAttribute("Delay", TimeValue(NanoSeconds(100)));
    net2->Attach(channel2);
    swnet2->Attach(channel2);

    // Create and add a switch net device to the switch node
    Ptr<SwitchNetDevice> sw = CreateObject<SwitchNetDevice>();
    sw->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(2)));
    sw->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(5)));
    n3->AddDevice(sw);
    sw->AddSwitchPort(swnet0);
    sw->AddSwitchPort(swnet1);
    sw->AddSwitchPort(swnet2);

    // Allocate Mac addresses to the netDevices
    net0->SetAddress(Mac48Address::Allocate());
    net1->SetAddress(Mac48Address::Allocate());
    net2->SetAddress(Mac48Address::Allocate());
    sw->SetAddress(Mac48Address::Allocate());

    // Create 8 output port FIFOs for each netDevice.
    Ptr<Cbs> cbs = CreateObject<Cbs>();
    cbs->SetTsnNetDevice(net0);
    cbs->SetAttribute("IdleSlope", DataRateValue(DataRate("20Kb/s")));
    cbs->SetAttribute("portTransmitRate", DataRateValue(DataRate("100Mb/s")));
    net0->SetQueue(CreateObject<DropTailQueue<Packet>>());      // FIFO 0
    net0->SetQueue(CreateObject<DropTailQueue<Packet>>(), cbs); // FIFO 1
    for (int i = 0; i < 6; i++)
    {
        net0->SetQueue(CreateObject<DropTailQueue<Packet>>()); // FIFO 0
    }

    for (int i = 0; i < 8; i++)
    {
        net1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net2->SetQueue(CreateObject<DropTailQueue<Packet>>());
        swnet0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        swnet1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        swnet2->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // Add and configure gPTP
    Ptr<GPTP> gPTP0 = CreateObject<GPTP>();
    gPTP0->SetNode(n0);
    gPTP0->SetMainClock(c0);
    gPTP0->AddDomain(0);
    gPTP0->AddPort(net0, GPTP::MASTER, 0);
    gPTP0->SetAttribute("SyncInterval", TimeValue(Seconds(0.125))); // This line is not mandatory because 0.125s is the default value
    gPTP0->SetAttribute("PdelayInterval", TimeValue(Seconds(1)));   // This line is not mandatory because 1s is the default value
    gPTP0->SetAttribute("Priority", UintegerValue(7));
    n0->AddApplication(gPTP0);
    gPTP0->SetStartTime(Seconds(0));
    Ptr<GPTP> gPTP1 = CreateObject<GPTP>();
    gPTP1->SetNode(n1);
    gPTP1->SetMainClock(c1);
    gPTP1->AddDomain(0);
    gPTP1->AddPort(net1, GPTP::SLAVE, 0);
    gPTP1->SetAttribute("Priority", UintegerValue(7));
    n1->AddApplication(gPTP1);
    gPTP1->SetStartTime(Seconds(0));
    Ptr<GPTP> gPTP2 = CreateObject<GPTP>();
    gPTP2->SetNode(n2);
    gPTP2->SetMainClock(c2);
    gPTP2->AddDomain(0);
    gPTP2->AddPort(net2, GPTP::SLAVE, 0);
    gPTP2->SetAttribute("Priority", UintegerValue(7));
    n2->AddApplication(gPTP2);
    gPTP2->SetStartTime(Seconds(0));
    Ptr<GPTP> gPTP3 = CreateObject<GPTP>();
    gPTP3->SetNode(n3);
    gPTP3->SetMainClock(c3);
    gPTP3->AddDomain(0);
    gPTP3->AddPort(swnet0, GPTP::SLAVE, 0);
    gPTP3->AddPort(swnet1, GPTP::MASTER, 0);
    gPTP3->AddPort(swnet2, GPTP::MASTER, 0);
    gPTP3->SetAttribute("Priority", UintegerValue(7));
    n3->AddApplication(gPTP3);
    gPTP3->SetStartTime(Seconds(0));

    // Configure TAS schedule
    swnet2->AddGclEntry(Time(Seconds(2)), 0); // All gates are close
    swnet2->AddGclEntry(Time(Seconds(3)), 2); // Only the gate of the FIFO 1 is open
    swnet2->StartTas();

    // Add a stream identification function
    Ptr<NullStreamIdentificationFunction> sif0 = CreateObject<NullStreamIdentificationFunction>();
    uint16_t StreamHandle = 10;
    sif0->SetAttribute("VlanID", UintegerValue(1));
    sif0->SetAttribute("Address", AddressValue(net2->GetAddress()));
    n3->AddStreamIdentificationFunction(StreamHandle, sif0, {swnet0}, {}, {}, {});

    // PSFP configuration
    Ptr<StreamFilterInstance> sfi0 = CreateObject<StreamFilterInstance>();
    sfi0->SetAttribute("StreamHandle", IntegerValue(StreamHandle));
    sfi0->SetAttribute("Priority", IntegerValue(-1)); //-1 = wildcard
    sfi0->SetAttribute("MaxSDUSize", UintegerValue(1422));
    n3->AddStreamFilter(sfi0);
    Ptr<FlowMeterInstance> fm0 = CreateObject<FlowMeterInstance>();
    fm0->SetAttribute("CIR", DataRateValue(DataRate("20Kb/s")));
    fm0->SetAttribute("CBS", UintegerValue(1400));
    fm0->SetAttribute("DropOnYellow", BooleanValue(true));
    fm0->SetAttribute("MarkAllFramesRedEnable", BooleanValue(false));
    uint16_t fmid = n3->AddFlowMeter(fm0);
    sfi0->AddFlowMeterInstanceId(fmid);

    // Sequencing : Sequence generation
    Ptr<SequenceGenerationFunction> seqf0 = CreateObject<SequenceGenerationFunction>();
    seqf0->SetAttribute("Direction", BooleanValue(false)); // in-facing
    seqf0->SetStreamHandle({StreamHandle});
    n3->AddSequenceGenerationFunction(seqf0);
    // Sequence encode
    Ptr<SequenceEncodeDecodeFunction> seqEnc0 = CreateObject<SequenceEncodeDecodeFunction>();
    seqEnc0->SetAttribute("Direction", BooleanValue(false)); // in-facing
    seqEnc0->SetAttribute("Active", BooleanValue(true));
    seqEnc0->SetStreamHandle({StreamHandle});
    seqEnc0->SetPort(swnet0);
    n3->AddSequenceEncodeDecodeFunction(seqEnc0);

    // Add a forwarding table entry
    sw->AddForwardingTableEntry(Mac48Address::ConvertFrom(net2->GetAddress()), 1, {swnet1, swnet2});

    // Enable ATS on the Switch Egress Port targeting ES3
    swnet2->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> swAts = swnet2->GetAts();
    swAts->SetClock(c3); // Bind to SW's local clock
    swAts->SetPriorityActivation(1, true);
    swAts->SetAttribute("MaxResidenceTime", TimeValue(Seconds(1))); // Drop if packets wait too long

    // Retrieve the Bridge Scheduler Group based on Per-Port-Per-Priority routing
    // (Ingress Port swnet0 -> Egress Port swnet2, Priority PCP 1)
    Ptr<AtsSchedulerGroup> atsGroup = swAts->GetGroupForBridge(swnet0, swnet2, 1);

    // Create a dedicated ATS Shaper Instance with custom CIR and CBS rates
    uint32_t instanceId = atsGroup->CreateAtsInstance(DataRate("15Mbps"), 32768);

    // Bind the existing 802.1CB StreamHandle (10) directly to this Shaper Instance
    atsGroup->BindStreamToInstance(StreamHandle, instanceId);

    // Application description
    // ES1 -> ES3 with priority 1
    Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
    app0->Setup(net0);
    app0->SetAttribute("Address", AddressValue(net2->GetAddress()));
    app0->SetAttribute("BurstSize", UintegerValue(5));
    app0->SetAttribute("PayloadSize", UintegerValue(1400));
    app0->SetAttribute("Period", TimeValue(Seconds(5)));
    app0->SetAttribute("VlanID", UintegerValue(1));
    app0->SetAttribute("PCP", UintegerValue(1));
    n0->AddApplication(app0);
    app0->SetStartTime(Seconds(0));
    app0->SetStopTime(Seconds(10));

    // Callback declarations
    // Callback to display the packet sent log
    std::string context = Names::FindName(n0) + ":" + Names::FindName(net0);
    net0->TraceConnectWithoutContext("MacTx", MakeBoundCallback(&MacTxCallback, context));
    // Callback to display the packet received log
    context = Names::FindName(n2) + ":" + Names::FindName(net2);
    net2->TraceConnectWithoutContext("MacRx", MakeBoundCallback(&MacRxCallback, context));
    // Callback to display clock offset after correction
    gPTP1->TraceConnectWithoutContext("ClockAfterCorrection", MakeBoundCallback(&ClockAfterCorrectionCallback, Names::FindName(n1)));
    gPTP2->TraceConnectWithoutContext("ClockAfterCorrection", MakeBoundCallback(&ClockAfterCorrectionCallback, Names::FindName(n2)));
    gPTP3->TraceConnectWithoutContext("ClockAfterCorrection", MakeBoundCallback(&ClockAfterCorrectionCallback, Names::FindName(n3)));

    // Execute the simulation
    NS_LOG_INFO("Start of the simulation");
    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("End of the simulation");
    return 0;
}
