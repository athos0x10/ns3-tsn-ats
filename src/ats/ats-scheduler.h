#ifndef ATS_SCHEDULER_H
#define ATS_SCHEDULER_H

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/uinteger.h"
#include "ns3/data-rate.h"
#include "ns3/clock.h"
#include "ns3/ats-scheduler-instance.h"
#include "ns3/tsn-net-device.h"
#include "ns3/packet.h"
#include "ns3/ats-eligibility-time-tag.h"
#include <map>
#include <set>

namespace ns3
{
    class TsnNetDevice;

    class AtsScheduler : public Object
    {
    public:
        /**
         * \brief Get the TypeId.
         *
         * \return The TypeId of this class.
         */
        static TypeId GetTypeId();

        /**
         * \brief Create an AtsScheduler.
         */
        AtsScheduler();

        /**
         * \brief Destroy an AtsScheduler.
         */
        ~AtsScheduler() override;

        // Delete copy constructor and assignment operator to avoid misuse.
        AtsScheduler &operator=(const AtsScheduler &) = delete;
        AtsScheduler(const AtsScheduler &) = delete;

        void SetTsnNetDevice(Ptr<TsnNetDevice> device);
        void SetClock(Ptr<Clock> clock);

        /**
         * \brief Process a packet for transmission according to the ATS algorithm.
         *
         * \param packet The packet to be processed.
         * \param streamHandler The handler of the stream to which the packet belongs.
         * \return True if the packet is allowed to be transmitted, false if it is dropped
         */
        bool ProcessPacket(Ptr<Packet> packet, uint32_t streamHandler);

        /**
         * \brief Create a new AtsSchedulerInstance with the specified attributes.
         *
         * \param cir The Committed Information Rate (CIR) for the instance.
         * \param cbs The Committed Burst Size (CBS) for the instance.
         * \return The id of the created instance.
         */
        uint32_t CreateSchedulerInstance(DataRate cir, uint32_t cbs);

        /**
         * \brief Associate an AtsSchedulerInstance with a stream handler.
         *
         * \param streamHandler The handler of the stream.
         * \param instanceId The AtsSchedulerInstance to be associated with the stream handler.
         * \return True if the association is successful, false if the stream handler already has an associated instance.
         */
        bool RegisterStreamToInstance(uint32_t streamHandler, uint32_t instanceId);

        // Function to allow the transmission selector to manipulate the queue
        bool IsQueueEmpty() const;
        Ptr<Packet> PeekTopPacket() const;
        void DequeueTopPacket();

    private:
        /**
         * \brief This class give a comparator for the packet queue.
         * First compare the time-tag of the packet,
         * then compare the priority to break ties.
         */
        struct AtsPacketComparator
        {
            bool operator()(const Ptr<Packet> &a, const Ptr<Packet> &b) const
            {
                // Retrieve the ATS eligibility time tags from the packets
                AtsEligibilityTimeTag tagA, tagB;
                bool hasTagA = a->FindFirstMatchingByteTag(tagA);
                bool hasTagB = b->FindFirstMatchingByteTag(tagB);

                // Retrieve the time eligibility for comparison
                Time eligibilityA = hasTagA ? tagA.GetEligibilityTime() : Seconds(0);
                Time eligibilityB = hasTagB ? tagB.GetEligibilityTime() : Seconds(0);

                // First compare the eligibility times
                if (eligibilityA != eligibilityB)
                {
                    return eligibilityA < eligibilityB; // Earlier eligibility time has higher priority
                }
                // If eligibility times are equal, compare the priority values
                uint32_t priorityA = hasTagA ? tagA.GetPriority() : 0;
                uint32_t priorityB = hasTagB ? tagB.GetPriority() : 0;
                return priorityA > priorityB;
            }
        };

        // Map of stream handlers to their associated ATS Scheduler Instances
        std::map<uint32_t, Ptr<AtsSchedulerInstance>> m_streamHandlerToInstanceMap;
        // Map of instance IDs to their corresponding ATS Scheduler Instances
        std::map<uint32_t, Ptr<AtsSchedulerInstance>> m_instanceIdToInstanceMap;
        // Default ATS Scheduler Instance for stream handlers that do not have a specific instance associated
        Ptr<AtsSchedulerInstance> m_defaultInstance;

        // Identification attribute
        uint32_t m_schedulerGroupId;
        uint32_t m_instanceIdCounter;

        // Pointer to the device and his local clock
        Ptr<Clock> m_clock;
        Ptr<TsnNetDevice> m_netDevice;

        // ATS attribute values
        Time m_groupEligibilityTime;
        Time m_maximumResidenceTime;

        // Calendar queue to manage packet scheduling based on eligibility time and priority
        std::multiset<Ptr<Packet>, AtsPacketComparator> m_calendarQueue;
        EventId m_nextAtsTransmissionEvent;
        void TriggerQueueCheck();
    };
} // namespace ns3
#endif // ATS_SCHEDULER_H