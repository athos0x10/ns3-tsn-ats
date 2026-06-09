#ifndef ATS_TRANSMISSION_SELECTION_ALGO_H
#define ATS_TRANSMISSION_SELECTION_ALGO_H

#include "ns3/tsn-transmission-selection-algo.h"
#include "ns3/ats-scheduler.h"

namespace ns3
{
    class AtsTransmissionSelectionAlgo : public TsnTransmissionSelectionAlgo
    {
    public:
        /**
         * \brief Get the TypeId.
         *
         * \return The TypeId of this class.
         */
        static TypeId GetTypeId();

        /**
         * \brief Create an AtsTransmissionSelectionAlgo.
         */
        AtsTransmissionSelectionAlgo();

        /**
         * \brief Destroy an AtsTransmissionSelectionAlgo.
         */
        ~AtsTransmissionSelectionAlgo() override;

        void SetAtsScheduler(Ptr<AtsScheduler> scheduler);
        bool IsReadyToTransmit() override;
        void TransmitStart(Ptr<Packet> p, Time txTime) override;

    private:
        Ptr<AtsScheduler> m_atsScheduler;
    };
} // namespace ns3
#endif // ATS_TRANSMISSION_SELECTION_ALGO_H