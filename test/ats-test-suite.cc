#include "ns3/tsn-net-device.h"
#include "ns3/ats.h"
#include "ns3/ethernet-channel.h"
#include "ns3/ethernet-header2.h"
#include "ns3/ethernet-generator.h"
#include "ns3/stream-identification-function-null.h"

// An essential include is test.h
#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/timestamp-tag.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("AtsTestSuite");

/**
 * \defgroup Ats-tests Tests for Asynchronous Traffic Shaping (ATS)
 * \ingroup tsn
 * \ingroup tests
 */

/**
 * \ingroup Ats-tests
 * \brief Check if packets cross a TSN channel successfully using Per-Priority ATS routing.
 */
class AtsBasicTestCase : public TestCase
{
public:
    AtsBasicTestCase(uint64_t pktSize, uint64_t expected_receive, uint8_t pcp, DataRate cir, uint32_t cbs);
    virtual ~AtsBasicTestCase();

private:
    void DoRun() override;
    void SendTx(Ptr<const Packet> p);
    void ReceiveRx(Ptr<const Packet> p);

    uint64_t m_sent{0};     //!< Number of bytes sent
    uint64_t m_received{0}; //!< Number of bytes received
    uint64_t m_pktSize;
    uint64_t m_expected_receive;
    uint8_t m_pcp;
    DataRate m_cir;
    uint32_t m_cbs;
};

AtsBasicTestCase::AtsBasicTestCase(uint64_t pktSize, uint64_t expected_receive, uint8_t pcp, DataRate cir, uint32_t cbs)
    : TestCase("Check if packets cross a point-to-point ethernet channel with Per-Priority ATS Scheduler")
{
    m_pktSize = pktSize;
    m_expected_receive = expected_receive;
    m_pcp = pcp;
    m_cir = cir;
    m_cbs = cbs;
}

AtsBasicTestCase::~AtsBasicTestCase() {}

void AtsBasicTestCase::SendTx(Ptr<const Packet> p) { m_sent += p->GetSize(); }
void AtsBasicTestCase::ReceiveRx(Ptr<const Packet> p) { m_received += p->GetSize(); }

void AtsBasicTestCase::DoRun()
{
    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Ptr<TsnNode> n1 = CreateObject<TsnNode>();

    Ptr<Clock> clock0 = CreateObject<Clock>();
    Ptr<Clock> clock1 = CreateObject<Clock>();

    n0->SetMainClock(clock0);
    n1->SetMainClock(clock1);
    n0->AddClock(clock0);
    n1->AddClock(clock1);
    n0->setActiveClock(0);
    n1->setActiveClock(0);

    Ptr<TsnNetDevice> net0 = CreateObject<TsnNetDevice>();
    n0->AddDevice(net0);

    Ptr<TsnNetDevice> net1 = CreateObject<TsnNetDevice>();
    n1->AddDevice(net1);

    Ptr<EthernetChannel> channel = CreateObject<EthernetChannel>();
    net0->Attach(channel);
    net1->Attach(channel);

    net0->SetAddress(Mac48Address::Allocate());
    net1->SetAddress(Mac48Address::Allocate());

    for (int i = 0; i < 8; i++)
    {
        net0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    net0->SetAttribute("isAtsEnabled", BooleanValue(true));

    Ptr<Ats> ats = net0->GetAts();
    NS_TEST_ASSERT_MSG_NE(ats, nullptr, "ATS Object inside TsnNetDevice should not be null");

    ats->SetClock(clock0);

    Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
    app0->Setup(net0);
    app0->SetAttribute("BurstSize", UintegerValue(1));
    app0->SetAttribute("PayloadSize", UintegerValue(m_pktSize));
    app0->SetAttribute("Period", TimeValue(Seconds(1)));
    app0->SetAttribute("VlanID", UintegerValue(1));
    app0->SetAttribute("PCP", UintegerValue(m_pcp));
    n0->AddApplication(app0);
    app0->SetStartTime(Seconds(0));
    app0->SetStopTime(Seconds(2));

    net0->TraceConnectWithoutContext("MacTx", MakeCallback(&AtsBasicTestCase::SendTx, this));
    net1->TraceConnectWithoutContext("MacRx", MakeCallback(&AtsBasicTestCase::ReceiveRx, this));

    Simulator::Stop(Seconds(3));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_GT(m_sent, 0, "Packets must successfully leave the generator application");
    NS_TEST_ASSERT_MSG_EQ(m_received, m_sent, "All packets processed by the ATS shaper must arrive completely without drops");
}

/**
 * \ingroup Ats-tests
 * \brief Test case configured to force packet drops due to MaxResidenceTime violations.
 */
class AtsPolicingDropTestCase : public TestCase
{
public:
    AtsPolicingDropTestCase(uint64_t pktSize, uint32_t totalBurst, uint32_t expectedPackets, uint8_t pcp, Time maxResidenceTime, Time period);
    virtual ~AtsPolicingDropTestCase();

private:
    void DoRun() override;
    void ReceiveRx(Ptr<const Packet> p);

    uint32_t m_receivedCount{0}; //!< Number of packets received
    uint64_t m_pktSize;          //!< Payload size configured in the generator
    uint32_t m_totalBurst;       //!< Total number of generated frames in the burst
    uint32_t m_expectedPackets;  //!< Total number of compliant packets expected at destination
    uint8_t m_pcp;               //!< Priority Code Point (PCP) values
    Time m_maxResidenceTime;     //!< Maximum allowable latency inside the scheduler bucket
    Time m_period;               //!< Interval between packet generations
};

AtsPolicingDropTestCase::AtsPolicingDropTestCase(uint64_t pktSize, uint32_t totalBurst, uint32_t expectedPackets, uint8_t pcp, Time maxResidenceTime, Time period)
    : TestCase("Verify ATS policing rule where traffic exceeds the MaxResidenceTime ceiling and drops non-compliant packets")
{
    m_pktSize = pktSize;
    m_totalBurst = totalBurst;
    m_expectedPackets = expectedPackets;
    m_pcp = pcp;
    m_maxResidenceTime = maxResidenceTime;
    m_period = period;
}

AtsPolicingDropTestCase::~AtsPolicingDropTestCase() {}

void AtsPolicingDropTestCase::ReceiveRx(Ptr<const Packet> p)
{
    m_receivedCount++;
}

void AtsPolicingDropTestCase::DoRun()
{
    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Ptr<TsnNode> n1 = CreateObject<TsnNode>();

    Ptr<Clock> clock0 = CreateObject<Clock>();
    Ptr<Clock> clock1 = CreateObject<Clock>();

    n0->SetMainClock(clock0);
    n1->SetMainClock(clock1);
    n0->AddClock(clock0);
    n1->AddClock(clock1);
    n0->setActiveClock(0);
    n1->setActiveClock(0);

    Ptr<TsnNetDevice> net0 = CreateObject<TsnNetDevice>();
    n0->AddDevice(net0);
    Ptr<TsnNetDevice> net1 = CreateObject<TsnNetDevice>();
    n1->AddDevice(net1);

    // High line-rate to isolate ATS shaping delays from transmission bottlenecking
    net0->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    net1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    Ptr<EthernetChannel> channel = CreateObject<EthernetChannel>();
    net0->Attach(channel);
    net1->Attach(channel);

    net0->SetAddress(Mac48Address::Allocate());
    net1->SetAddress(Mac48Address::Allocate());

    for (int i = 0; i < 8; i++)
    {
        net0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // ATS Core setup
    net0->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> ats = net0->GetAts();
    NS_TEST_ASSERT_MSG_NE(ats, nullptr, "ATS Object inside TsnNetDevice should not be null");

    ats->SetClock(clock0);
    // Apply the custom Maximum Residence Time restriction passed by the test suite
    ats->SetAttribute("MaxResidenceTime", TimeValue(m_maxResidenceTime));

    // Configure the generator for a single dense flash burst
    Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
    app0->Setup(net0);
    app0->SetAttribute("BurstSize", UintegerValue(m_totalBurst));
    app0->SetAttribute("PayloadSize", UintegerValue(m_pktSize));
    app0->SetAttribute("Period", TimeValue(m_period));
    app0->SetAttribute("VlanID", UintegerValue(1));
    app0->SetAttribute("PCP", UintegerValue(m_pcp));
    n0->AddApplication(app0);

    // Immediate operational window cutoff to enforce only one burst event execution
    app0->SetStartTime(MilliSeconds(0));
    app0->SetStopTime(MilliSeconds(0) + MicroSeconds(2));

    net1->TraceConnectWithoutContext("MacRx", MakeCallback(&AtsPolicingDropTestCase::ReceiveRx, this));

    // Provide enough execution time for shaped/delayed packets to safely arrive
    Simulator::Stop(MilliSeconds(1000));
    Simulator::Run();
    Simulator::Destroy();

    // Final assertion check
    NS_TEST_ASSERT_MSG_EQ(m_receivedCount, m_expectedPackets,
                          "ATS policing failed! Expected exactly " << m_expectedPackets << " packets but received " << m_receivedCount);
}

/**
 * \ingroup Ats-tests
 * \brief Test case validating multi-application isolation inside an End-Station.
 */
class AtsMultiAppIsolationTestCase : public TestCase
{
public:
    AtsMultiAppIsolationTestCase();
    virtual ~AtsMultiAppIsolationTestCase() {}

private:
    void DoRun() override;
    void ReceiveRx(Ptr<const Packet> p);

    uint32_t m_receivedCount{0};
};

AtsMultiAppIsolationTestCase::AtsMultiAppIsolationTestCase()
    : TestCase("Verify End-Station ATS isolates concurrent bursts using unique dynamic stream ID shaper instances") {}

void AtsMultiAppIsolationTestCase::ReceiveRx(Ptr<const Packet> p)
{
    m_receivedCount++;
}

void AtsMultiAppIsolationTestCase::DoRun()
{
    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Ptr<TsnNode> n1 = CreateObject<TsnNode>();

    Ptr<Clock> clock0 = CreateObject<Clock>();
    Ptr<Clock> clock1 = CreateObject<Clock>();

    n0->SetMainClock(clock0);
    n1->SetMainClock(clock1);

    Ptr<TsnNetDevice> net0 = CreateObject<TsnNetDevice>();
    n0->AddDevice(net0);
    Ptr<TsnNetDevice> net1 = CreateObject<TsnNetDevice>();
    n1->AddDevice(net1);

    net0->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    net1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    Ptr<EthernetChannel> channel = CreateObject<EthernetChannel>();
    net0->Attach(channel);
    net1->Attach(channel);

    net0->SetAddress(Mac48Address::Allocate());
    net1->SetAddress(Mac48Address::Allocate());

    for (int i = 0; i < 8; i++)
    {
        net0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    net0->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> ats = net0->GetAts();
    ats->SetClock(clock0);
    ats->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(5)));

    // Application 1
    Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
    app1->Setup(net0);
    app1->SetAttribute("BurstSize", UintegerValue(2));
    app1->SetAttribute("VlanID", UintegerValue(1));
    app1->SetAttribute("PayloadSize", UintegerValue(500));
    app1->SetAttribute("Period", TimeValue(Seconds(1)));
    n0->AddApplication(app1);
    app1->SetStartTime(MilliSeconds(10));
    app1->SetStopTime(MilliSeconds(10) + MicroSeconds(2));

    // Application 2
    Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
    app2->Setup(net0);
    app2->SetAttribute("BurstSize", UintegerValue(2));
    app2->SetAttribute("VlanID", UintegerValue(2));
    app2->SetAttribute("PayloadSize", UintegerValue(500));
    app2->SetAttribute("Period", TimeValue(Seconds(1)));
    n0->AddApplication(app2);
    app2->SetStartTime(MilliSeconds(10));
    app2->SetStopTime(MilliSeconds(10) + MicroSeconds(2));

    net1->TraceConnectWithoutContext("MacRx", MakeCallback(&AtsMultiAppIsolationTestCase::ReceiveRx, this));

    Simulator::Stop(MilliSeconds(100));
    Simulator::Run();
    Simulator::Destroy();

    // If isolation works, both queues process 2 frames safely = 4 total packets received.
    NS_TEST_ASSERT_MSG_EQ(m_receivedCount, 4, "Multi-application streams incorrectly shared a common queue or experienced collision.");
}

/**
 * \ingroup Ats-tests
 * \brief Test case checking the shaping latency accuracy.
 */
class AtsShapingLatencyTestCase : public TestCase
{
public:
    AtsShapingLatencyTestCase();
    virtual ~AtsShapingLatencyTestCase() {}

private:
    void DoRun() override;
    void RecordRxTime(Ptr<const Packet> p);

    std::vector<Time> m_rxTimes;
    uint32_t m_localPacketCount{0};
};

AtsShapingLatencyTestCase::AtsShapingLatencyTestCase()
    : TestCase("Verify that ATS spaces back-to-back burst frames precisely matching the computed CIR recovery latency") {}

void AtsShapingLatencyTestCase::RecordRxTime(Ptr<const Packet> p)
{
    m_localPacketCount++;
    m_rxTimes.push_back(Simulator::Now());
}

void AtsShapingLatencyTestCase::DoRun()
{
    m_rxTimes.clear();
    m_localPacketCount = 0;

    // Initialisation of tsn node and clocks
    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Ptr<TsnNode> n1 = CreateObject<TsnNode>();

    Ptr<Clock> clock0 = CreateObject<Clock>();
    Ptr<Clock> clock1 = CreateObject<Clock>();

    n0->SetMainClock(clock0);
    n1->SetMainClock(clock1);

    // Creation of the device, and link them
    Ptr<TsnNetDevice> net0 = CreateObject<TsnNetDevice>();
    n0->AddDevice(net0);
    Ptr<TsnNetDevice> net1 = CreateObject<TsnNetDevice>();
    n1->AddDevice(net1);

    Ptr<EthernetChannel> channel = CreateObject<EthernetChannel>();
    net0->Attach(channel);
    net1->Attach(channel);

    net0->SetAddress(Mac48Address::Allocate());
    net1->SetAddress(Mac48Address::Allocate());

    for (int i = 0; i < 8; i++)
    {
        net0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // Activation of ATS on the emmiter
    net0->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> ats = net0->GetAts();
    ats->SetClock(clock0);
    ats->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(10)));

    net1->TraceConnectWithoutContext("MacRx", MakeCallback(&AtsShapingLatencyTestCase::RecordRxTime, this));

    Ptr<EthernetGenerator> app = CreateObject<EthernetGenerator>();
    app->Setup(net0);
    app->SetAttribute("BurstSize", UintegerValue(2));
    app->SetAttribute("PayloadSize", UintegerValue(500));
    app->SetAttribute("Period", TimeValue(Seconds(1)));

    n0->AddApplication(app);
    app->SetStartTime(MilliSeconds(0));
    app->SetStopTime(MilliSeconds(5));

    Simulator::Stop(MilliSeconds(100));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(m_rxTimes.size(), 2, "Shaping precision verification failed: Missing frames. Received " << m_rxTimes.size());

    Time deltaReceived = m_rxTimes[1] - m_rxTimes[0];
    double deltaUs = deltaReceived.GetMicroSeconds();
    // It is different than the drop test because we removed VlanId so the packet size is 518 bytes
    double expectedSpacingUs = 414.4;
    double toleranceUs = 0.5;

    NS_TEST_ASSERT_MSG_EQ_TOL(deltaUs, expectedSpacingUs, toleranceUs,
                              "ATS spacing delay drifts outside allowable bounds! " << "Measured: " << deltaUs << " us, Expected: " << expectedSpacingUs << " us");
}

/**
 * \ingroup Ats-tests
 * \brief Test case checking strict Stream ID isolation against a malicious stream at t=0ms.
 */
class AtsNoisyNeighborIsolationTestCase : public TestCase
{
public:
    AtsNoisyNeighborIsolationTestCase();
    virtual ~AtsNoisyNeighborIsolationTestCase() {}

private:
    void DoRun() override;
    void ReceiveRx(Ptr<const Packet> p);

    uint32_t m_stream10Count{0};
    uint32_t m_stream20Count{0};
};

AtsNoisyNeighborIsolationTestCase::AtsNoisyNeighborIsolationTestCase()
    : TestCase("Verify that a non-compliant malicious burst on Stream 10 does not cause drop propagation on compliant Stream 20") {}

void AtsNoisyNeighborIsolationTestCase::ReceiveRx(Ptr<const Packet> p)
{
    // Differentiate incoming streams via payload sizing profiles safely without header dependencies
    if (p->GetSize() == 522)
    {
        m_stream10Count++;
    }
    else if (p->GetSize() == 322)
    {
        m_stream20Count++;
    }
}

void AtsNoisyNeighborIsolationTestCase::DoRun()
{
    m_stream10Count = 0;
    m_stream20Count = 0;

    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Ptr<TsnNode> n1 = CreateObject<TsnNode>();

    Ptr<Clock> clock0 = CreateObject<Clock>();
    Ptr<Clock> clock1 = CreateObject<Clock>();
    n0->SetMainClock(clock0);
    n1->SetMainClock(clock1);

    Ptr<TsnNetDevice> net0 = CreateObject<TsnNetDevice>();
    n0->AddDevice(net0);
    Ptr<TsnNetDevice> net1 = CreateObject<TsnNetDevice>();
    n1->AddDevice(net1);

    net0->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    net1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    Ptr<EthernetChannel> channel = CreateObject<EthernetChannel>();
    net0->Attach(channel);
    net1->Attach(channel);

    net0->SetAddress(Mac48Address::Allocate());
    net1->SetAddress(Mac48Address::Allocate());

    for (int i = 0; i < 8; i++)
    {
        net0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    net0->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> ats = net0->GetAts();
    ats->SetClock(clock0);
    ats->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(1)));

    net1->TraceConnectWithoutContext("MacRx", MakeCallback(&AtsNoisyNeighborIsolationTestCase::ReceiveRx, this));

    // App 1: Noisy Neighbor -> Mass burst of 6 packets (causes policing drops)
    Ptr<EthernetGenerator> appMalicious = CreateObject<EthernetGenerator>();
    appMalicious->Setup(net0);
    appMalicious->SetAttribute("BurstSize", UintegerValue(6));
    appMalicious->SetAttribute("PayloadSize", UintegerValue(500));
    appMalicious->SetAttribute("Period", TimeValue(MicroSeconds(10)));
    appMalicious->SetAttribute("VlanID", UintegerValue(1));
    appMalicious->SetAttribute("PCP", UintegerValue(5));
    n0->AddApplication(appMalicious);

    // App 2: Compliant Flow -> Standard burst of 2 packets (must safely pass)
    Ptr<EthernetGenerator> appCompliant = CreateObject<EthernetGenerator>();
    appCompliant->Setup(net0);
    appCompliant->SetAttribute("BurstSize", UintegerValue(2));
    appCompliant->SetAttribute("PayloadSize", UintegerValue(300));
    appCompliant->SetAttribute("Period", TimeValue(MicroSeconds(10)));
    appCompliant->SetAttribute("VlanID", UintegerValue(2));
    appCompliant->SetAttribute("PCP", UintegerValue(5));
    n0->AddApplication(appCompliant);

    // Align start execution at t=0ms and restrict lifecycle window to block transmission cycles
    Time startTime = MilliSeconds(0);
    appMalicious->SetStartTime(startTime);
    appMalicious->SetStopTime(startTime + MicroSeconds(5));

    appCompliant->SetStartTime(startTime);
    appCompliant->SetStopTime(startTime + MicroSeconds(5));

    Simulator::Stop(MilliSeconds(50));
    Simulator::Run();
    Simulator::Destroy();

    // Verify that the compliant flow passed without drops, and the noisy flow was policed
    NS_TEST_ASSERT_MSG_EQ(m_stream10Count, 2, "Noisy Neighbor policing failure: Unexpected throughput on Stream 10");
    NS_TEST_ASSERT_MSG_EQ(m_stream20Count, 2, "ATS Segregation Failure: Noisy Neighbor induced drops or delay on compliant Stream 20");
}

/**
 * \ingroup Ats-tests
 * \brief Test case checking ATS shaper integration inside an intermediate SwitchNetDevice.
 * * \details This test illustrate this environement:
 * - 1 Source End-Station, 1 TSN Switch (SW1), 1 Destination End-Station.
 * - Source sends a high-rate back-to-back burst of 3 frames.
 * - SW1 looks up its L2 forwarding table and switches the packets to the egress port.
 * - Egress port applies ATS shaping (CIR = 10 Mbps).
 * - Target asserts that spacing matches exactly the required LengthRecovery delay.
 */
class AtsBridgeTransitTestCase : public TestCase
{
public:
    AtsBridgeTransitTestCase();
    virtual ~AtsBridgeTransitTestCase() {}

private:
    void DoRun() override;
    void RecordRxTime(Ptr<const Packet> p);

    std::vector<Time> m_rxTimes;
};

AtsBridgeTransitTestCase::AtsBridgeTransitTestCase()
    : TestCase("Verify that an ATS shaper embedded inside a SwitchNetDevice port reshapes transient bursty flows") {}

void AtsBridgeTransitTestCase::RecordRxTime(Ptr<const Packet> p)
{
    m_rxTimes.push_back(Simulator::Now());
}

void AtsBridgeTransitTestCase::DoRun()
{
    m_rxTimes.clear();

    // Instantiate nodes
    Ptr<TsnNode> nSource = CreateObject<TsnNode>();
    Ptr<TsnNode> nDest = CreateObject<TsnNode>();
    Ptr<TsnNode> nSw1 = CreateObject<TsnNode>();

    // Instantiate clocks
    Ptr<Clock> clockSource = CreateObject<Clock>();
    Ptr<Clock> clockSw1 = CreateObject<Clock>();
    Ptr<Clock> clockDest = CreateObject<Clock>();
    nSource->SetMainClock(clockSource);
    nSw1->SetMainClock(clockSw1);
    nDest->SetMainClock(clockDest);

    // Instantiate TSN interfaces
    Ptr<TsnNetDevice> netSource = CreateObject<TsnNetDevice>();
    nSource->AddDevice(netSource);
    Ptr<TsnNetDevice> netDest = CreateObject<TsnNetDevice>();
    nDest->AddDevice(netDest);

    // Switch NetDevices (Port 1 Ingress, Port 2 Egress)
    Ptr<TsnNetDevice> netSw1_1 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_1);
    Ptr<TsnNetDevice> netSw1_2 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_2);

    // Operational Line-rate (1 Gbps)
    netSource->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_2->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netDest->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    // Cable links configuration via EthernetChannel
    Ptr<EthernetChannel> chA = CreateObject<EthernetChannel>();
    netSource->Attach(chA);
    netSw1_1->Attach(chA);

    Ptr<EthernetChannel> chB = CreateObject<EthernetChannel>();
    netSw1_2->Attach(chB);
    netDest->Attach(chB);

    // Instantiate Layer-2 Infrastructure inside SW1
    Ptr<SwitchNetDevice> sw1 = CreateObject<SwitchNetDevice>();
    sw1->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(10)));
    sw1->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(10)));
    nSw1->AddDevice(sw1);
    sw1->AddSwitchPort(netSw1_1);
    sw1->AddSwitchPort(netSw1_2);

    netSource->SetAddress(Mac48Address::Allocate());
    netDest->SetAddress(Mac48Address::Allocate());
    sw1->SetAddress(Mac48Address::Allocate());

    // Allocating CoS Queues
    for (int i = 0; i < 8; i++)
    {
        netSource->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netDest->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_2->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // Set Bridging Rule for VLAN 100
    sw1->AddForwardingTableEntry(Mac48Address("ff:ff:ff:ff:ff:ff"), 100, {netSw1_2});

    // Stream Identification
    Ptr<NullStreamIdentificationFunction> sif0 = CreateObject<NullStreamIdentificationFunction>();
    uint16_t StreamHandle = 10;
    sif0->SetAttribute("VlanID", UintegerValue(100));
    sif0->SetAttribute("Address", AddressValue(Mac48Address("ff:ff:ff:ff:ff:ff")));
    nSw1->AddStreamIdentificationFunction(StreamHandle, sif0, {netSw1_1}, {}, {}, {});

    // Configure ATS Engine on Switch Egress Port
    netSw1_2->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> atsEngine = netSw1_2->GetAts();
    NS_TEST_ASSERT_MSG_NE(atsEngine, nullptr, "ATS Engine must be successfully instantiated on SW port 2");
    atsEngine->SetClock(clockSw1);
    atsEngine->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(5)));

    // Connect metrics capture trace
    netDest->TraceConnectWithoutContext("MacRx", MakeCallback(&AtsBridgeTransitTestCase::RecordRxTime, this));

    // Dense Burst Traffic Generator Profile
    Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
    app0->Setup(netSource);
    app0->SetAttribute("BurstSize", UintegerValue(1));         // 3 packets burst
    app0->SetAttribute("PayloadSize", UintegerValue(1400));    // Large frame payload
    app0->SetAttribute("Period", TimeValue(MicroSeconds(10))); // Microsecond arrival separation
    app0->SetAttribute("PCP", UintegerValue(1));
    app0->SetAttribute("VlanID", UintegerValue(100));
    nSource->AddApplication(app0);
    app0->SetStartTime(MilliSeconds(0));
    app0->SetStopTime(MilliSeconds(0) + MicroSeconds(25));

    // Execution environment lifecycle
    Simulator::Stop(MilliSeconds(20));
    Simulator::Run();
    Simulator::Destroy();

    // Verification Layer
    NS_TEST_ASSERT_MSG_EQ(m_rxTimes.size(), 3, "Bridge Transit Failure: Not all frames emerged from the switch node.");

    // Validate precision of Inter-packet Spacing (LengthRecovery = 1422 bytes @ 10Mbps = ~1137.6 us)
    double deltaP0_P1 = (m_rxTimes[1] - m_rxTimes[0]).GetMicroSeconds();
    double deltaP1_P2 = (m_rxTimes[2] - m_rxTimes[1]).GetMicroSeconds();
    double expectedSpacingUs = 1137.6;
    double toleranceUs = 1.0;

    NS_TEST_ASSERT_MSG_EQ_TOL(deltaP0_P1, expectedSpacingUs, toleranceUs,
                              "Transit ATS precision fault on P0->P1 gap. Measured: " << deltaP0_P1);
    NS_TEST_ASSERT_MSG_EQ_TOL(deltaP1_P2, expectedSpacingUs, toleranceUs,
                              "Transit ATS precision fault on P1->P2 gap. Measured: " << deltaP1_P2);
}

/**
 * \ingroup Ats-tests
 * \brief Test case checking ATS dynamic instance multiplexing inside a single group on a SwitchNetDevice.
 * \details Validates that two distinct applications sharing the same PCP (same group) but using
 * different VLAN IDs (100 and 110) create two isolated shaper instances without mutual interference.
 */
class AtsBridgeMultiplexingTestCase : public TestCase
{
public:
    AtsBridgeMultiplexingTestCase();
    virtual ~AtsBridgeMultiplexingTestCase() {}

private:
    void DoRun() override;
    void RecordRxTime(Ptr<const Packet> p);

    std::vector<Time> m_rxTimes;
    std::vector<uint32_t> m_rxVlans;
};

AtsBridgeMultiplexingTestCase::AtsBridgeMultiplexingTestCase()
    : TestCase("Verify that an ATS shaper inside a Switch separates streams into unique instances within the same group") {}

void AtsBridgeMultiplexingTestCase::RecordRxTime(Ptr<const Packet> p)
{
    m_rxTimes.push_back(Simulator::Now());

    Ptr<Packet> originalPacket = p->Copy();
    EthernetHeader2 ethHeader;
    originalPacket->RemoveHeader(ethHeader);
    m_rxVlans.push_back(ethHeader.GetVid());
}

void AtsBridgeMultiplexingTestCase::DoRun()
{
    m_rxTimes.clear();
    m_rxVlans.clear();

    Ptr<TsnNode> nSource = CreateObject<TsnNode>();
    Ptr<TsnNode> nDest = CreateObject<TsnNode>();
    Ptr<TsnNode> nSw1 = CreateObject<TsnNode>();

    Ptr<Clock> clockSource = CreateObject<Clock>();
    Ptr<Clock> clockSw1 = CreateObject<Clock>();
    Ptr<Clock> clockDest = CreateObject<Clock>();
    nSource->SetMainClock(clockSource);
    nSw1->SetMainClock(clockSw1);
    nDest->SetMainClock(clockDest);

    Ptr<TsnNetDevice> netSource = CreateObject<TsnNetDevice>();
    nSource->AddDevice(netSource);
    Ptr<TsnNetDevice> netDest = CreateObject<TsnNetDevice>();
    nDest->AddDevice(netDest);

    Ptr<TsnNetDevice> netSw1_1 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_1);
    Ptr<TsnNetDevice> netSw1_2 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_2);

    netSource->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_2->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netDest->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    Ptr<EthernetChannel> chA = CreateObject<EthernetChannel>();
    netSource->Attach(chA);
    netSw1_1->Attach(chA);

    Ptr<EthernetChannel> chB = CreateObject<EthernetChannel>();
    netSw1_2->Attach(chB);
    netDest->Attach(chB);

    Ptr<SwitchNetDevice> sw1 = CreateObject<SwitchNetDevice>();
    sw1->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(10)));
    sw1->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(10)));
    nSw1->AddDevice(sw1);
    sw1->AddSwitchPort(netSw1_1);
    sw1->AddSwitchPort(netSw1_2);

    Mac48Address macDest = Mac48Address::Allocate();
    netSource->SetAddress(Mac48Address::Allocate());
    netDest->SetAddress(macDest);
    sw1->SetAddress(Mac48Address::Allocate());

    for (int i = 0; i < 8; i++)
    {
        netSource->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netDest->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_2->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    sw1->AddForwardingTableEntry(macDest, 100, {netSw1_2});
    sw1->AddForwardingTableEntry(macDest, 110, {netSw1_2});

    // Stream identification
    Ptr<NullStreamIdentificationFunction> sif1 = CreateObject<NullStreamIdentificationFunction>();
    uint16_t streamHandle1 = 10;
    sif1->SetAttribute("VlanID", UintegerValue(100));
    sif1->SetAttribute("Address", AddressValue(macDest));
    nSw1->AddStreamIdentificationFunction(streamHandle1, sif1, {netSw1_1}, {}, {}, {});

    Ptr<NullStreamIdentificationFunction> sif2 = CreateObject<NullStreamIdentificationFunction>();
    uint16_t streamHandle2 = 20;
    sif2->SetAttribute("VlanID", UintegerValue(110));
    sif2->SetAttribute("Address", AddressValue(macDest));
    nSw1->AddStreamIdentificationFunction(streamHandle2, sif2, {netSw1_1}, {}, {}, {});

    netSw1_2->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> atsEngine = netSw1_2->GetAts();
    atsEngine->SetClock(clockSw1);
    atsEngine->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(20)));

    netDest->TraceConnectWithoutContext("MacRx", MakeCallback(&AtsBridgeMultiplexingTestCase::RecordRxTime, this));

    Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
    app1->Setup(netSource);
    app1->SetAttribute("Address", AddressValue(macDest));
    app1->SetAttribute("BurstSize", UintegerValue(2));
    app1->SetAttribute("PayloadSize", UintegerValue(1400)); // Total frame = 1422B
    app1->SetAttribute("Period", TimeValue(MicroSeconds(5)));
    app1->SetAttribute("PCP", UintegerValue(1));
    app1->SetAttribute("VlanID", UintegerValue(100));
    nSource->AddApplication(app1);
    app1->SetStartTime(MilliSeconds(0));
    app1->SetStopTime(MicroSeconds(2));

    Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
    app2->Setup(netSource);
    app2->SetAttribute("Address", AddressValue(macDest));
    app2->SetAttribute("BurstSize", UintegerValue(2));
    app2->SetAttribute("PayloadSize", UintegerValue(1400)); // Total frame = 1422B
    app2->SetAttribute("Period", TimeValue(MicroSeconds(5)));
    app2->SetAttribute("PCP", UintegerValue(1));
    app2->SetAttribute("VlanID", UintegerValue(110));
    nSource->AddApplication(app2);
    app2->SetStartTime(MilliSeconds(0));
    app2->SetStopTime(MicroSeconds(2));

    Simulator::Stop(MilliSeconds(50));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(m_rxTimes.size(), 4, "Bridge Multiplexing Failure: Packet loss detected.");

    double interAppDelay = (m_rxTimes[2] - m_rxTimes[1]).GetMicroSeconds();
    NS_TEST_ASSERT_MSG_LT(interAppDelay, 50.0, "Isolation Fault: App 2 was delayed by App 1's recovery time!");
}

/**
 * \ingroup Ats-tests
 * \brief Test case checking ATS scheduler group isolation based on traffic class priority (PCP).
 * \details Validates that two distinct applications sharing the same stream attributes {VID, MAC}
 * but operating on different PCPs (PCP 1 and PCP 2) instantiate separate AtsSchedulerGroups.
 */
class AtsBridgePcpPriorityTestCase : public TestCase
{
public:
    AtsBridgePcpPriorityTestCase();
    virtual ~AtsBridgePcpPriorityTestCase() {}

private:
    void DoRun() override;
    void RecordRxTime(Ptr<const Packet> p);

    std::vector<Time> m_rxTimes;
    std::vector<uint8_t> m_rxPcps;
};

AtsBridgePcpPriorityTestCase::AtsBridgePcpPriorityTestCase()
    : TestCase("Verify that an ATS shaper inside a Switch separates traffic categories into completely independent priority groups") {}

void AtsBridgePcpPriorityTestCase::RecordRxTime(Ptr<const Packet> p)
{
    m_rxTimes.push_back(Simulator::Now());

    Ptr<Packet> originalPacket = p->Copy();
    EthernetHeader2 ethHeader;
    originalPacket->RemoveHeader(ethHeader);
    m_rxPcps.push_back(ethHeader.GetPcp());
}

void AtsBridgePcpPriorityTestCase::DoRun()
{
    m_rxTimes.clear();
    m_rxPcps.clear();

    Ptr<TsnNode> nSource = CreateObject<TsnNode>();
    Ptr<TsnNode> nDest = CreateObject<TsnNode>();
    Ptr<TsnNode> nSw1 = CreateObject<TsnNode>();

    Ptr<Clock> clockSource = CreateObject<Clock>();
    Ptr<Clock> clockSw1 = CreateObject<Clock>();
    Ptr<Clock> clockDest = CreateObject<Clock>();
    nSource->SetMainClock(clockSource);
    nSw1->SetMainClock(clockSw1);
    nDest->SetMainClock(clockDest);

    Ptr<TsnNetDevice> netSource = CreateObject<TsnNetDevice>();
    nSource->AddDevice(netSource);
    Ptr<TsnNetDevice> netDest = CreateObject<TsnNetDevice>();
    nDest->AddDevice(netDest);

    Ptr<TsnNetDevice> netSw1_1 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_1);
    Ptr<TsnNetDevice> netSw1_2 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_2);

    netSource->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_2->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netDest->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    Ptr<EthernetChannel> chA = CreateObject<EthernetChannel>();
    netSource->Attach(chA);
    netSw1_1->Attach(chA);

    Ptr<EthernetChannel> chB = CreateObject<EthernetChannel>();
    netSw1_2->Attach(chB);
    netDest->Attach(chB);

    Ptr<SwitchNetDevice> sw1 = CreateObject<SwitchNetDevice>();
    sw1->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(10)));
    sw1->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(10)));
    nSw1->AddDevice(sw1);
    sw1->AddSwitchPort(netSw1_1);
    sw1->AddSwitchPort(netSw1_2);

    Mac48Address macDest = Mac48Address::Allocate();
    netSource->SetAddress(Mac48Address::Allocate());
    netDest->SetAddress(macDest);
    sw1->SetAddress(Mac48Address::Allocate());

    for (int i = 0; i < 8; i++)
    {
        netSource->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netDest->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_2->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    sw1->AddForwardingTableEntry(macDest, 100, {netSw1_2});

    // Stream identification
    Ptr<NullStreamIdentificationFunction> sif1 = CreateObject<NullStreamIdentificationFunction>();
    uint16_t streamHandle1 = 10;
    sif1->SetAttribute("VlanID", UintegerValue(100));
    sif1->SetAttribute("Address", AddressValue(macDest));
    nSw1->AddStreamIdentificationFunction(streamHandle1, sif1, {netSw1_1}, {}, {}, {});

    netSw1_2->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> atsEngine = netSw1_2->GetAts();
    atsEngine->SetClock(clockSw1);
    atsEngine->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(20)));

    netDest->TraceConnectWithoutContext("MacRx", MakeCallback(&AtsBridgePcpPriorityTestCase::RecordRxTime, this));

    Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
    app1->Setup(netSource);
    app1->SetAttribute("Address", AddressValue(macDest));
    app1->SetAttribute("BurstSize", UintegerValue(2));
    app1->SetAttribute("PayloadSize", UintegerValue(1400));
    app1->SetAttribute("Period", TimeValue(MicroSeconds(5)));
    app1->SetAttribute("PCP", UintegerValue(2));
    app1->SetAttribute("VlanID", UintegerValue(100));
    nSource->AddApplication(app1);
    app1->SetStartTime(MilliSeconds(0));
    app1->SetStopTime(MicroSeconds(2));

    Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
    app2->Setup(netSource);
    app2->SetAttribute("Address", AddressValue(macDest));
    app2->SetAttribute("BurstSize", UintegerValue(2));
    app2->SetAttribute("PayloadSize", UintegerValue(1400));
    app2->SetAttribute("Period", TimeValue(MicroSeconds(5)));
    app2->SetAttribute("PCP", UintegerValue(1));
    app2->SetAttribute("VlanID", UintegerValue(100));
    nSource->AddApplication(app2);
    app2->SetStartTime(MilliSeconds(0));
    app2->SetStopTime(MicroSeconds(2));

    Simulator::Stop(MilliSeconds(50));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(m_rxTimes.size(), 4, "Bridge PCP Isolation Failure: Missing frames.");

    double interGroupDelay = (m_rxTimes[1] - m_rxTimes[0]).GetMicroSeconds();
    NS_TEST_ASSERT_MSG_LT(interGroupDelay, 50.0, "PCP Segregation Fault: High priority queue blocks on low priority scheduler timeline.");
}

/**
 * \ingroup Ats-tests
 * \brief Test case checking ATS scheduler group isolation based on ingress ports.
 * \details Validates that two distinct source nodes transmitting concurrent bursts
 * with identical VlanIDs and PCPs are assigned to separate AtsSchedulerGroups
 * upon entering the switch from different ingress ports.
 */
class AtsBridgeIngressIsolationTestCase : public TestCase
{
public:
    AtsBridgeIngressIsolationTestCase();
    virtual ~AtsBridgeIngressIsolationTestCase() {}

private:
    void DoRun() override;
    void RecordRxTime(Ptr<const Packet> p);

    std::vector<Time> m_rxTimes;
    std::vector<Mac48Address> m_rxSrcMacs;
};

AtsBridgeIngressIsolationTestCase::AtsBridgeIngressIsolationTestCase()
    : TestCase("Verify that an ATS shaper inside a Switch isolates streams based on their physical ingress port") {}

void AtsBridgeIngressIsolationTestCase::RecordRxTime(Ptr<const Packet> p)
{
    m_rxTimes.push_back(Simulator::Now());

    Ptr<Packet> originalPacket = p->Copy();
    EthernetHeader2 ethHeader;
    originalPacket->RemoveHeader(ethHeader);
    m_rxSrcMacs.push_back(ethHeader.GetSrc());
}

void AtsBridgeIngressIsolationTestCase::DoRun()
{
    m_rxTimes.clear();
    m_rxSrcMacs.clear();

    Ptr<TsnNode> nSource1 = CreateObject<TsnNode>();
    Ptr<TsnNode> nSource2 = CreateObject<TsnNode>();
    Ptr<TsnNode> nDest = CreateObject<TsnNode>();
    Ptr<TsnNode> nSw1 = CreateObject<TsnNode>();

    Ptr<Clock> clockSrc1 = CreateObject<Clock>();
    Ptr<Clock> clockSrc2 = CreateObject<Clock>();
    Ptr<Clock> clockSw1 = CreateObject<Clock>();
    Ptr<Clock> clockDest = CreateObject<Clock>();
    nSource1->SetMainClock(clockSrc1);
    nSource2->SetMainClock(clockSrc2);
    nSw1->SetMainClock(clockSw1);
    nDest->SetMainClock(clockDest);

    Ptr<TsnNetDevice> netSource1 = CreateObject<TsnNetDevice>();
    nSource1->AddDevice(netSource1);
    Ptr<TsnNetDevice> netSource2 = CreateObject<TsnNetDevice>();
    nSource2->AddDevice(netSource2);
    Ptr<TsnNetDevice> netDest = CreateObject<TsnNetDevice>();
    nDest->AddDevice(netDest);

    Ptr<TsnNetDevice> netSw1_1 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_1);
    Ptr<TsnNetDevice> netSw1_2 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_2);
    Ptr<TsnNetDevice> netSw1_3 = CreateObject<TsnNetDevice>();
    nSw1->AddDevice(netSw1_3);

    netSource1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSource2->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_2->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netSw1_3->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    netDest->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    Ptr<EthernetChannel> chA = CreateObject<EthernetChannel>();
    netSource1->Attach(chA);
    netSw1_1->Attach(chA);

    Ptr<EthernetChannel> chB = CreateObject<EthernetChannel>();
    netSource2->Attach(chB);
    netSw1_2->Attach(chB);

    Ptr<EthernetChannel> chC = CreateObject<EthernetChannel>();
    netSw1_3->Attach(chC);
    netDest->Attach(chC);

    Ptr<SwitchNetDevice> sw1 = CreateObject<SwitchNetDevice>();
    sw1->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(10)));
    sw1->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(10)));
    nSw1->AddDevice(sw1);
    sw1->AddSwitchPort(netSw1_1);
    sw1->AddSwitchPort(netSw1_2);
    sw1->AddSwitchPort(netSw1_3);

    Mac48Address macSrc1 = Mac48Address("00:00:00:00:00:01");
    Mac48Address macSrc2 = Mac48Address("00:00:00:00:00:02");
    Mac48Address macDest = Mac48Address("00:00:00:00:00:03");
    netSource1->SetAddress(macSrc1);
    netSource2->SetAddress(macSrc2);
    netDest->SetAddress(macDest);
    sw1->SetAddress(Mac48Address::Allocate());

    for (int i = 0; i < 8; i++)
    {
        netSource1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSource2->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netDest->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_2->SetQueue(CreateObject<DropTailQueue<Packet>>());
        netSw1_3->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    sw1->AddForwardingTableEntry(macDest, 100, {netSw1_3});

    // Stream identification
    Ptr<NullStreamIdentificationFunction> sif1 = CreateObject<NullStreamIdentificationFunction>();
    uint16_t streamHandle1 = 10;
    sif1->SetAttribute("VlanID", UintegerValue(100));
    sif1->SetAttribute("Address", AddressValue(macDest));
    nSw1->AddStreamIdentificationFunction(streamHandle1, sif1, {netSw1_1}, {}, {}, {});

    Ptr<NullStreamIdentificationFunction> sif2 = CreateObject<NullStreamIdentificationFunction>();
    uint16_t streamHandle2 = 20;
    sif2->SetAttribute("VlanID", UintegerValue(100));
    sif2->SetAttribute("Address", AddressValue(macDest));
    nSw1->AddStreamIdentificationFunction(streamHandle2, sif2, {netSw1_2}, {}, {}, {});

    netSw1_3->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> atsEngine = netSw1_3->GetAts();
    atsEngine->SetClock(clockSw1);
    atsEngine->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(20)));

    netDest->TraceConnectWithoutContext("MacRx", MakeCallback(&AtsBridgeIngressIsolationTestCase::RecordRxTime, this));

    // App 1 on Port 1 (PCP 3, VLAN 100)
    Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
    app1->Setup(netSource1);
    app1->SetAttribute("Address", AddressValue(macDest));
    app1->SetAttribute("BurstSize", UintegerValue(2));
    app1->SetAttribute("PayloadSize", UintegerValue(1400));
    app1->SetAttribute("Period", TimeValue(MicroSeconds(5)));
    app1->SetAttribute("PCP", UintegerValue(3));
    app1->SetAttribute("VlanID", UintegerValue(100));
    nSource1->AddApplication(app1);
    app1->SetStartTime(MilliSeconds(0));
    app1->SetStopTime(MicroSeconds(2));

    // App 2 on Port 2 (PCP 3, VLAN 100)
    Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
    app2->Setup(netSource2);
    app2->SetAttribute("Address", AddressValue(macDest));
    app2->SetAttribute("BurstSize", UintegerValue(2));
    app2->SetAttribute("PayloadSize", UintegerValue(1400));
    app2->SetAttribute("Period", TimeValue(MicroSeconds(5)));
    app2->SetAttribute("PCP", UintegerValue(3));
    app2->SetAttribute("VlanID", UintegerValue(100));
    nSource2->AddApplication(app2);
    app2->SetStartTime(MilliSeconds(0));
    app2->SetStopTime(MicroSeconds(2));

    Simulator::Stop(MilliSeconds(50));
    Simulator::Run();
    Simulator::Destroy();

    // Verification layer
    NS_TEST_ASSERT_MSG_EQ(m_rxTimes.size(), 4, "Ingress Isolation Failure: Missing packets.");

    // If port-based isolation functions correctly, the first frames from both ports
    // must arrive back-to-back at destination with minimal physical transmission gap (~11 us)
    double ingressPortDelay = (m_rxTimes[1] - m_rxTimes[0]).GetMicroSeconds();
    NS_TEST_ASSERT_MSG_LT(ingressPortDelay, 50.0, "Ingress Port Isolation Fault: Traffic from Port 2 was serialised/blocked by Port 1 timeline.");
}

/**
 * \ingroup ats-tests
 * \brief Main TestSuite registration class for the ATS module
 */
class AtsTestSuite : public TestSuite
{
public:
    AtsTestSuite();
};

AtsTestSuite::AtsTestSuite()
    : TestSuite("ats", UNIT)
{
    LogComponentEnable("AtsTestSuite", LOG_LEVEL_ALL);
    LogComponentEnable("TsnNetDevice", LOG_LEVEL_ALL);

    // -------------------------------------------------------------------------
    // Test Case 1 : Baseline Nominal Configuration (No Drops Expected)
    // -------------------------------------------------------------------------
    uint64_t pktSizeBasic = 500;
    uint64_t ethFrameSizeBasic = pktSizeBasic + 22;
    AddTestCase(new AtsBasicTestCase(pktSizeBasic, ethFrameSizeBasic, 5, DataRate("100Mbps"), 12288), TestCase::QUICK);

    // -------------------------------------------------------------------------
    // Test Case 2 : Packet Drop via MaxResidenceTime Violation
    // -------------------------------------------------------------------------
    // - ATS Defaults: CIR = 10 Mbps, MaxResidenceTime = 1 ms (1000 µs)
    // - Frame Size: 522 Bytes = 4176 bits -> Processing cost = 417.6 µs per packet
    // - A burst of 5 packets arrives at t = 0 µs (0 s):
    //   * Pkt 0: Eligibility = 417.6 µs -> Delay = 417.6 µs (<= 1ms)  -> ALLOW
    //   * Pkt 1: Eligibility = 835.2 µs -> Delay = 835.2 µs (<= 1ms)  -> ALLOW
    //   * Pkt 2: Eligibility = 1252.8 µs -> Delay = 1252.8 µs (> 1ms) -> DROP
    //   * Pkt 3: Eligibility = 1252.8 µs -> Delay = 1252.8 µs (> 1ms) -> DROP
    //   * Pkt 4: Eligibility = 1252.8 µs -> Delay = 1252.8 µs (> 1ms) -> DROP
    // - Execution results in exactly 2 packets passing the shaper (Pkt 0 and Pkt 1).
    // -------------------------------------------------------------------------
    uint64_t payloadSizeDrop = 500;
    uint32_t burstSize = 5;
    uint32_t expectedPackets = 2;
    uint8_t targetPcp = 5;
    Time maxResidence = MilliSeconds(1);
    Time intervalPeriod = MicroSeconds(10);

    AddTestCase(new AtsPolicingDropTestCase(payloadSizeDrop, burstSize, expectedPackets, targetPcp, maxResidence, intervalPeriod), TestCase::QUICK);

    // -------------------------------------------------------------------------
    // Test Case 3 : End-Station Isolation (Multi-Application Shapers)
    // -------------------------------------------------------------------------
    AddTestCase(new AtsMultiAppIsolationTestCase(), TestCase::QUICK);

    // -------------------------------------------------------------------------
    // Test Case 4 : Shaping Latency Precision Check
    // -------------------------------------------------------------------------
    AddTestCase(new AtsShapingLatencyTestCase(), TestCase::QUICK);

    // -------------------------------------------------------------------------
    // Test Case 5 : Stream Segregation / Noisy Neighbor Protection (t=0ms)
    // -------------------------------------------------------------------------
    AddTestCase(new AtsNoisyNeighborIsolationTestCase(), TestCase::QUICK);

    // -------------------------------------------------------------------------
    // Test Case 6 : SwitchNetDevice Transit & L2 Forwarding Reshaping Check
    // -------------------------------------------------------------------------
    AddTestCase(new AtsBridgeTransitTestCase(), TestCase::QUICK);

    // -------------------------------------------------------------------------
    // Test Case 7 : SwitchNetDevice Multiplexing & Instance Separation (VLAN 100 & 110)
    // -------------------------------------------------------------------------
    AddTestCase(new AtsBridgeMultiplexingTestCase(), TestCase::QUICK);

    // -------------------------------------------------------------------------
    // Test Case 8 : SwitchNetDevice PCP Priority Group Segregation (PCP 1 vs PCP 2)
    // -------------------------------------------------------------------------
    AddTestCase(new AtsBridgePcpPriorityTestCase(), TestCase::QUICK);

    // -------------------------------------------------------------------------
    // Test Case 9 : SwitchNetDevice Ingress Port Isolation Check (Port 1 vs Port 2)
    // -------------------------------------------------------------------------
    AddTestCase(new AtsBridgeIngressIsolationTestCase(), TestCase::QUICK);
}

static AtsTestSuite m_atsTestSuite;