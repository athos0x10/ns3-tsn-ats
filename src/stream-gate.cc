#include "stream-gate.h"

#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/simulator.h"


namespace ns3
{

NS_LOG_COMPONENT_DEFINE("StreamGate");

NS_OBJECT_ENSURE_REGISTERED(StreamGate);

TypeId
StreamGate::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::StreamGate")
            .SetParent<Object>()
            .SetGroupName("Tsn")
            .AddConstructor<StreamGate>()
            .AddAttribute("IpvEnable",
                          "IpvEnable function activation boolean",
                          BooleanValue(false),
                          MakeBooleanAccessor(&StreamGate::m_ipvEnable),
                          MakeBooleanChecker())
            .AddAttribute("Ipv",
                          "Internal Priority Value (must be between 0 and 7)",
                          UintegerValue(0),
                          MakeUintegerAccessor(&StreamGate::m_ipv),
                          MakeUintegerChecker<uint8_t> (0 ,7))
                          
                          ;

    return tid;
}

StreamGate::StreamGate()
{
  NS_LOG_FUNCTION(this);
  m_state = OPEN;
}

StreamGate::~StreamGate()
{
    NS_LOG_FUNCTION(this);
}

bool
StreamGate::IsOpen()
{
  NS_LOG_FUNCTION(this);
  if(m_state == OPEN)
  {
    return true;
  }
  else
  {
    return false;
  }
}

void
StreamGate::Open()
{
    m_state = OPEN;
}

void
StreamGate::Close()
{
    m_state = CLOSE;
}

uint8_t
StreamGate::GetIpvId()
{
    return m_ipv;
}

void
StreamGate::SetGateAndIpv(State gateState, uint8_t ipvId, Time startTime)
{
  NS_LOG_FUNCTION(this);
  // Schedule the change at the time we want
  if (ipvId <=7) {
    Simulator::Schedule(startTime, &StreamGate::handleSetGateAndIpv, this, gateState, ipvId);
  } else {
    NS_LOG_INFO("Impossible to change, Ipv must be between 0 and 7");
  }
}

void
StreamGate::handleSetGateAndIpv(State gateState, uint8_t ipvId)
{ 
  NS_LOG_FUNCTION(this);
  // We change both values only if ipvId is in the good range
  m_ipv = ipvId;
  m_state = gateState;
  NS_LOG_INFO("Gate state and IPV changed to: " << gateState << ipvId << " at " << Simulator::Now());
}



};
