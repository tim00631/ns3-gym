#ifndef TDMA_RL_ENV_H
#define TDMA_RL_ENV_H


#include "ns3/opengym-module.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/core-module.h"
#include "ns3/node-list.h"
#include "ns3/network-module.h"
#include "ns3/log.h"
#include "ns3/olsr-routing-protocol.h"
#include "ns3/simple-wireless-tdma-module.h"

#include <vector>

namespace ns3 {

class Node;
class Packet;

class TdmaGymEnv : public OpenGymEnv
{
public:
  TdmaGymEnv ();
  TdmaGymEnv (Time stepInterval1,Time stepInterval2);
  virtual ~TdmaGymEnv ();
  static TypeId GetTypeId (void);
  virtual void DoDispose ();


  // OpenGym interface
  Ptr<OpenGymSpace> GetActionSpace();
  Ptr<OpenGymSpace> GetObservationSpace();
  bool GetGameOver();
  Ptr<OpenGymDataContainer> GetObservation();
  float GetReward();
  std::string GetExtraInfo();
  bool ExecuteActions(Ptr<OpenGymDataContainer> action);



private:
  void ScheduleNextStateRead ();

  uint32_t m_slotNum;
  std::map<Ipv4Address,uint32_t> m_ip2id; // Convert ipv4 to nodeId 
  Time m_stepInterval1; // skip to next ctrl slot (ctrl slot size)
  Time m_stepInterval2; // skip all data slot ( data slot num * data slot size)
};




} // namespace ns3

#endif /* TDMA_RL_ENV_H */
