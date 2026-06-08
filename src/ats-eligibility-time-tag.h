#ifndef ATS_ELIGIBILITY_TIME_TAG_H
#define ATS_ELIGIBILITY_TIME_TAG_H

#include "ns3/tag.h"
#include "ns3/nstime.h"

namespace ns3
{
    /**
     * \brief A tag to store eligible time for a packet in the ATS algorithm.
     * This tag also contains the priority of the packet,
     * which is used to determine the order of transmission
     * when multiple packets are eligible at the same time.
     *
     */
    class AtsEligibilityTimeTag : public Tag
    {
    public:
        /**
         * \brief Get the TypeId.
         *
         * \return The TypeId of this class.
         */
        static TypeId GetTypeId();

        /**
         * \brief Create an AtsEligibilityTimeTag.
         */
        AtsEligibilityTimeTag();

        /**
         * \brief Destroy an AtsEligibilityTimeTag.
         */
        ~AtsEligibilityTimeTag() override;

        // Specialized methods for Tag interface
        TypeId GetInstanceTypeId() const override;
        uint32_t GetSerializedSize() const override;
        void Serialize(TagBuffer i) const override;
        void Deserialize(TagBuffer i) override;
        void Print(std::ostream &os) const override;

        // Delete copy constructor and assignment operator to avoid misuse.
        AtsEligibilityTimeTag &operator=(const AtsEligibilityTimeTag &) = delete;
        AtsEligibilityTimeTag(const AtsEligibilityTimeTag &) = delete;

        void SetEligibilityTime(Time time);
        Time GetEligibilityTime() const;

        // PCP or IPV priority value, range from 0 to 7
        void SetPriority(uint8_t priority);
        uint8_t GetPriority() const;

    private:
        Time m_eligibilityTime;
        uint8_t m_priority;
    };
} // namespace ns3
#endif // ATS_ELIGIBILITY_TIME_TAG_H