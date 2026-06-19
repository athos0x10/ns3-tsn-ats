#include "ats.h"

#include "ns3/log.h"
#include "ns3/ats-scheduler-group.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("Ats");
    NS_OBJECT_ENSURE_REGISTERED(Ats);

    TypeId
    Ats::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::Ats")
                .SetParent<Object>()
                .SetGroupName("Tsn")
                .AddConstructor<Ats>();
        return tid;
    }

    Ats::Ats()
    {
        NS_LOG_FUNCTION(this);
    }

    Ats::~Ats()
    {
        NS_LOG_FUNCTION(this);
    }

    Ptr<AtsSchedulerGroup>
    Ats::GetGroup(const AtsGroupKey &key, Ptr<TsnNetDevice> outputDevice)
    {
        NS_LOG_FUNCTION(this << key.inputPortId << key.outputPortId << key.internalId);

        auto it = m_groupsMap.find(key);
        if (it != m_groupsMap.end())
        {
            return it->second;
        }

        Ptr<AtsSchedulerGroup> newGroup = CreateObject<AtsSchedulerGroup>();
        newGroup->SetAts(this);
        if (outputDevice)
        {
            newGroup->SetNetDevice(outputDevice);
        }

        m_groupsMap[key] = newGroup;
        return newGroup;
    }

    bool
    Ats::EnqueueFrame(Ptr<Packet> packet, uint32_t streamHandle,
                      uint32_t inputPortId, uint32_t outputPortId,
                      uint8_t priority, Ptr<TsnNetDevice> outputDevice,
                      Time hardwareLatency)
    {
        NS_LOG_FUNCTION(this << packet << streamHandle << inputPortId << outputPortId << (uint32_t)priority);

        AtsGroupKey key;
        key.inputPortId = inputPortId;
        key.outputPortId = outputPortId;

        if (key.inputPortId == LOCAL_INPUT_PORT)
        {
            // End-station (one group for each stream)
            key.internalId = streamHandle;
        }
        else
        {
            // Bridge
            key.internalId = priority;
        }

        Ptr<AtsSchedulerGroup> targetGroup = GetGroup(key, outputDevice);

        return targetGroup->ProcessFrame(packet, streamHandle, hardwareLatency);
    }

}