#include "ats.h"

#include "ns3/log.h"
#include "ns3/ats-scheduler-group.h"
#include "ns3/ethernet-header2.h"

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
    Ats::EnqueueFrame(Ptr<Packet> packet,
                      uint32_t inputPortId, uint32_t outputPortId,
                      uint8_t priority, Ptr<TsnNetDevice> outputDevice,
                      Time hardwareLatency)
    {
        NS_LOG_FUNCTION(this << packet << inputPortId << outputPortId << (uint32_t)priority);

        uint32_t internalId = 0;
        Ptr<AtsSchedulerGroup> targetGroup;

        if (inputPortId == LOCAL_INPUT_PORT)
        {
            // End Station
            Ptr<Packet> packetCopy = packet->Copy();
            EthernetHeader2 ethHeader;
            packetCopy->RemoveHeader(ethHeader);

            uint16_t vlanId = ethHeader.GetVid();
            Mac48Address destMac = ethHeader.GetDest();

            uint8_t buffer[6];
            destMac.CopyTo(buffer);
            uint32_t macHash = (buffer[2] << 24) | (buffer[3] << 16) | (buffer[4] << 8) | buffer[5];

            internalId = (static_cast<uint32_t>(vlanId) << 16) ^ macHash;
        }
        else
        {
            // Bridge
            internalId = static_cast<uint32_t>(priority);
        }

        targetGroup = GetGroup(inputPortId, outputPortId, internalId, outputDevice);

        return targetGroup->ProcessFrame(packet, hardwareLatency);
    }

}