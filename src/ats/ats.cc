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

    Ptr<AtsSchedulerGroup> Ats::GetGroupForEndStation(Mac48Address destMac, uint16_t vlanId, Ptr<TsnNetDevice> egressDevice)
    {
        NS_LOG_FUNCTION(this << destMac << vlanId << egressDevice);
        NS_ASSERT_MSG(egressDevice != nullptr, "Ats::GetGroupForEndStation: Egress device pointer cannot be null.");

        // Compute the internalId
        uint8_t buffer[6];
        destMac.CopyTo(buffer);
        uint32_t macHash = (buffer[2] << 24) | (buffer[3] << 16) | (buffer[4] << 8) | buffer[5];
        uint32_t internalId = (static_cast<uint32_t>(vlanId) << 16) ^ macHash;

        uint32_t outputPortId = egressDevice->GetIfIndex();

        return Ats::GetGroup(Ats::LOCAL_INPUT_PORT, outputPortId, internalId, egressDevice);
    }

    Ptr<AtsSchedulerGroup> Ats::GetGroupForBridge(Ptr<TsnNetDevice> ingressDevice, Ptr<TsnNetDevice> egressDevice, uint8_t priority)
    {
        NS_LOG_FUNCTION(this << ingressDevice << egressDevice << priority);
        NS_ASSERT_MSG(ingressDevice != nullptr, "Ats::GetGroupForBridge: Ingress device pointer cannot be null.");
        NS_ASSERT_MSG(egressDevice != nullptr, "Ats::GetGroupForBridge: Egress device pointer cannot be null.");

        uint32_t inputPortId = ingressDevice->GetIfIndex();
        uint32_t outputPortId = egressDevice->GetIfIndex();
        uint32_t internalId = static_cast<uint32_t>(priority);

        return Ats::GetGroup(inputPortId, outputPortId, internalId, egressDevice);
    }

    bool
    Ats::EnqueueFrame(Ptr<Packet> packet,
                      uint32_t inputPortId, uint32_t outputPortId,
                      uint8_t priority, Ptr<TsnNetDevice> outputDevice,
                      Time hardwareLatency, uint32_t streamHandle)
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
            streamHandle = internalId;
        }
        else
        {
            // Bridge
            internalId = static_cast<uint32_t>(priority);
        }

        targetGroup = GetGroup(inputPortId, outputPortId, internalId, outputDevice);

        return targetGroup->ProcessFrame(packet, hardwareLatency, streamHandle);
    }

    bool
    Ats::IsPriorityActivated(uint8_t priority) const
    {
        auto it = m_priorityActivationMap.find(priority);
        if (it != m_priorityActivationMap.end())
        {
            return it->second;
        }
        return false; // ATS is disabled by default
    }

    void
    Ats::SetPriorityActivation(uint8_t priority, bool activated)
    {
        NS_LOG_FUNCTION(this << (uint32_t)priority << activated);
        m_priorityActivationMap[priority] = activated;
    }

}