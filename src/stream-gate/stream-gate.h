#ifndef STREAM_GATE_H
#define STREAM_GATE_H

#include "ns3/object.h"
#include "ns3/packet.h"

namespace ns3
{

    class StreamGate : public Object
    {
    public:
        /**
         * \brief Get the TypeId
         *
         * \return The TypeId for this class
         */
        static TypeId GetTypeId();

        /**
         * Enumeration of the states of the transimission gate.
         */
        enum State
        {
            OPEN, /**< The transimission gate is open */
            CLOSE /**< The transimission gate is close */
        };

        /**
         * \brief Create a StreamGate
         */
        StreamGate();

        /**
         * Destroy a StreamGate
         *
         * This is the destructor for the StreamGate.
         */
        ~StreamGate();

        /**
         * \brief Change the state and the ipv of the gate at the Start Time.
         *
         * \param gateState the new state of the gate.
         * \param ipvId the new id of the gate.
         * \param startTime the start time of our change.
         */
        void SetGateAndIpv(State gateState, uint8_t ipvId, Time startTime);

        /**
         * \brief Handle the change of the state and ipv of the gate.
         *
         * \param gateState the new state of the gate.
         * \param ipvId the new id of the gate.
         */
        void handleSetGateAndIpv(State gateState, uint8_t ipvId);

        // Delete copy constructor and assignment operator to avoid misuse
        StreamGate &operator=(const StreamGate &) = delete;
        StreamGate(const StreamGate &) = delete;

        bool IsOpen();
        void Open();
        void Close();
        uint8_t GetIpvId();
        bool GetIpvEnable();

    protected:
    private:
        /**
         * The state of the Net Device transmit state machine.
         */
        State m_state;

        /**
         * The boolean to enable IPV use on the gate
         */
        bool m_ipvEnable;

        /**
         * The value of the gate's IPV (must be between 0 and 7).
         */
        uint8_t m_ipv;
    };

}

#endif /* STREAM_GATE_H */
