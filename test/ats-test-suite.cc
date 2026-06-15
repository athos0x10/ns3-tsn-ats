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

void AtsBasicTestCase::SendTx(Ptr<const Packet> p)
{
    m_sent += p->GetSize();
}

void AtsBasicTestCase::ReceiveRx(Ptr<const Packet> p)
{
    m_received += p->GetSize();
}

void AtsBasicTestCase::DoRun()
{
    // Create two nodes
    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Ptr<TsnNode> n1 = CreateObject<TsnNode>();

    // Add perfect clock on each node
    n0->AddClock(CreateObject<Clock>());
    n1->AddClock(CreateObject<Clock>());

    // Create and add a netDevice to each node
    Ptr<TsnNetDevice> net0 = CreateObject<TsnNetDevice>();
    n0->AddDevice(net0);
    Ptr<TsnNetDevice> net1 = CreateObject<TsnNetDevice>();
    n1->AddDevice(net1);

    // Create a TSN Channel and attach it to the two netDevices
    Ptr<EthernetChannel> channel = CreateObject<EthernetChannel>();
    net0->Attach(channel);
    net1->Attach(channel);

    // Allocate a MAC address for each netDevice
    net0->SetAddress(Mac48Address::Allocate());
    net1->SetAddress(Mac48Address::Allocate());

    // Create and add 8 FIFOs per net device
    for (int i = 0; i < 8; i++)
    {
        net0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // --- ATS CONFIGURATION ---
    // Enable ATS on the transmitting device
    net0->SetAttribute("IsAtsEnabled", BooleanValue(true));
    Ptr<AtsSchedulerGroup> atsGroup = net0->GetAtsSchedulerGroup();
    atsGroup->SetAttribute("MaxResidenceTime", TimeValue(Seconds(1.0)));

    // Explicitly enforce per-priority routing mode
    atsGroup->SetPerPriorityRouting(true);

    // Instantiate the Token Bucket shaper for the requested target priority
    atsGroup->CreateAtsInstanceForPriority(m_cir, m_cbs, m_pcp);

    // Ensure the transmission gates are open so TAS doesn't drop the traffic
    // net0->AddGclEntry(Seconds(2.0), 0xFF); // Open all 8 gates continuously
    // net0->StartTas();

    // Application configuration (Generates 1 packet every second)
    Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
    app0->Setup(net0);
    app0->SetAttribute("BurstSize", UintegerValue(1));
    app0->SetAttribute("PayloadSize", UintegerValue(m_pktSize));
    app0->SetAttribute("Period", TimeValue(Seconds(1)));
    app0->SetAttribute("VlanID", UintegerValue(1));
    app0->SetAttribute("PCP", UintegerValue(m_pcp));
    n0->AddApplication(app0);
    app0->SetStartTime(Seconds(0));
    app0->SetStopTime(Seconds(1));

    // Connect trace sources to verify transmission and reception counts
    net0->TraceConnectWithoutContext("MacTx", MakeCallback(&AtsBasicTestCase::SendTx, this));
    net1->TraceConnectWithoutContext("MacRx", MakeCallback(&AtsBasicTestCase::ReceiveRx, this));

    // Execute the simulation
    Simulator::Stop(Seconds(2));
    Simulator::Run();
    Simulator::Destroy();

    // --- ASSERTIONS ---
    // Check that traffic passed through completely and didn't fall to zero bytes
    NS_TEST_ASSERT_MSG_GT(m_sent, 0, "Packets must successfully leave the generator application");
    NS_TEST_ASSERT_MSG_EQ(m_received, m_sent, "All packets processed by the ATS shaper must arrive completely without drops");
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

    uint64_t pktSize = 500;               // Application payload size
    uint64_t ethFrameSize = pktSize + 22; // Total L2 frame size over the channel (Payload + Header + CRC)

    // -------------------------------------------------------------------------
    // Test Case 1: Basic Line-Rate Packet Delivery Validation (Per-Priority)
    // -------------------------------------------------------------------------
    // A standard packet at 100 Mbps on PCP 5 must pass cleanly without blocking
    AddTestCase(new AtsBasicTestCase(pktSize, ethFrameSize, 5, DataRate("100Mbps"), 12288), TestCase::QUICK);
}

// Static initialization forces registration into the global ns-3 test runner framework
static AtsTestSuite m_atsTestSuite;