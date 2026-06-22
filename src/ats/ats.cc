#include "ats.h"

#include "ns3/log.h"
#include "ns3/ats-scheduler-group.h"
#include "stream-id-tag.h"

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
                .AddConstructor<Ats>()
                .AddAttribute("MaxResidenceTime",
                              "Maximum residence time by default for each group.",
                              TimeValue(Seconds(1)),
                              MakeTimeAccessor(&Ats::m_defaultMaximumResidenceTime),
                              MakeTimeChecker());
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
    Ats::GetGroup(uint32_t inputPortId, uint32_t outputPortId, uint32_t internalId, Ptr<TsnNetDevice> outputDevice)
    {
        NS_LOG_FUNCTION(this << inputPortId << outputPortId << internalId);
        AtsGroupKey key;
        key.inputPortId = inputPortId;
        key.outputPortId = outputPortId;
        key.internalId = internalId;

        auto it = m_groupsMap.find(key);
        if (it != m_groupsMap.end())
        {
            return it->second;
        }

        Ptr<AtsSchedulerGroup> newGroup = CreateObject<AtsSchedulerGroup>();
        newGroup->SetAttribute("MaxResidenceTime", TimeValue(m_defaultMaximumResidenceTime));
        newGroup->SetAts(this);
        if (outputDevice)
        {
            newGroup->SetNetDevice(outputDevice);
        }

        m_groupsMap[key] = newGroup;
        return newGroup;
    }

    bool
    Ats::EnqueueFrame(Ptr<Packet> packet, uint32_t streamId,
                      uint32_t inputPortId, uint32_t outputPortId,
                      uint8_t priority, Ptr<TsnNetDevice> outputDevice,
                      Time hardwareLatency)
    {
        NS_LOG_FUNCTION(this << packet << streamId << inputPortId << outputPortId << (uint32_t)priority);

        StreamIdTag tag;
        if (packet->FindFirstMatchingByteTag(tag))
        {
            streamId = tag.GetStreamId(); // Retrieve the unique id of the application
        }

        Ptr<AtsSchedulerGroup> targetGroup;

        if (inputPortId == LOCAL_INPUT_PORT)
        {
            // End-station (one group for each stream)
            targetGroup = GetGroup(inputPortId, outputPortId, streamId, outputDevice);
        }
        else
        {
            // Bridge
            targetGroup = GetGroup(inputPortId, outputPortId, priority, outputDevice);
        }

        return targetGroup->ProcessFrame(packet, streamId, hardwareLatency);
    }

}