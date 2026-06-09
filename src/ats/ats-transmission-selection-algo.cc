#include "ats-transmission-selection-algo.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("AtsTransmissionSelectionAlgo");
    NS_OBJECT_ENSURE_REGISTERED(AtsTransmissionSelectionAlgo);

    TypeId
    AtsTransmissionSelectionAlgo::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::AtsTransmissionSelectionAlgo")
                .SetParent<TsnTransmissionSelectionAlgo>()
                .SetGroupName("Tsn")
                .AddConstructor<AtsTransmissionSelectionAlgo>();
        return tid;
    }

    AtsTransmissionSelectionAlgo::AtsTransmissionSelectionAlgo()
    {
        NS_LOG_FUNCTION(this);
    }

    AtsTransmissionSelectionAlgo::~AtsTransmissionSelectionAlgo()
    {
        NS_LOG_FUNCTION(this);
    }

    void AtsTransmissionSelectionAlgo::SetAtsScheduler(Ptr<AtsScheduler> scheduler)
    {
        m_atsScheduler = scheduler;
    }

    bool AtsTransmissionSelectionAlgo::IsReadyToTransmit()
    {
        if (!m_atsScheduler || m_atsScheduler->IsQueueEmpty())
        {
            return false;
        }

        Ptr<Packet> topPacket = m_atsScheduler->PeekTopPacket();
        AtsEligibilityTimeTag tag;
        if (topPacket->FindFirstMatchingByteTag(tag))
        {
            return (Simulator::Now() >= tag.GetEligibilityTime());
        }
        return true;
    }

    void AtsTransmissionSelectionAlgo::TransmitStart(Ptr<Packet> p, Time txTime)
    {
        if (m_atsScheduler)
            m_atsScheduler->DequeueTopPacket();
    }

} // namespace ns3