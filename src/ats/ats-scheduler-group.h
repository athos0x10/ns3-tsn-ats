#ifndef ATS_SCHEDULER_GROUP_H
#define ATS_SCHEDULER_GROUP_H

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/uinteger.h"
#include "ns3/data-rate.h"
#include "ns3/clock.h"
#include "ns3/ats-scheduler-instance.h"
#include "ns3/tsn-net-device.h"
#include "ns3/packet.h"
#include <map>

namespace ns3
{
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
            uint32_t streamHandle;
            Time hardwareLatency;
        };

        /**
         * \brief Create an instance with his specific attributes (cir, cbs) and the priority.
         *
         * \param cir The Committed Information Rate (CIR) for the instance.
         * \param cbs The Committed Burst Size (CBS) for the instance.
         * \param priority The priority for which the instance is created (used when per-priority routing is enabled).
         * \return The ID of the created instance.
         *
         */
        uint32_t CreateAtsInstanceForPriority(DataRate cir, uint32_t cbs, uint8_t priority);

        /**
         * \brief Compute the eligibility time of a frame and put it in the calendarQueue.
         *
         * \param packet The packet we want to calculate the eligibilityTime.
         * \param streamHandle The handle of the stream.
         * \param hardwareLatency The hardware latency experienced by the packet.
         * \return True if the packet is valid and has been added to the calendar queue, false if the packet is dropped.
         */
        bool ProcessFrame(Ptr<Packet> packet, uint32_t streamHandle, Time hardwareLatency);

        /**
         * \brief Handle the transmission of a frame when its eligibility time is reached.
         *
         */
        void HandleAtsTransmission();

        // Getters and Setters
        Time GetMaximumResidenceTime() const { return m_maximumResidenceTime; }
        Time GetGroupEligibilityTime() const { return m_groupEligibilityTime; }
        void SetGroupEligibilityTime(Time t) { m_groupEligibilityTime = t; }
        void SetClock(Ptr<Clock> c) { m_clock = c; }
        void SetNetDevice(Ptr<TsnNetDevice> d) { m_netDevice = d; }
        void SetPerPriorityRouting(bool enable) { m_perPriorityRouting = enable; }

    private:
        // Identification attribute
        uint32_t m_schedulerGroupId;
        uint32_t m_instanceIdCounter;

        // bool to determine if the ATS Scheduler Group is configured for per-priority routing or not
        bool m_perPriorityRouting; // true by default, false if the ATS Scheduler Group is configured for per-stream routing

        // Map of instance IDs to their corresponding ATS Scheduler Instances
        std::map<uint32_t, Ptr<AtsSchedulerInstance>> m_instanceIdToInstanceMap;
        // Default Map  of priority to their corresponding ATS Scheduler Instances (used when per-priority routing is enabled)
        std::map<uint8_t, Ptr<AtsSchedulerInstance>> m_priorityToInstanceMap;
        // Default instance used when per-stream routing is enabled
        Ptr<AtsSchedulerInstance> m_defaultInstance;

        // ATS group attributes values
        Time m_groupEligibilityTime;
        Time m_maximumResidenceTime;

        /**
         * \brief This struct give a comparator for the calendar queue.
         * First compare the eligibility time of the packet,
         * then compare the priority to break ties.
         */
        struct AtsPacketComparator
        {
            bool operator()(const AtsPacketInfo a, const AtsPacketInfo b) const
            {
                // First compare the eligibility times
                if (a.eligibilityTime != b.eligibilityTime)
                {
                    return a.eligibilityTime < b.eligibilityTime; // Earlier eligibility time has higher priority.
                }
                // If eligibility times are equal, compare the priority values
                return a.priority > b.priority;
            }
        };

        // Calendar queue to manage packet scheduling based on eligibility time and priority
        std::multiset<AtsPacketInfo, AtsPacketComparator>
            m_calendarQueue;
        EventId m_nextAtsTransmissionEvent;
        Time m_nextAtsTransmissionTime;

        // Pointer to the local clock of the node
        Ptr<Clock> m_clock;
        // Pointer to the TsnNetDevice to trigger the transmission of the packet when its eligibility time is reached
        Ptr<TsnNetDevice> m_netDevice;
    };

} // namespace ns3
#endif // ATS_SCHEDULER_GROUP_H