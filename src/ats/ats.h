#ifndef ATS_H
#define ATS_H

#include "ns3/object.h"
#include "ns3/ats-scheduler-group.h"
#include <map>
#include "ns3/clock.h"

namespace ns3
{

    class Ats : public Object
    {
    public:
        /**
         * \brief Get the TypeId.
         *
         * \return The TypeId of this class.
         */
        static TypeId GetTypeId();

        /**
         * \brief Create an Ats.
         */
        Ats();

        /**
         * \brief Destroy an Ats.
         */
        ~Ats();

        // Delete copy constructor and assignment operator to avoid misuse.
        Ats &operator=(const Ats &) = delete;
        Ats(const Ats &) = delete;

        // virtual local port for end-station
        static const uint32_t LOCAL_INPUT_PORT = 0xFFFFFFFF;

        /**
         * \brief Enqueue a frame into its corresponding ATS scheduler group.
         *
         * This method acts as the main entry point for the ATS (Asynchronous Traffic Shaping)
         * subsystem at the egress port. It resolves or dynamically creates the specific
         * AtsSchedulerGroup mapped to the triplet unique identifier:
         * (inputPortId, outputPortId, priority).
         *
         * For traffic originated locally (End Station), the inputPortId should be set to
         * Ats::LOCAL_INPUT_PORT (0xFFFFFFFF). For forwarded traffic (Bridge), the actual
         * incoming interface index must be provided.
         *
         * \param packet The network packet to be shaped and scheduled.
         * \param inputPortId The interface index of the ingress port, or LOCAL_INPUT_PORT if generated locally.
         * \param outputPortId The interface index of the egress port handling the transmission.
         * \param priority The Priority Code Point (PCP) or traffic class of the frame.
         * \param outputDevice Pointer to the egress TsnNetDevice where the packet will be reinjected.
         * \param hardwareLatency The cumulative hardware processing latency experienced by the packet.
         * \return True if the packet was successfully accepted into the ATS group queue,
         * false if it was dropped due to exceeding the maximum residence time.
         */
        bool EnqueueFrame(Ptr<Packet> packet,
                          uint32_t inputPortId, uint32_t outputPortId,
                          uint8_t priority, Ptr<TsnNetDevice> outputDevice,
                          Time hardwareLatency);

        /**
         * \brief Retrieve or dynamically create an AtsSchedulerGroup.
         * Useful for both internal routing and user-space configuration.
         *
         * \param inputPortId The interface index of the ingress port, or LOCAL_INPUT_PORT for locally generated traffic.
         * \param outputPortId The interface index of the egress port.
         * \param InternalId The internal id of the stream.
         * \param outputDevice Egress device to bind if a new group is created.
         * \return Pointer to the existing or newly created scheduler group.
         */
        Ptr<AtsSchedulerGroup> GetGroup(uint32_t inputPortId, uint32_t outputPortId, uint32_t internalId, Ptr<TsnNetDevice> outputDevice = nullptr);

        /**
         * \brief Ergonomic abstraction to retrieve a scheduler group for an End-Station (ES).
         * \details Automatically abstracts the internal localized port mapping and binds
         * the group directly to the egress device's unique interface index.
         *
         * \param destMac The destination mac address of the stream.
         * \param vlanId The vlan id of the stream.
         * \param egressDevice The outbound TsnNetDevice pointer of the End-Station.
         * \return Pointer to the existing or newly created scheduler group.
         */
        Ptr<AtsSchedulerGroup> GetGroupForEndStation(Mac48Address destMac, uint16_t vlanId, Ptr<TsnNetDevice> egressDevice);

        /**
         * \brief Ergonomic abstraction to retrieve a scheduler group inside a Bridge/Switch.
         * \details Automatically extracts interface indices from device pointers to prevent
         * manual indexing mismatches.
         *
         * \param ingressDevice The inbound TsnNetDevice where the frame entered the switch.
         * \param egressDevice The outbound TsnNetDevice handling the egress shaping.
         * \param priority The Traffic Class / PCP identifier mapping to the internal queue.
         * \return Pointer to the existing or newly created scheduler group.
         */
        Ptr<AtsSchedulerGroup> GetGroupForBridge(Ptr<TsnNetDevice> ingressDevice, Ptr<TsnNetDevice> egressDevice, uint8_t priority);

        // Getters and Setters
        Ptr<Clock> GetClock() { return m_clock; }
        void SetClock(Ptr<Clock> clock) { m_clock = clock; }

    private:
        /**
         * \brief Structure of unique key to identify an AtsSchedulerGroup.
         * Maps BOTH bridge aggregation triplets and per-stream end-station groups.
         */
        struct AtsGroupKey
        {
            uint32_t inputPortId;
            uint32_t outputPortId;
            uint32_t internalId;

            bool operator<(const AtsGroupKey &other) const
            {
                if (inputPortId != other.inputPortId)
                    return inputPortId < other.inputPortId;
                if (outputPortId != other.outputPortId)
                    return outputPortId < other.outputPortId;
                return internalId < other.internalId;
            }
        };

        // Map which contains each ATS group active in this ATS
        std::map<AtsGroupKey, Ptr<AtsSchedulerGroup>> m_groupsMap;

        // Pointer to the local clock
        Ptr<Clock> m_clock;

        // Default maximum residence time
        Time m_defaultMaximumResidenceTime;
    };
} // namespace ns3
#endif // ATS_H