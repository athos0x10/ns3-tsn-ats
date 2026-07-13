#ifndef ATS_SCHEDULER_INSTANCE_H
#define ATS_SCHEDULER_INSTANCE_H

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/uinteger.h"
#include "ns3/data-rate.h"
#include "ns3/traced-callback.h"

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

        // Getters and setters for attributes so that they can be accessed and modified by ATS Scheduler.
        DataRate GetCir() const { return m_committedInformationRate; }
        uint32_t GetCbs() const { return m_committedBurstSize; }

        Time GetBucketEmptyTime() const { return m_bucketEmptyTime; }
        void SetBucketEmptyTime(Time t) { m_bucketEmptyTime = t; }

        uint32_t GetSchedulerIdentifier() const { return m_schedulerIdentifier; }
        uint32_t GetSchedulerGroupIdentifier() const { return m_schedulerGroupIdentifier; }

        void TraceTokens(Time currentTime);

    private:
        // Identification attributes
        uint32_t m_schedulerIdentifier;
        uint32_t m_schedulerGroupIdentifier;

        // Instance attributes
        DataRate m_committedInformationRate;
        uint32_t m_committedBurstSize;
        Time m_bucketEmptyTime;
        TracedCallback<double> m_tokensTrace;
    };

} // namespace ns3

#endif // ATS_SCHEDULER_INSTANCE_H