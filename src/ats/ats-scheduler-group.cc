#include "ats-scheduler-group.h"

#include "ns3/log.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("AtsSchedulerGroup");
    NS_OBJECT_ENSURE_REGISTERED(AtsSchedulerGroup);

    TypeId
    AtsSchedulerGroup::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::AtsScheduler")
                .SetParent<Object>()
                .SetGroupName("Tsn")
                .AddConstructor<AtsSchedulerGroup>()
                .AddAttribute("SchedulerGroupIdentifier",
                              "The group identifier for this ATS scheduler.",
                              UintegerValue(0),
                              MakeUintegerAccessor(&AtsSchedulerGroup::m_schedulerGroupId),
                              MakeUintegerChecker<uint32_t>())
                .AddAttribute("MaxResidenceTime", "Maximum residence time inside the scheduler.",
                              TimeValue(MilliSeconds(10)),
                              MakeTimeAccessor(&AtsSchedulerGroup::m_maximumResidenceTime),
                              MakeTimeChecker());
        return tid;
    }

    AtsSchedulerGroup::AtsSchedulerGroup()
    {
        NS_LOG_FUNCTION(this);
        m_instanceIdCounter = 0; // Default instance ID is 0
        // Create default instance with default attributes
        m_defaultInstance = CreateObject<AtsSchedulerInstance>();
        m_defaultInstance->SetAttribute("SchedulerIdentifier", UintegerValue(m_instanceIdCounter)); // Default instance has ID 0
        m_defaultInstance->SetAttribute("SchedulerGroupIdentifier", UintegerValue(m_schedulerGroupId));
        m_defaultInstance->SetAttribute("CommittedInformationRate", DataRateValue(DataRate("100Mbps")));
        m_defaultInstance->SetAttribute("CommittedBurstSize", UintegerValue(12288)); // Default to 12KB
    }

    AtsSchedulerGroup::~AtsSchedulerGroup()
    {
        NS_LOG_FUNCTION(this);
    }

    uint32_t
    AtsSchedulerGroup::CreateAtsInstance(DataRate cir, uint32_t cbs)
    {
        NS_LOG_FUNCTION(this << cir << cbs);
        uint32_t instanceId = m_instanceIdCounter++;
        Ptr<AtsSchedulerInstance> instance = CreateObject<AtsSchedulerInstance>();
        instance->SetAttribute("SchedulerIdentifier", UintegerValue(instanceId));
        instance->SetAttribute("SchedulerGroupIdentifier", UintegerValue(m_schedulerGroupId));
        instance->SetAttribute("CommittedInformationRate", DataRateValue(cir));
        instance->SetAttribute("CommittedBurstSize", UintegerValue(cbs));
        instance->SetBucketEmptyTime(m_clock ? m_clock->GetLocalTime() : Seconds(0));
        m_instanceIdToInstanceMap[instanceId] = instance;
        return instanceId;
    }

    bool
    AtsSchedulerGroup::AssociateInstanceWithStream(uint32_t instanceId, uint32_t streamHandle)
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

    void
    AtsSchedulerGroup::ProcessFrame(Ptr<Packet> packet, uint3é_t streamHandle)
    {
        NS_ASSERT_MSG(m_clock != nullptr, "AtsScheduler: Clock not configured.");
        Time currentTime = m_clock->GetLocalTime();
        uint32_t sizeBits = packet->GetSize() * 8;

        // Retrieve the priority
        uint8_t priority;
        Ptr<Packet> packetCopy = packet->Copy();
        EthernetHeader2 ethHeader;
        packetCopy->RemoveHeader(ethHeader);
        priority = ethHeader.GetPcp();

        // Check if an instance is associated to the stream
        Ptr<AtsSchedulerInstance> instance = m_defaultInstance;
        auto it = m_streamHandlerToInstanceMap.find(stream_handler);
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
            m_groupEligibilityTime = eligibilityTime;
            Time newBucketEmptyTime = (eligibilityTime < bucketFullTime) ? schedulerEligibilityTime : (schedulerEligibilityTime + eligibilityTime - bucketFullTime);
            instance->SetBucketEmptyTime(newBucketEmptyTime);

            // Create AtsPacketInfo and insert it in the calendar queue
            AtsPacketInfo packetInfo;
            packetInfo.packet = packet;
            packetInfo.priority = priority;
            packetInfo.eligibilityTime = eligibilityTime;

            m_calendarQueue.insert(packetInfo);

            // Retrieve the earliest packet in the calendar queue to schedule the next transmission event
            AtsPacketInfo mostUrgentPacket = *m_calendarQueue.begin();
            Time nextEligibilityTime = mostUrgentPacket.eligibilityTime;

            Time delay = (nextEligibilityTime > currentTime) ? (nextEligibilityTime - currentTime) : Seconds(0);
            Time absoluteTargetTime = currentTime + delay;

            if (!m_nextAtsTransmissionEvent.IsRunning())
            {
                // No event is currently scheduled, we can schedule a new one
                m_nextAtsTransmissionEvent = Simulator::Schedule(delay, &AtsSchedulerGroup::HandleAtsTransmission, this);
                m_nextAtsTransmissionTime = absoluteTargetTime;
            }
            // An event is already scheduled, we need to check if the new packet is more urgent than the one already scheduled
            else if (absoluteTargetTime < m_nextAtsTransmissionTime)
            {
                Simulator::Cancel(m_nextAtsTransmissionEvent);
                m_nextAtsTransmissionEvent = Simulator::Schedule(delay, &AtsSchedulerGroup::HandleAtsTransmission, this);
            }
        }
        return true;
    }
    NS_LOG_INFO("Packet dropped by ATS Scheduler: eligibility time " << eligibilityTime << " exceeds maximum residence time.");
    return false; // The packet is dropped
}
}