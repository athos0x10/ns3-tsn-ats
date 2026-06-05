#include "ats-scheduler-instance.h"

#include "ns3/log.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("AtsSchedulerInstance");

    NS_OBJECT_ENSURE_REGISTERED(AtsSchedulerInstance);

    AtsSchedulerInstance::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::AtsSchedulerInstance")
                .SetParent<Object>()
                .SetGroupName("Tsn")
                .AddConstructor<AtsSchedulerInstance>()
                .AddAttribute("SchedulerIdentifier",
                              "The unique identifier for this ATS scheduler instance.",
                              UintegerValue(0),
                              MakeUintegerAccessor(&AtsSchedulerInstance::m_schedulerIdentifier),
                              MakeUintegerChecker<uint32_t>())
                .AddAttribute("SchedulerGroupIdentifier",
                              "The group identifier to which this scheduler instance belongs.",
                              UintegerValue(0),
                              MakeUintegerAccessor(&AtsSchedulerInstance::m_schedulerGroupIdentifier),
                              MakeUintegerChecker<uint8_t>())
                .AddAttribute("CommittedInformationRate",
                              "The committed information rate (CIR) in bits per second.",
                              DataRateValue(DataRate("100Mbps")),
                              MakeDataRateAccessor(&AtsSchedulerInstance::m_committedInformationRate),
                              MakeDataRateChecker())
                .AddAttribute("CommittedBurstSize",
                              "The committed burst size (CBS) in bits.",
                              UintegerValue(12288), // Équivalent par défaut à ~1536 octets
                              MakeUintegerAccessor(&AtsSchedulerInstance::m_committedBurstSize),
                              MakeUintegerChecker<uint32_t>());
        return tid;
    }

    AtsSchedulerInstance::AtsSchedulerInstance()
    {
        NS_LOG_FUNCTION(this);
    }

    AtsSchedulerInstance::~AtsSchedulerInstance()
    {
        NS_LOG_FUNCTION(this);
    }

    void
    AtsSchedulerInstance::SetClock(Ptr<Clock> clock)
    {
        NS_LOG_FUNCTION(this << clock);
        m_clock = clock;
    }

    Time AtsSchedulerInstance::CalculateSchedulerEligibility(uint16_t size)
    {
        NS_LOG_FUNCTION(this << size);
        NS_ASSERT_MSG(m_clock != nullptr, "AtsSchedulerInstance: Clock pointer is null, call SetClock first.");

        // Retrieve local current time of the device
        Time currentTime = m_clock->GetLocalTime();

        double lengthRecoveryDuration = static_cast<double>(size) / m_committedInformationRate.GetBitRate();
    }

} // namespace ns3
