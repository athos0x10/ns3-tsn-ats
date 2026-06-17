#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/tsn-module.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ethernet-channel.h"
#include "ns3/ethernet-generator.h"
#include "ns3/switch-net-device.h" // Indispensable pour le switch

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AtsScratchDebug");

void PacketTxCallback(Ptr<const Packet> packet)
{
    NS_LOG_INFO("[APP TX] Paquet généré à t = " << Simulator::Now().GetMilliSeconds()
                                                << " ms | Taille = " << packet->GetSize() << " octets");
}

void PacketRxCallback(Ptr<const Packet> packet)
{
    NS_LOG_INFO("[NET RX] Paquet REÇU sur le nœud final à t = " << Simulator::Now().GetMilliSeconds()
                                                                << " ms | Taille = " << packet->GetSize() << " octets");
}

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // 1. Activation complète des logs ATS
    LogComponentEnable("AtsScratchDebug", LOG_LEVEL_INFO);
    LogComponentEnable("AtsSchedulerGroup", LOG_LEVEL_DEBUG);
    LogComponentEnable("AtsSchedulerInstance", LOG_LEVEL_DEBUG);

    NS_LOG_INFO("--- Configuration Topologie Commutée (Switch) avec ATS ---");

    // 2. Création des 3 Nœuds (Source, Switch, Destination)
    Ptr<TsnNode> n0 = CreateObject<TsnNode>();
    Ptr<TsnNode> switchNode = CreateObject<TsnNode>(); // Le commutateur intermédiaire
    Ptr<TsnNode> n1 = CreateObject<TsnNode>();

    n0->AddClock(CreateObject<Clock>());
    switchNode->AddClock(CreateObject<Clock>());
    n1->AddClock(CreateObject<Clock>());

    // 3. Configuration du SwitchNetDevice central
    Ptr<SwitchNetDevice> switchDevice = CreateObject<SwitchNetDevice>();
    switchNode->AddDevice(switchDevice);

    // 4. LIEN UNIQUE 1 : Entre Source (n0) et Switch (Port 0)
    Ptr<TsnNetDevice> net0 = CreateObject<TsnNetDevice>();
    n0->AddDevice(net0);
    Ptr<TsnNetDevice> switchPort0 = CreateObject<TsnNetDevice>();
    switchNode->AddDevice(switchPort0);
    switchDevice->AddSwitchPort(switchPort0); // Enregistrement du port dans le pont L2

    Ptr<EthernetChannel> channel1 = CreateObject<EthernetChannel>();
    net0->Attach(channel1);
    switchPort0->Attach(channel1);

    // 5. LIEN UNIQUE 2 : Entre Switch (Port 1) et Destination (n1)
    Ptr<TsnNetDevice> switchPort1 = CreateObject<TsnNetDevice>();
    switchNode->AddDevice(switchPort1);
    switchDevice->AddSwitchPort(switchPort1);

    Ptr<TsnNetDevice> net1 = CreateObject<TsnNetDevice>();
    n1->AddDevice(net1);

    Ptr<EthernetChannel> channel2 = CreateObject<EthernetChannel>();
    switchPort1->Attach(channel2);
    net1->Attach(channel2);

    // Configuration des adresses MAC et files de base
    net0->SetAddress(Mac48Address::Allocate());
    switchPort0->SetAddress(Mac48Address::Allocate());
    switchPort1->SetAddress(Mac48Address::Allocate());
    net1->SetAddress(Mac48Address::Allocate());

    for (int i = 0; i < 8; i++)
    {
        net0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        switchPort0->SetQueue(CreateObject<DropTailQueue<Packet>>());
        switchPort1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        net1->SetQueue(CreateObject<DropTailQueue<Packet>>());
    }

    // Débits physiques élevés pour isoler le Shaper
    net0->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    switchPort0->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    switchPort1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    net1->SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));

    // 6. ACTIVATION DE L'ATS SUR LE PORT D'ENTRÉE DU SWITCH (switchPort0)
    uint8_t targetPcp = 5;
    DataRate cir = DataRate("100Kbps");
    uint32_t cbs = 16000;

    switchPort0->SetAttribute("IsAtsEnabled", BooleanValue(true));
    Ptr<AtsSchedulerGroup> atsGroup = switchPort0->GetAtsSchedulerGroup();

    // Limite de rétention critique à 150 ms
    atsGroup->SetAttribute("MaxResidenceTime", TimeValue(MilliSeconds(150)));
    atsGroup->SetPerPriorityRouting(true);
    atsGroup->CreateAtsInstanceForPriority(cir, cbs, targetPcp);

    // 7. Générateur de Trafic (Envoi de rafales)
    Ptr<EthernetGenerator> app0 = CreateObject<EthernetGenerator>();
    app0->Setup(net0);
    app0->SetAttribute("BurstSize", UintegerValue(5));
    app0->SetAttribute("PayloadSize", UintegerValue(478));
    app0->SetAttribute("Period", TimeValue(MilliSeconds(1)));
    app0->SetAttribute("VlanID", UintegerValue(1));
    app0->SetAttribute("PCP", UintegerValue(targetPcp));
    n0->AddApplication(app0);

    app0->SetStartTime(MilliSeconds(10));
    app0->SetStopTime(MilliSeconds(20)); // Émettra à 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 ms

    // Traces
    net0->TraceConnectWithoutContext("MacTx", MakeCallback(&PacketTxCallback));
    net1->TraceConnectWithoutContext("MacRx", MakeCallback(&PacketRxCallback));

    // 8. Run
    NS_LOG_INFO("Lancement de la simulation avec commutateur...");
    Simulator::Stop(Seconds(2.0));
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("--- Fin de la simulation ---");
    return 0;
}