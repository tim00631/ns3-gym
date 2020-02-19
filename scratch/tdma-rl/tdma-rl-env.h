#ifndef TDMA_RL_ENV_H
#define TDMA_RL_ENV_H


#include "ns3/opengym-module.h"
#include <vector>

namespace ns3 {


class Packet;

class TdmaGymEnv : public OpenGymEnv
{
public:
  TdmaGymEnv ();
  virtual ~TdmaGymEnv ();
  static TypeId GetTypeId (void);
  virtual void DoDispose ();


  // OpenGym interface
  Ptr<OpenGymSpace> GetActionSpace();
  bool GetGameOver();
  float GetReward();
  std::string GetExtraInfo();
  bool ExecuteActions(Ptr<OpenGymDataContainer> action);

  Ptr<OpenGymSpace> GetObservationSpace();
  Ptr<OpenGymDataContainer> GetObservation();


protected:

};




} // namespace ns3

#endif /* TDMA_RL_ENV_H */
