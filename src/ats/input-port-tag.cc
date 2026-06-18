#include "input-port-tag.h"
#include "ns3/log.h"

namespace ns3
{

    NS_LOG_COMPONENT_DEFINE("InputPortTag");

    TypeId
    InputPortTag::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::InputPortTag")
                                .SetParent<Tag>()
                                .SetGroupName("Tsn")
                                .AddConstructor<InputPortTag>();
        return tid;
    }

    TypeId
    InputPortTag::GetInstanceTypeId(void) const
    {
        return GetTypeId();
    }

} // namespace ns3