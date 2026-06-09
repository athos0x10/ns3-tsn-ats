#include "ats-eligibility-time-tag.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("AtsEligibilityTimeTag");
    NS_OBJECT_ENSURE_REGISTERED(AtsEligibilityTimeTag);

    TypeId
    AtsEligibilityTimeTag::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::AtsEligibilityTimeTag")
                .SetParent<Tag>()
                .SetGroupName("Tsn")
                .AddConstructor<AtsEligibilityTimeTag>();
        return tid;
    }

    AtsEligibilityTimeTag::AtsEligibilityTimeTag()
    {
        NS_LOG_FUNCTION(this);
    }

    AtsEligibilityTimeTag::~AtsEligibilityTimeTag()
    {
        NS_LOG_FUNCTION(this);
    }

    TypeId
    AtsEligibilityTimeTag::GetInstanceTypeId() const
    {
        return GetTypeId();
    }

    uint32_t
    AtsEligibilityTimeTag::GetSerializedSize() const
    {
        // 8 bytes for Time (int64_t) and 1 byte for priority (uint8_t)
        return sizeof(int64_t) + sizeof(uint8_t);
    }

    void
    AtsEligibilityTimeTag::Serialize(TagBuffer i) const
    {
        i.WriteU64(m_eligibilityTime.GetInteger());
        i.WriteU8(m_priority);
    }

    void
    AtsEligibilityTimeTag::Deserialize(TagBuffer i)
    {
        m_eligibilityTime = TimeValue(Time(i.ReadU64())).Get();
        m_priority = i.ReadU8();
    }

    void
    AtsEligibilityTimeTag::Print(std::ostream &os) const
    {
        os << "EligibilityTime=" << m_eligibilityTime.As(Time::NS) << ", Priority=" << (unsigned)m_priority;
    }

    void AtsEligibilityTimeTag::SetEligibilityTime(Time time) { m_eligibilityTime = time; }
    Time AtsEligibilityTimeTag::GetEligibilityTime() const { return m_eligibilityTime; }
    void AtsEligibilityTimeTag::SetPriority(uint8_t priority) { m_priority = priority; }
    uint8_t AtsEligibilityTimeTag::GetPriority() const { return m_priority; }

} // namespace ns3