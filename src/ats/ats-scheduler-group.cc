#include "ats-scheduler-group.h"

#include "ns3/log.h"
#include "ns3/ethernet-header2.h"

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
                .AddAttribute("PerPriorityRouting",
                              "Whether the ATS Scheduler Group is configured for per-priority routing (true) or per-stream routing (false).",
                              BooleanValue(true),
                              MakeBooleanAccessor(&AtsSchedulerGroup::m_perPriorityRouting),
                              MakeBooleanChecker())
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

        for (uint8_t pcp = 0; pcp < 8; pcp++)
        {
            uint32_t instanceId = m_instanceIdCounter++;
            Ptr<AtsSchedulerInstance> instance = CreateObject<AtsSchedulerInstance>();

            // Configure the instance with default values
            instance->SetAttribute("SchedulerIdentifier", UintegerValue(instanceId));
            instance->SetAttribute("SchedulerGroupIdentifier", UintegerValue(m_schedulerGroupId));
            instance->SetAttribute("CommittedInformationRate", DataRateValue(DataRate("100Mbps")));
            instance->SetAttribute("CommittedBurstSize", UintegerValue(12288)); // 12 KB
            instance->SetBucketEmptyTime(m_clock ? m_clock->GetLocalTime() : Seconds(0));

            // Register the instance in the maps
            m_instanceIdToInstanceMap[instanceId] = instance;
            m_priorityToInstanceMap[pcp] = instance;

            // Use the instance associated with PCP 0 as the default instance for per-stream routing
            if (pcp == 0)
            {
                m_defaultInstance = instance;
            }
        }
    }

    AtsSchedulerGroup::~AtsSchedulerGroup()
    {
        NS_LOG_FUNCTION(this);
    }

    uint32_t
    AtsSchedulerGroup::CreateAtsInstanceForPriority(DataRate cir, uint32_t cbs, uint8_t priority)
    {
        NS_LOG_FUNCTION(this << cir << cbs << priority);

        NS_ASSERT_MSG(priority < 8, "AtsSchedulerGroup: La priorité PCP doit être comprise entre 0 et 7.");
        NS_ASSERT_MSG(cir.GetBitRate() > 0, "AtsSchedulerGroup: Le Committed Information Rate (CIR) doit être supérieur à 0.");
        NS_ASSERT_MSG(cbs > 0, "AtsSchedulerGroup: Le Committed Burst Size (CBS) doit être supérieur à 0.");

        uint32_t instanceId = m_instanceIdCounter++;
        Ptr<AtsSchedulerInstance> instance = CreateObject<AtsSchedulerInstance>();
        instance->SetAttribute("SchedulerIdentifier", UintegerValue(instanceId));
        instance->SetAttribute("SchedulerGroupIdentifier", UintegerValue(m_schedulerGroupId));
        instance->SetAttribute("CommittedInformationRate", DataRateValue(cir));
        instance->SetAttribute("CommittedBurstSize", UintegerValue(cbs));
        instance->SetBucketEmptyTime(m_clock ? m_clock->GetLocalTime() : Seconds(0));
        m_instanceIdToInstanceMap[instanceId] = instance;
        m_priorityToInstanceMap[priority] = instance;
        return instanceId;
    }

    bool
    AtsSchedulerGroup::ProcessFrame(Ptr<Packet> packet, uint32_t streamHandle, Time hardwareLatency)
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

        // Retrieve the instance associated with the priority or the stream
        Ptr<AtsSchedulerInstance> instance = m_defaultInstance;

        if (m_perPriorityRouting)
        {
            auto it = m_priorityToInstanceMap.find(priority);
            if (it != m_priorityToInstanceMap.end())
            {
                instance = it->second;
            }
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
            packetInfo.streamHandle = streamHandle;
            packetInfo.hardwareLatency = hardwareLatency;

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
                m_nextAtsTransmissionTime = absoluteTargetTime;
            }
            return true;
        }
        NS_LOG_INFO("Packet dropped by ATS Scheduler: eligibility time " << eligibilityTime << " exceeds maximum residence time.");
        return false; // The packet is dropped
    }

    void
    AtsSchedulerGroup::HandleAtsTransmission()
    {
        NS_LOG_FUNCTION(this);

        if (m_calendarQueue.empty())
        {
            NS_LOG_WARN("HandleAtsTransmission called but calendar queue is empty.");
            return;
        }

        Time currentTime = m_clock->GetLocalTime();

        // Retrieve the packet to transmit and erase it from the calendarQueue
        auto it = m_calendarQueue.begin();
        AtsPacketInfo urgentPacket = *it;
        m_calendarQueue.erase(it);

        // Forward the packet
        if (m_netDevice)
        {
            m_netDevice->ForwardUp(urgentPacket.packet, urgentPacket.streamHandle, urgentPacket.hardwareLatency);
        }

        // Schedule next packet if any
        if (!m_calendarQueue.empty())
        {
            AtsPacketInfo nextPacket = *m_calendarQueue.begin();
            Time nextEligibility = nextPacket.eligibilityTime;

            Time delay = (nextEligibility > currentTime) ? (nextEligibility - currentTime) : Seconds(0);

            m_nextAtsTransmissionEvent = Simulator::Schedule(delay, &AtsSchedulerGroup::HandleAtsTransmission, this);
            m_nextAtsTransmissionTime = currentTime + delay;
        }
    }
}