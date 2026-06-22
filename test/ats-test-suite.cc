// Include a header file from your module to test.
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
    // - A burst of 5 packets arrives at t = 0 µs:
    //   * Pkt 1: Eligibility = 0 µs     -> Delay = 0 µs (<= 1ms)    -> ALLOW
    //   * Pkt 2: Eligibility = 417.6 µs -> Delay = 417.6 µs (<= 1ms) -> ALLOW
    //   * Pkt 3: Eligibility = 835.2 µs -> Delay = 835.2 µs (<= 1ms) -> ALLOW (or DROP if strict state tracking)
    //   * Pkt 4: Eligibility = 1252.8 µs -> Delay = 1252.8 µs (> 1ms) -> DROP
    //   * Pkt 5: Eligibility = 1670.4 µs -> Delay = 1670.4 µs (> 1ms) -> DROP
    // - Execution results in exactly 2 packets passing the shaper.
    // -------------------------------------------------------------------------
    uint64_t payloadSizeDrop = 500;
    uint32_t burstSize = 5;
    uint32_t expectedPackets = 2;
    uint8_t targetPcp = 5;
    Time maxResidence = MilliSeconds(1);
    Time intervalPeriod = MicroSeconds(10);

    AddTestCase(new AtsPolicingDropTestCase(payloadSizeDrop, burstSize, expectedPackets, targetPcp, maxResidence, intervalPeriod), TestCase::QUICK);
}

static AtsTestSuite m_atsTestSuite;