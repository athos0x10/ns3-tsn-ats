#ifndef ATS_SCHEDULER_INSTANCE_H
#define ATS_SCHEDULER_INSTANCE_H

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/uinteger.h"
#include "ns3/data-rate.h"
#include "ns3/clock.h"

namespace ns3
{

    class AtsSchedulerInstance : public Object
    {
    public:
        /**
         * \brief Get the TypeId.
         *
         * \return The TypeId of this class.
         */
        static TypeId GetTypeId();

        /**
         * \brief Create an AtsSchedulerInstance.
         */
        AtsSchedulerInstance();

        /**
         * \brief Destroy an AtsSchedulerInstance.
         */
        ~AtsSchedulerInstance();

        // Delete copy constructor and assignment operator to avoid misuse.
        AtsSchedulerInstance &operator=(const AtsSchedulerInstance &) = delete;
        AtsSchedulerInstance(const AtsSchedulerInstance &) = delete;

        /**
         * \brief Calculates the ATS eligibility date of a package.
         *
         * \param size Incoming frame size.
         * \return Time the point at which the packet becomes eligible for transmission.
         */
        Time CalculateSchedulerEligibility(uint16_t size);

        void SetClock(Ptr<Clock> clock);

    private:
        // Identification attributes
        uint32_t m_schedulerIdentifier;
        uint8_t m_schedulerGroupIdentifier;

        // Algorithm attributes
        DataRate m_committedInformationRate;
        uint32_t m_committedBurstSize;
        Time m_bucketEmptyTime;
        Time m_emptyToFullDuration;

        // Copy of the local clock
        Ptr<Clock> m_clock;
    };

} // namespace ns3

#endif // ATS_SCHEDULER_INSTANCE_H