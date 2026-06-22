#include "ns3/ethernet-channel.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/tsn-module.h" // Modifie selon le nom exact de ton module TSN
#include "ns3/drop-tail-queue.h"
#include "ns3/ethernet-generator.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Active les logs de debug pour voir tes messages personnalisés
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // 1. Création des nœuds et des horloges
    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Ptr<TsnNode> n1 = CreateObject<TsnNode>();

    Ptr<Clock> clock0 = CreateObject<Clock>();
    Ptr<Clock> clock1 = CreateObject<Clock>();
    n0->SetMainClock(clock0);
    n1->SetMainClock(clock1);
    n0->AddClock(clock0);
    n1->AddClock(clock1);
    n0->setActiveClock(0);
    n1->setActiveClock(0);

    // 2. Configuration des NetDevices (1 Gbps)
    Ptr<TsnNetDevice> net0 = CreateObject<TsnNetDevice>();
    n0->AddDevice(net0);
    Ptr<TsnNetDevice> net1 = CreateObject<TsnNetDevice>();
    n1->AddDevice(net1);

    net0->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    net1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    Ptr<EthernetChannel> channel = CreateObject<EthernetChannel>();
    net0->Attach(channel);
    net1->Attach(channel);

    net0->SetAddress(Mac48Address::Allocate());
    net1->SetAddress(Mac48Address::Allocate());

    for (int i = 0; i < 8; i++)
    {
        net0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // 3. Activation et Configuration de l'ATS
    net0->SetAttribute("isAtsEnabled", BooleanValue(true));
    Ptr<Ats> ats = net0->GetAts();
    ats->SetClock(clock0);
    ats->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(1))); // 1ms maximum ceiling

    // 4. Générateur de Trafic (Burst Flash de 5 paquets)
    Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
    app0->Setup(net0);
    app0->SetAttribute("BurstSize", UintegerValue(5));
    app0->SetAttribute("PayloadSize", UintegerValue(500));
    app0->SetAttribute("Period", TimeValue(MicroSeconds(10)));
    app0->SetAttribute("VlanID", UintegerValue(1));
    app0->SetAttribute("PCP", UintegerValue(5));
    n0->AddApplication(app0);

    // Fenêtre d'exécution flash pour n'avoir qu'un seul burst
    app0->SetStartTime(MilliSeconds(10));
    app0->SetStopTime(MilliSeconds(10) + MicroSeconds(2));

    // 5. Simulation Execution
    NS_LOG_UNCOND("Starting ATS Debug Simulation...");
    Simulator::Stop(MilliSeconds(50));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_UNCOND("Simulation Finished.");

    return 0;
}