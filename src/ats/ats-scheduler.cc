#include "ats-scheduler.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/tsn-net-device.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("AtsScheduler");
    NS_OBJECT_ENSURE_REGISTERED(AtsScheduler);

    TypeId
    AtsScheduler::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::AtsScheduler")
                .SetParent<Object>()
                .SetGroupName("Tsn")
                .AddConstructor<AtsScheduler>()
                .AddAttribute("SchedulerGroupIdentifier",
                              "The group identifier for this ATS scheduler.",
                              UintegerValue(0),
                              MakeUintegerAccessor(&AtsScheduler::m_schedulerGroupId),
                              MakeUintegerChecker<uint32_t>())
                .AddAttribute("MaxResidenceTime", "Maximum residence time inside the scheduler.",
                              TimeValue(MilliSeconds(10)),
                              MakeTimeAccessor(&AtsScheduler::m_maximumResidenceTime),
                              MakeTimeChecker());
        return tid;
    }

    AtsScheduler::AtsScheduler()
    {
        NS_LOG_FUNCTION(this);
        m_instanceIdCounter = 0; // Start instance IDs from 0
        // create default ATS Scheduler Instance with default attributes
        m_defaultInstance = CreateObject<AtsSchedulerInstance>();
        m_defaultInstance->SetAttribute("SchedulerIdentifier", 0); // Default instance has ID 0
        m_defaultInstance->SetAttribute("SchedulerGroupIdentifier", m_schedulerGroupId);
        m_defaultInstance->SetAttribute("CommittedInformationRate", DataRate("100Mbps"));
        m_defaultInstance->SetAttribute("CommittedBurstSize", 12288); // Default to 12KB
    }

    AtsScheduler::~AtsScheduler()
    {
        NS_LOG_FUNCTION(this);
    }

    void AtsScheduler::SetTsnNetDevice(Ptr<TsnNetDevice> device)
    {
        NS_LOG_FUNCTION(this << device);
        m_netDevice = device;
    }

    void AtsScheduler::SetClock(Ptr<Clock> clock)
    {
        NS_LOG_FUNCTION(this << clock);
        m_clock = clock;

        if (m_clock)
        {
            m_defaultInstance->SetBucketEmptyTime(m_clock->GetLocalTime());
        }
    }

    uint32_t AtsScheduler::CreateSchedulerInstance(DataRate cir, uint32_t cbs)
    {
        NS_LOG_FUNCTION(this << cir << cbs);
        uint32_t instanceId = m_instanceIdCounter++;
        Ptr<AtsSchedulerInstance> instance = CreateObject<AtsSchedulerInstance>();
        instance->SetAttribute("SchedulerIdentifier", UintegerValue(instanceId));
        instance->SetAttribute("SchedulerGroupIdentifier", UintegerValue(m_schedulerGroupId));
        instance->SetAttribute("CommittedInformationRate", DataRateValue(cir));
        instance->SetAttribute("CommittedBurstSize", UintegerValue(cbs));
        newInst->SetBucketEmptyTime(m_clock ? m_clock->GetLocalTime() : Seconds(0));
        m_instanceIdToInstanceMap[instanceId] = instance;
        return instanceId;
    }

    bool AtsScheduler::RegisterStreamToInstance(uint32_t streamHandler, uint32_t instanceId)
    {
        NS_LOG_FUNCTION(this << streamHandler << instanceId);
        if (m_streamHandlerToInstanceMap.find(streamHandler) != m_streamHandlerToInstanceMap.end())
        {
            NS_LOG_WARN("Stream handler " << streamHandler << " already has an associated ATS Scheduler Instance.");
            return false;
        }
        auto instanceIt = m_instanceIdToInstanceMap.find(instanceId);
        if (instanceIt == m_instanceIdToInstanceMap.end())
        {
            NS_LOG_WARN("ATS Scheduler Instance with ID " << instanceId << " does not exist.");
            return false;
        }
        m_streamHandlerToInstanceMap[streamHandler] = instanceIt->second;
        return true;
    }

    bool AtsScheduler::ProcessPacket(Ptr<Packet> packet, uint32_t stream_handler, uint8_t priority)
    {
        NS_ASSERT_MSG(m_clock != nullptr, "AtsScheduler: Clock non configurée.");
        Time currentTime = m_clock->GetLocalTime();
        uint32_t sizeBits = packet->GetSize() * 8;

        // Check if an instance is associated to the stream
        Ptr<AtsSchedulerInstance> instance = m_defaultInstance;
        auto it = m_streamHandlerToInstanceMap.find(streamHandler);
        if (it != m_streamHandlerToInstanceMap.end())
        {
            instance = it->second;
        }

        double cir = instance->GetCir().GetBitRate();
        uint32_t cbs = instance->GetCbs();
        if (cir <= 0)
        {
            return false;
        }

        // Process Frame logic
        Time lengthRecoveryDuration = Seconds(static_cast<double>(sizeBits) / cir);
        Time emptyToFullDuration = Seconds(static_cast<double>(cbs) / cir);

        Time schedulerEligibilityTime = instance->GetBucketEmptyTime() + lengthRecoveryDuration;
        Time bucketFullTime = instance->GetBucketEmptyTime() + emptyToFullDuration;

        Time eligibilityTime = Max(currentTime, Max(m_groupEligibilityTime, schedulerEligibilityTime));

        if (eligibilityTime <= (currentTime + m_maximumResidenceTime))
        {
            // The frame is valid
            m_groupElibilityTime = eligibilityTime;
            Time newBucketEmptyTime = (eligibilityTime < bucketFullTime) ? schedulerEligibilityTime : (schedulerEligibilityTime + eligibilityTime - bucketFullTime);
            instance->SetBucketEmptyTime(newBucketEmptyTime);

            // Add the time tag on the packet
            AtsEligibilityTimeTag tag;
            tag.SetEligibilityTime(eligibilityTime);
            tag.SetPriority(priority);
            packet->AddByteTag(tag);

            m_calendarQueue.insert(packet);

            if (eligibilityTime > currentTime)
            {
                Time delay = eligibilityTime - currentTime;
                if (!m_nextAtsTransmissionEvent.IsRunning())
                {
                    m_nextAtsTransmissionEvent = Simulator::Schedule(delay, &AtsScheduler::TriggerQueueCheck, this);
                }
            }
            return true;
        }
        NS_LOG_INFO("Packet dropped by ATS Scheduler: eligibility time " << eligibilityTime << " exceeds maximum residence time.");
        return false; // The packet is dropped
    }

    bool AtsScheduler::IsQueueEmpty() const { return m_calendarQueue.empty(); }
    Ptr<Packet> AtsScheduler::PeekTopPacket() const { return m_calendarQueue.empty() ? nullptr : *m_calendarQueue.begin(); }
    void AtsScheduler::DequeueTopPacket()
    {
        if (!m_calendarQueue.empty())
            m_calendarQueue.erase(m_calendarQueue.begin());
    }

    void AtsScheduler::TriggerQueueCheck()
    {
        if (m_netDevice)
        {
            m_netDevice->CheckForReadyPacket();
        }
    }
} // namespace ns3