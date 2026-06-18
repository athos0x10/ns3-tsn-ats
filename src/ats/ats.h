#ifndef ATS_H
#define ATS_H

#include "ns3/object.h"
#include "ns3/ats-scheduler-group.h"
#include <map>

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
         * \param streamHandle The unique identifier of the TSN stream this packet belongs to.
         * \param inputPortId The interface index of the ingress port, or LOCAL_INPUT_PORT if generated locally.
         * \param outputPortId The interface index of the egress port handling the transmission.
         * \param priority The Priority Code Point (PCP) or traffic class of the frame.
         * \param outputDevice Pointer to the egress TsnNetDevice where the packet will be reinjected.
         * \param hardwareLatency The cumulative hardware processing latency experienced by the packet.
         * \return True if the packet was successfully accepted into the ATS group queue,
         * false if it was dropped due to exceeding the maximum residence time.
         */
        bool EnqueueFrame(Ptr<Packet> packet, uint32_t streamHandle,
                          uint32_t inputPortId, uint32_t outputPortId,
                          uint8_t priority, Ptr<TsnNetDevice> outputDevice,
                          Time hardwareLatency);

    private:
        // Structure of unique key to identifie an AtsSchedulerGroup
        struct AtsGroupKey
        {
            uint32_t inputPortId;
            uint32_t outputPortId;
            uint8_t priority;

            bool operator<(const AtsGroupKey &other) const
            {
                if (inputPortId != other.inputPortId)
                    return inputPortId < other.inputPortId;
                if (outputPortId != other.outputPortId)
                    return outputPortId < other.outputPortId;
                return priority < other.priority;
            }
        };

        // Map which contains each ATS group active in this ATS
        std::map<AtsGroupKey, Ptr<AtsSchedulerGroup>> m_groupsMap;
    };
} // namespace ns3
#endif // ATS_H