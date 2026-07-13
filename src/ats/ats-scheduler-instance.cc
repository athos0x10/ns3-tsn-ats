#include "ats-scheduler-instance.h"

#include "ns3/log.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("AtsSchedulerInstance");

    NS_OBJECT_ENSURE_REGISTERED(AtsSchedulerInstance);

    TypeId
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
                              MakeUintegerChecker<uint32_t>())
                .AddAttribute("CommittedInformationRate",
                              "The committed information rate (CIR) in bits per second.",
                              DataRateValue(DataRate("100Mbps")),
                              MakeDataRateAccessor(&AtsSchedulerInstance::m_committedInformationRate),
                              MakeDataRateChecker())
                .AddAttribute("CommittedBurstSize",
                              "The committed burst size (CBS) in bits.",
                              UintegerValue(12288), // Default to 12KB
                              MakeUintegerAccessor(&AtsSchedulerInstance::m_committedBurstSize),
                              MakeUintegerChecker<uint32_t>())
                .AddTraceSource("Tokens",
                                "Current ATS bucket token level in bits",
                                MakeTraceSourceAccessor(&AtsSchedulerInstance::m_tokensTrace),
                                "ns3::TracedValueCallback::double");
        ;
        return tid;
    }

    AtsSchedulerInstance::AtsSchedulerInstance()
    {
        NS_LOG_FUNCTION(this);
        m_bucketEmptyTime = Seconds(0);
    }

    AtsSchedulerInstance::~AtsSchedulerInstance()
    {
        NS_LOG_FUNCTION(this);
    }

    void
    AtsSchedulerInstance::TraceTokens(Time currentTime)
    {
        // The bucket is full at m_committedBurstSize (CBS).
        // Consumed tokens correspond to the remaining time needed for the bucket to empty.
        double cbs = static_cast<double>(m_committedBurstSize);
        double cir = m_committedInformationRate.GetBitRate();

        if (currentTime >= m_bucketEmptyTime)
        {
            // The bucket is completely empty (meaning it is full of tokens/capacity available)
            m_tokensTrace(cbs);
        }
        else
        {
            // The remaining time before m_bucketEmptyTime represents the missing consumed bits
            double missingBits = (m_bucketEmptyTime - currentTime).GetSeconds() * cir;
            double currentTokens = cbs - missingBits;

            // Safety check against negative values due to rounding errors
            if (currentTokens < 0)
                currentTokens = 0;

            m_tokensTrace(currentTokens);
        }
    }

} // namespace ns3
