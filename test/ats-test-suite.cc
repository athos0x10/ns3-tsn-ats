// Include a header file from your module to test.
#include "ns3/tsn-net-device.h"
#include "ns3/ats-scheduler-group.h"
#include "ns3/ats-scheduler-instance.h"
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
 * \brief Check if packets cross a TSN channel successfully using Per-Priority ATS routing,
 * ensuring that both default and specific shaper configurations do not inadvertently drop valid traffic.
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

    n0->AddClock(CreateObject<Clock>());
    n1->AddClock(CreateObject<Clock>());

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

    net0->SetAttribute("IsAtsEnabled", BooleanValue(true));
    Ptr<AtsSchedulerGroup> atsGroup = net0->GetAtsSchedulerGroup();
    atsGroup->SetAttribute("MaxResidenceTime", TimeValue(Seconds(1.0)));
    atsGroup->SetPerPriorityRouting(true);
    atsGroup->CreateAtsInstanceForPriority(m_cir, m_cbs, m_pcp);

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
 * \brief Check that a dense burst (10ms to 14ms) processed under a 150ms MaxResidenceTime
 * results in exactly the 5th packet being dropped.
 * Manual calculation tracking:
 * - Pkt 1 (t=10ms): Eligibility=10ms, delay=0ms <= 150ms -> PASS
 * - Pkt 2 (t=11ms): Eligibility=50ms, delay=39ms <= 150ms -> PASS
 * - Pkt 3 (t=12ms): Eligibility=90ms, delay=78ms <= 150ms -> PASS
 * - Pkt 4 (t=13ms): Eligibility=130ms, delay=117ms <= 150ms -> PASS
 * - Pkt 5 (t=14ms): Eligibility=170ms, delay=156ms > 150ms -> DROP
 */
class AtsPolicingDropTestCase : public TestCase
{
public:
    AtsPolicingDropTestCase(uint64_t pktSize, uint32_t totalBurst, uint32_t expectedPackets, uint8_t pcp, DataRate cir, uint32_t cbs, Time maxResidenceTime, Time period);
    virtual ~AtsPolicingDropTestCase();

private:
    void DoRun() override;
    void ReceiveRx(Ptr<const Packet> p);

    uint32_t m_receivedCount{0}; //!< Number of packets received
    uint64_t m_pktSize;          //!< Payload size configured in the generator
    uint32_t m_totalBurst;       //!< Total number of generated frames in the burst
    uint32_t m_expectedPackets;  //!< Total number of compliant packets expected at destination
    uint8_t m_pcp;               //!< Priority Code Point (PCP) values
    DataRate m_cir;              //!< Committed Information Rate
    uint32_t m_cbs;              //!< Committed Burst Size
    Time m_maxResidenceTime;     //!< Maximum allowable latency inside the scheduler bucket
    Time m_period;               //!< Interval between packet generations
};

AtsPolicingDropTestCase::AtsPolicingDropTestCase(uint64_t pktSize, uint32_t totalBurst, uint32_t expectedPackets, uint8_t pcp, DataRate cir, uint32_t cbs, Time maxResidenceTime, Time period)
    : TestCase("Verify ATS policing rule where the 5th dense burst packet is dropped due to 150ms MaxResidenceTime timeout")
{
    m_pktSize = pktSize;
    m_totalBurst = totalBurst;
    m_expectedPackets = expectedPackets;
    m_pcp = pcp;
    m_cir = cir;
    m_cbs = cbs;
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

    n0->AddClock(CreateObject<Clock>());
    n1->AddClock(CreateObject<Clock>());

    Ptr<TsnNetDevice> net0 = CreateObject<TsnNetDevice>();
    n0->AddDevice(net0);
    Ptr<TsnNetDevice> net1 = CreateObject<TsnNetDevice>();
    n1->AddDevice(net1);

    // High wire data rates to completely isolate ATS shaping mechanics
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

    // Configure the ATS scheduler group
    net0->SetAttribute("IsAtsEnabled", BooleanValue(true));
    Ptr<AtsSchedulerGroup> atsGroup = net0->GetAtsSchedulerGroup();
    atsGroup->SetAttribute("MaxResidenceTime", TimeValue(m_maxResidenceTime)); // 150 ms
    atsGroup->SetPerPriorityRouting(true);
    atsGroup->CreateAtsInstanceForPriority(m_cir, m_cbs, m_pcp); // CIR=100Kbps, CBS=16000B

    // Configure traffic generator for dense burst (t = 10ms to 14ms)
    Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
    app0->Setup(net0);
    app0->SetAttribute("BurstSize", UintegerValue(m_totalBurst)); // 5 packets
    app0->SetAttribute("PayloadSize", UintegerValue(m_pktSize));  // 478 payload size -> 500B total L2
    app0->SetAttribute("Period", TimeValue(m_period));            // 1ms inter-packet gap
    app0->SetAttribute("VlanID", UintegerValue(1));
    app0->SetAttribute("PCP", UintegerValue(m_pcp));
    n0->AddApplication(app0);

    app0->SetStartTime(MilliSeconds(10));
    app0->SetStopTime(MilliSeconds(20));

    net1->TraceConnectWithoutContext("MacRx", MakeCallback(&AtsPolicingDropTestCase::ReceiveRx, this));

    // Increase stop window to let all 4 compliant packets finish shaping completely (last one at 130ms)
    Simulator::Stop(MilliSeconds(1000));
    Simulator::Run();
    Simulator::Destroy();

    // Final assessment evaluation
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
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_ALL);

    // -------------------------------------------------------------------------
    // Test Case 1: Original Basic Configuration (No Drops Expected)
    // -------------------------------------------------------------------------
    uint64_t pktSizeBasic = 500;
    uint64_t ethFrameSizeBasic = pktSizeBasic + 22;
    AddTestCase(new AtsBasicTestCase(pktSizeBasic, ethFrameSizeBasic, 5, DataRate("100Mbps"), 12288), TestCase::QUICK);

    // -------------------------------------------------------------------------
    // Test Case 2: Validation based on Manual Policing Drop Calculations (4 Recv / 5 Sent)
    // -------------------------------------------------------------------------
    uint64_t payloadSizeManual = 478;
    uint32_t burstSize = 5;
    uint32_t expectedPackets = 4; // Only the 5th packet should drop
    uint8_t targetPcp = 5;
    DataRate cirManual = DataRate("100Kbps");
    uint32_t cbsManual = 16000; // 2000 bytes
    Time maxResidenceManual = MilliSeconds(150);
    Time intervalPeriod = MilliSeconds(1);

    // AddTestCase(new AtsPolicingDropTestCase(payloadSizeManual, burstSize, expectedPackets, targetPcp, cirManual, cbsManual, maxResidenceManual, intervalPeriod), TestCase::QUICK);
}

static AtsTestSuite m_atsTestSuite;