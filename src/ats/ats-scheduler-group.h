#ifndef ATS_SCHEDULER_GROUP_H
#define ATS_SCHEDULER_GROUP_H

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/uinteger.h"
#include "ns3/data-rate.h"
#include "ns3/ats-scheduler-instance.h"
#include "ns3/tsn-net-device.h"
#include "ns3/packet.h"
#include <map>

namespace ns3
{
    class Ats;

    class AtsSchedulerGroup : public Object
    {
    public:
        /**
         * \brief Get the TypeId.
         *
         * \return The TypeId of this class.
         */
        static TypeId GetTypeId();

        /**
         * \brief Create an AtsSchedulerGroup.
         */
        AtsSchedulerGroup();

        /**
         * \brief Destroy an AtSchedulerGroup.
         */
        ~AtsSchedulerGroup();

        // Delete copy constructor and assignment operator to avoid misuse.
        AtsSchedulerGroup &operator=(const AtsSchedulerGroup &) = delete;
        AtsSchedulerGroup(const AtsSchedulerGroup &) = delete;

        /**
         * \brief Struct to hold packet information for the calendar queue.
         *
         * \var packet The packet to be scheduled.
         * \var priority The priority of the packet, used for tie-breaking in scheduling.
         * \var eligibilityTime The time at which the packet becomes eligible for transmission.
         * \var streamHandle The handle of the stream to which the packet belongs.
         */
        struct AtsPacketInfo
        {
            Ptr<Packet> packet;
            uint8_t priority;
            Time eligibilityTime;
            Time arrivalTime;
            Time hardwareLatency;
        };

        /**
         * \brief Create a generic ATS scheduler instance with specific attributes (cir, cbs).
         *
         * \param cir The Committed Information Rate (CIR) for the instance.
         * \param cbs The Committed Burst Size (CBS) for the instance.
         * \return The id of the created instance.
         */
        uint32_t CreateAtsInstance(DataRate cir, uint32_t cbs);

        /**
         * \brief Explicitly bind a stream handle to an existing ATS instance.
         *
         * \param streamId The unique identifier of the stream.
         * \param instanceId The id of the ATS instance.
         * \return True if the binding succeeded, false if the stream was already bound to an instance.
         */
        bool BindStreamToInstance(uint32_t streamId, uint32_t instanceId);

        /**
         * \brief Retrieve the ATS instance associated with a stream handle.
         * If none exists, a dedicated default instance is created for this stream.
         *
         * \param streamId The unique identifier of the stream.
         * \return The pointer to the ATS instance mapped to this stream.
         */
        Ptr<AtsSchedulerInstance> GetInstanceForStream(uint32_t streamId);

        /**
         * \brief Compute the eligibility time of a frame and put it in the calendarQueue.
         *
         * \param packet The packet we want to calculate the eligibilityTime.
         * \param streamId The identifier of the stream.
         * \param hardwareLatency The hardware latency experienced by the packet.
         * \return True if the packet is valid and has been added to the calendar queue, false if the packet is dropped.
         */
        bool ProcessFrame(Ptr<Packet> packet, uint32_t streamId, Time hardwareLatency);

        /**
         * \brief Handle the transmission of a frame when its eligibility time is reached.
         *
         */
        void HandleAtsTransmission();

        // Getters and Setters
        Time GetMaximumResidenceTime() const { return m_maximumResidenceTime; }
        Time GetGroupEligibilityTime() const { return m_groupEligibilityTime; }
        void SetGroupEligibilityTime(Time t) { m_groupEligibilityTime = t; }
        void SetNetDevice(Ptr<TsnNetDevice> d) { m_netDevice = d; }
        void SetAts(Ptr<Ats> ats) { m_ats = ats; }

    private:
        // Identification attribute
        uint32_t m_schedulerGroupId;
        uint32_t m_instanceIdCounter;

        // Map linking each instance id with the pointer of the actual instance
        std::map<uint32_t, Ptr<AtsSchedulerInstance>> m_idToInstanceMap;

        // Map linking each unique stream ID to its corresponding ATS Scheduler Instance
        std::map<uint32_t, Ptr<AtsSchedulerInstance>> m_streamToInstanceMap;

        // ATS group attributes values
        Time m_groupEligibilityTime;
        Time m_maximumResidenceTime;

        // Default instance parameters used for dynamic per-stream auto-instantiation
        DataRate m_defaultCir;
        uint32_t m_defaultCbs;

        /**
         * \brief This struct give a comparator for the calendar queue.
         * First compare the eligibility time of the packet,
         * then compare the priority to break ties.
         */
        struct AtsPacketComparator
        {
            bool operator()(const AtsPacketInfo &a, const AtsPacketInfo &b) const
            {
                // First compare the eligibility times
                if (a.eligibilityTime != b.eligibilityTime)
                {
                    return a.eligibilityTime < b.eligibilityTime; // Earlier eligibility time has higher priority.
                }
                // If eligibility times are equal, compare the arrival time
                return a.arrivalTime < b.arrivalTime;
            }
        };

        // Calendar queue to manage packet scheduling based on eligibility time and priority
        std::multiset<AtsPacketInfo, AtsPacketComparator> m_calendarQueue;
        EventId m_nextAtsTransmissionEvent;
        Time m_nextAtsTransmissionTime;

        Ptr<TsnNetDevice> m_netDevice;
        Ptr<Ats> m_ats;
    };

} // namespace ns3
#endif // ATS_SCHEDULER_GROUP_H