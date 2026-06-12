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
         */
        struct AtsPacketInfo
        {
            Ptr<Packet> packet,
                uint8_t priority,
                Time eligibilityTime,
        };

        /**
         * \brief Create an instance with his specific attributes (cir, cbs).
         *
         * \param cir The Committed Information Rate (CIR) for the instance.
         * \param cbs The Committed Burst Size (CBS) for the instance.
         * \return The ID of the created instance.
         *
         */
        uint32_t CreateAtsInstance(DataRate cir, uint32_t cbs);

        /**
         * \brief Associate an ATS Scheduler Instance with a specific stream handler.
         *
         * \param instanceId The ID of the ATS Scheduler Instance to associate.
         * \param streamHandle The handle of the stream to associate with the instance.
         * \return True if the association was successful, false otherwise.
         */
        bool AssociateInstanceWithStream(uint32_t instanceId, uint32_t streamHandle);

        /**
         * \brief Compute the eligibility time of a frame and put it in the calendarQueue.
         *
         * \param packet The packet we want to calculate the eligibilityTime.
         * \param streamHandle The handle of the stream.
         */
        void ProcessFrame(Ptr<Packet> packet, uint32_t streamHandle);

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

    private:
        // Identification attribute
        uint32_t m_schedulerGroupId;
        uint32_t m_instanceIdCounter;

        // Map of stream handlers to their associated ATS Scheduler Instances
        std::map<uint32_t, Ptr<AtsSchedulerInstance>> m_streamHandlerToInstanceMap;
        // Map of instance IDs to their corresponding ATS Scheduler Instances
        std::map<uint32_t, Ptr<AtsSchedulerInstance>> m_instanceIdToInstanceMap;
        // Default ATS Scheduler Instance for stream handlers that do not have a specific instance associated
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
    }

}