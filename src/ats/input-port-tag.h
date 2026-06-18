#include "ns3/tag.h"

class InputPortTag : public Tag
{
public:
    static TypeId GetTypeId(void);
    virtual TypeId GetInstanceTypeId(void) const;
    virtual uint32_t GetSerializedSize(void) const { return 4; }
    virtual void Serialize(TagBuffer i) const { i.WriteU32(m_inputPortId); }
    virtual void Deserialize(TagBuffer i) { m_inputPortId = i.ReadU32(); }
    virtual void Print(std::ostream &os) const { os << "InputPortId=" << m_inputPortId; }

    void SetInputPortId(uint32_t id) { m_inputPortId = id; }
    uint32_t GetInputPortId() const { return m_inputPortId; }

private:
    uint32_t m_inputPortId;
};