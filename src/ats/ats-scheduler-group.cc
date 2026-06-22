#include "ats-scheduler-group.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/ethernet-header2.h"
#include "ns3/ats.h"
#include "ns3/timestamp-tag.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("AtsSchedulerGroup");
    NS_OBJECT_ENSURE_REGISTERED(AtsSchedulerGroup);

    TypeId
    AtsSchedulerGroup::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::AtsSchedulerGroup")
                .SetParent<Object>()
                .SetGroupName("Tsn")
                .AddConstructor<AtsSchedulerGroup>()
                .AddAttribute("SchedulerGroupIdentifier",
                              "The group identifier for this ATS scheduler.",
                              UintegerValue(0),
                              MakeUintegerAccessor(&AtsSchedulerGroup::m_schedulerGroupId),
                              MakeUintegerChecker<uint32_t>())
                .AddAttribute("MaxResidenceTime",
                              "Maximum residence time inside the scheduler.",
                              TimeValue(Seconds(1)),
                              MakeTimeAccessor(&AtsSchedulerGroup::m_maximumResidenceTime),
                              MakeTimeChecker())
                .AddAttribute("DefaultCir",
                              "The default Committed Information Rate (CIR) for dynamic instance creation.",
                              DataRateValue(DataRate("10Mbps")),
                              MakeDataRateAccessor(&AtsSchedulerGroup::m_defaultCir),
                              MakeDataRateChecker())
                .AddAttribute("DefaultCbs",
                              "The default Committed Burst Size (CBS) in bits for dynamic instance creation.",
                              UintegerValue(16384), // Default to 2048 bytes (16384 bits)
                              MakeUintegerAccessor(&AtsSchedulerGroup::m_defaultCbs),
                              MakeUintegerChecker<uint32_t>());
        return tid;
    }

    AtsSchedulerGroup::AtsSchedulerGroup()
    {
        NS_LOG_FUNCTION(this);
        m_instanceIdCounter = 0;
        m_groupEligibilityTime = Seconds(0);
        m_nextAtsTransmissionTime = Seconds(0);
    }

    AtsSchedulerGroup::~AtsSchedulerGroup()
    {
        NS_LOG_FUNCTION(this);
    }

    uint32_t
    AtsSchedulerGroup::CreateAtsInstance(DataRate cir, uint32_t cbs)
    {
        NS_LOG_FUNCTION(this << cir << cbs);

        NS_ASSERT_MSG(cir.GetBitRate() > 0, "AtsSchedulerGroup: The Committed Information Rate (CIR) must be greater than 0.");
        NS_ASSERT_MSG(cbs > 0, "AtsSchedulerGroup: The Committed Burst Size (CBS) must be greater than 0.");

        uint32_t instanceId = m_instanceIdCounter++;
        Ptr<AtsSchedulerInstance> instance = CreateObject<AtsSchedulerInstance>();

        instance->SetAttribute("SchedulerIdentifier", UintegerValue(instanceId));
        instance->SetAttribute("SchedulerGroupIdentifier", UintegerValue(m_schedulerGroupId));
        instance->SetAttribute("CommittedInformationRate", DataRateValue(cir));
        instance->SetAttribute("CommittedBurstSize", UintegerValue(cbs));

        // Set initial bucket empty time to current clock time or fallback to 0
        Time currentTime = m_ats->GetClock() ? m_ats->GetClock()->GetLocalTime() : Seconds(0);
        instance->SetBucketEmptyTime(currentTime);

        // Store the instance in our ID-to-Instance mapping
        m_idToInstanceMap[instanceId] = instance;
        return instanceId;
    }

    bool
    AtsSchedulerGroup::BindStreamToInstance(uint32_t streamId, uint32_t instanceId)
    {
        NS_LOG_FUNCTION(this << streamId << instanceId);

        // Check if the requested ATS instance actually exists in this group
        auto itInstance = m_idToInstanceMap.find(instanceId);
        if (itInstance == m_idToInstanceMap.end())
        {
            NS_LOG_ERROR("AtsSchedulerGroup: ATS Instance ID " << instanceId << " does not exist in this group.");
            return false;
        }

        // Check if the stream was already bound somewhere else
        auto itStream = m_streamToInstanceMap.find(streamId);
        if (itStream != m_streamToInstanceMap.end())
        {
            NS_LOG_INFO("AtsSchedulerGroup: Stream " << streamId
                                                     << " is already bound. Moving it to the new Instance ID " << instanceId);
        }

        // Bind (or overwrite) the stream to the existing instance pointer
        m_streamToInstanceMap[streamId] = itInstance->second;

        return true;
    }

    Ptr<AtsSchedulerInstance>
    AtsSchedulerGroup::GetInstanceForStream(uint32_t streamId)
    {
        NS_LOG_FUNCTION(this << streamId);

        auto it = m_streamToInstanceMap.find(streamId);
        if (it != m_streamToInstanceMap.end())
        {
            return it->second;
        }

        // If no mapping exists, auto-instanciate a dedicated ATS instance
        NS_LOG_INFO("AtsSchedulerGroup: No explicit instance found for Stream " << streamId
                                                                                << ". Creating a dynamic default instance.");

        // Create the new instance using default group values
        uint32_t dynamicInstanceId = CreateAtsInstance(m_defaultCir, m_defaultCbs);

        // Retrieve the newly created pointer from our instance map
        Ptr<AtsSchedulerInstance> dynamicInstance = m_idToInstanceMap[dynamicInstanceId];

        // Bind this stream ID to the newly created instance permanently
        m_streamToInstanceMap[streamId] = dynamicInstance;

        return dynamicInstance;
    }

    bool
    AtsSchedulerGroup::ProcessFrame(Ptr<Packet> packet, uint32_t streamId, Time hardwareLatency)
    {
        Time currentTime = (m_ats && m_ats->GetClock()) ? m_ats->GetClock()->GetLocalTime() : Simulator::Now();
        uint32_t sizeBits = packet->GetSize() * 8;

        Time arrivalTime = Simulator::Now();
        TimestampTag tag;
        if (packet->FindFirstMatchingByteTag(tag))
        {
            arrivalTime = tag.GetTimestamp();
        }

        // Retrieve the priority
        uint8_t priority;
        Ptr<Packet> packetCopy = packet->Copy();
        EthernetHeader2 ethHeader;
        packetCopy->RemoveHeader(ethHeader);
        priority = ethHeader.GetPcp();

        // Retrieve the instance associated with the stream
        Ptr<AtsSchedulerInstance> instance = GetInstanceForStream(streamId);

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
        Time residenceDelay = eligibilityTime - arrivalTime;

        // --- ATS DEBUG LOGS ---
        NS_LOG_DEBUG("[ATS-DEBUG] Packet UID: " << packet->GetUid()
                                                << " | Size: " << packet->GetSize() << " Bytes (" << sizeBits << " bits)"
                                                << " | Arrival: " << arrivalTime.As(Time::S)
                                                << " | CurrentTime: " << currentTime.As(Time::S));
        NS_LOG_DEBUG("[ATS-DEBUG] LengthRecovery: " << lengthRecoveryDuration.As(Time::S)
                                                    << " | BucketEmptyTime Before: " << instance->GetBucketEmptyTime().As(Time::S)
                                                    << " | SchedulerEligibilityTime: " << schedulerEligibilityTime.As(Time::S));
        NS_LOG_DEBUG("[ATS-DEBUG] Evaluated EligibilityTime: " << eligibilityTime.As(Time::S)
                                                               << " | Residence Delay: " << residenceDelay.As(Time::S)
                                                               << " | Max Allowed Residence: " << m_maximumResidenceTime.As(Time::S));
        // -------------------------------

        if (eligibilityTime <= (currentTime + m_maximumResidenceTime))
        {
            NS_LOG_DEBUG("[ATS-DEBUG] Packet " << packet->GetUid() << " -> PASSED (Compliant)");
            // The frame is valid
            m_groupEligibilityTime = eligibilityTime;
            Time newBucketEmptyTime = (eligibilityTime < bucketFullTime) ? schedulerEligibilityTime : (schedulerEligibilityTime + eligibilityTime - bucketFullTime);
            instance->SetBucketEmptyTime(newBucketEmptyTime);

            // Create AtsPacketInfo and insert it in the calendar queue
            AtsPacketInfo packetInfo;
            packetInfo.packet = packet;
            packetInfo.priority = priority;
            packetInfo.eligibilityTime = eligibilityTime;
            packetInfo.hardwareLatency = hardwareLatency;
            packetInfo.arrivalTime = arrivalTime;

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
        NS_LOG_WARN("[ATS-DEBUG] Packet " << packet->GetUid() << " -> DROPPED (Residence delay "
                                          << residenceDelay.As(Time::S) << " > " << m_maximumResidenceTime.As(Time::S) << ")");
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

        Time currentTime = (m_ats && m_ats->GetClock()) ? m_ats->GetClock()->GetLocalTime() : Simulator::Now();

        // Retrieve the packet to transmit and erase it from the calendarQueue
        auto it = m_calendarQueue.begin();
        AtsPacketInfo urgentPacket = *it;
        m_calendarQueue.erase(it);

        // Forward the packet
        if (m_netDevice)
        {
            m_netDevice->EnqueueAfterAts(urgentPacket.packet, urgentPacket.priority, urgentPacket.hardwareLatency);
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
} // namespace ns3