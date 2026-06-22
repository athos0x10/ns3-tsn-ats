#ifndef STREAM_ID_TAG_H
#define STREAM_ID_TAG_H

#include "ns3/tag.h"
#include "ns3/core-module.h"

namespace ns3
{

    class StreamIdTag : public Tag
    {
    public:
        static TypeId GetTypeId(void)
        {
            static TypeId tid = TypeId("ns3::StreamIdTag")
                                    .SetParent<Tag>()
                                    .SetGroupName("Tsn")
                                    .AddConstructor<StreamIdTag>();
            return tid;
        }

        TypeId GetInstanceTypeId(void) const override { return GetTypeId(); }

        uint32_t GetSerializedSize(void) const override { return 4; }

        void Serialize(TagBuffer i) const override { i.WriteU32(m_streamId); }
        void Deserialize(TagBuffer i) override { m_streamId = i.ReadU32(); }

        void Print(std::ostream &os) const override { os << "StreamId=" << m_streamId; }

        void SetStreamId(uint32_t id) { m_streamId = id; }
        uint32_t GetStreamId(void) const { return m_streamId; }

    private:
        uint32_t m_streamId{0};
    };

} // namespace ns3

#endif // STREAM_ID_TAG_H