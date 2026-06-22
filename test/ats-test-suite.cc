#include "ns3/tsn-net-device.h"
#include "ns3/ats.h"
#include "ns3/ethernet-channel.h"
#include "ns3/ethernet-header2.h"
#include "ns3/ethernet-generator.h"

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
    app0->SetStartTime(MilliSeconds(10));
    app0->SetStopTime(MilliSeconds(10) + MicroSeconds(2));

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

    // Application 1 -> Generates Stream ID 10
    Ptr<EthernetGenerator> app1 = CreateObject<EthernetGenerator>();
    app1->Setup(net0);
    app1->SetAttribute("StreamId", UintegerValue(10));
    app1->SetAttribute("BurstSize", UintegerValue(2));
    app1->SetAttribute("PayloadSize", UintegerValue(500));
    app1->SetAttribute("Period", TimeValue(Seconds(1)));
    n0->AddApplication(app1);
    app1->SetStartTime(MilliSeconds(10));
    app1->SetStopTime(MilliSeconds(10) + MicroSeconds(2));

    // Application 2 -> Generates Stream ID 20 at the exact same arrival time
    Ptr<EthernetGenerator> app2 = CreateObject<EthernetGenerator>();
    app2->Setup(net0);
    app2->SetAttribute("StreamId", UintegerValue(20));
    app2->SetAttribute("BurstSize", UintegerValue(2));
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
    app->SetAttribute("StreamId", UintegerValue(10));
    app->SetAttribute("BurstSize", UintegerValue(2));
    app->SetAttribute("PayloadSize", UintegerValue(500));
    app->SetAttribute("Period", TimeValue(Seconds(1)));

    n0->AddApplication(app);
    app->SetStartTime(MilliSeconds(20));
    app->SetStopTime(MilliSeconds(25));

    Simulator::Stop(MilliSeconds(100));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(m_rxTimes.size(), 2, "Shaping precision verification failed: Missing frames. Received " << m_rxTimes.size());

    Time deltaReceived = m_rxTimes[1] - m_rxTimes[0];
    double deltaUs = deltaReceived.GetMicroSeconds();

    double expectedSpacingUs = 414.4;
    double toleranceUs = 0.5;

    NS_TEST_ASSERT_MSG_EQ_TOL(deltaUs, expectedSpacingUs, toleranceUs,
                              "ATS spacing delay drifts outside allowable bounds! " << "Measured: " << deltaUs << " us, Expected: " << expectedSpacingUs << " us");
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
    // - A burst of 5 packets arrives at t = 10000 µs (0.01 s):
    //   * Pkt 0: Eligibility = 10417.6 µs -> Delay = 417.6 µs (<= 1ms)  -> ALLOW
    //   * Pkt 1: Eligibility = 10835.2 µs -> Delay = 835.2 µs (<= 1ms)  -> ALLOW
    //   * Pkt 2: Eligibility = 11252.8 µs -> Delay = 1252.8 µs (> 1ms) -> DROP
    //   * Pkt 3: Eligibility = 11252.8 µs -> Delay = 1252.8 µs (> 1ms) -> DROP
    //   * Pkt 4: Eligibility = 11252.8 µs -> Delay = 1252.8 µs (> 1ms) -> DROP
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
}

static AtsTestSuite m_atsTestSuite;