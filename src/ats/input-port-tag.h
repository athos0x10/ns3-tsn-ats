#ifndef INPUT_PORT_TAG_H
#define INPUT_PORT_TAG_H

#include "ns3/tag.h"
#include "ns3/type-id.h"
#include <iostream>

namespace ns3
{

    class InputPortTag : public Tag
    {
    public:
        static TypeId GetTypeId(void);
        virtual TypeId GetInstanceTypeId(void) const override;

        virtual uint32_t GetSerializedSize(void) const override { return 4; }
        virtual void Serialize(TagBuffer i) const override { i.WriteU32(m_inputPortId); }
        virtual void Deserialize(TagBuffer i) override { m_inputPortId = i.ReadU32(); }
        virtual void Print(std::ostream &os) const override { os << "InputPortId=" << m_inputPortId; }

        void SetInputPortId(uint32_t id) { m_inputPortId = id; }
        uint32_t GetInputPortId(void) const { return m_inputPortId; }

    private:
        uint32_t m_inputPortId;
    };

} // namespace ns3

#endif /* INPUT_PORT_TAG_H */