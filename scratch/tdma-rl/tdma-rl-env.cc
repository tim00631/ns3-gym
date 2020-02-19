#include "tdma-rl-env.h"
#include "ns3/object.h"
#include "ns3/core-module.h"
#include "ns3/node-list.h"
#include "ns3/log.h"
#include <sstream>
#include <iostream>

#include <iostream>

namespace ns3 {


NS_LOG_COMPONENT_DEFINE ("TdmaGymEnv");

NS_OBJECT_ENSURE_REGISTERED (TdmaGymEnv);


TdmaGymEnv::TdmaGymEnv ()
{


}

TdmaGymEnv::~TdmaGymEnv ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
TdmaGymEnv::GetTypeId (void)
{
  static TypeId tid = TypeId ("TdmaGymEnv")
    .SetParent<OpenGymEnv> ()
    .SetGroupName ("OpenGym")
    .AddConstructor<TdmaGymEnv> ()
  ;
  return tid;
}

void
TdmaGymEnv::DoDispose ()
{
  NS_LOG_FUNCTION (this);
}

/*
Define action space
*/
Ptr<OpenGymSpace>
TdmaGymEnv::GetActionSpace()
{
  // output : Data slot number
  uint32_t max_slots = 3; // the max slots the node could choose

  float low = 0;
  float high = 99;
  std::vector<uint32_t> shape = {max_slots,};
  std::string dtype = TypeNameGet<uint32_t> ();

  Ptr<OpenGymBoxSpace> box = CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);
  NS_LOG_INFO ("TdmaGetActionSpace: "<<box);
  return box;
}

/*
Define observation space
*/
Ptr<OpenGymSpace>
TdmaGymEnv::GetObservationSpace()
{
  // input :
  // Slot Used Table (size : 100 slots)
  // tdma queue top 3 packet percentage 
  uint32_t dataSlotNum = 100;

  float low = -1;
  float high = 3;
  std::vector<uint32_t> shape = {dataSlotNum, };
  std::string dtype = TypeNameGet<int32_t> ();

  Ptr<OpenGymBoxSpace> slotUsedTable_box = CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);

  uint32_t topNkinds = 3;

  float low_2 = 0;
  float high_2 = 100;
  std::vector<uint32_t> shape_2 = {topNkinds, };
  std::string dtype_2 = TypeNameGet<uint32_t> ();

  Ptr<OpenGymBoxSpace> percentage_box = CreateObject<OpenGymBoxSpace> (low_2, high_2, shape_2, dtype_2);

  Ptr<OpenGymDictSpace> space = CreateObject<OpenGymDictSpace> ();
  space->Add("slotUsedTable", slotUsedTable_box);
  space->Add("percentage", percentage_box);
 

  NS_LOG_INFO ("TdmaGetObservationSpace"<<space);

  return space;

}


/*
Define game over condition
*/
bool
TdmaGymEnv::GetGameOver()
{
  // Collision more than 3 times
  
  bool isGameOver = false;
  return isGameOver;
}


/*
Collect observations
*/
Ptr<OpenGymDataContainer>
TdmaGymEnv::GetObservation()
{
  Ptr<OpenGymTupleContainer> data = CreateObject<OpenGymTupleContainer> ();
  return data;
}

/*
Define reward function
*/
float
TdmaGymEnv::GetReward()
{
  static float reward = 0.0;
  return reward;

}

/*
Define extra info. Optional
*/
std::string
TdmaGymEnv::GetExtraInfo()
{
  std::string Info = "testInfo";
  NS_LOG_UNCOND("MyGetExtraInfo: " << Info);
  return Info;
}


/*
Execute received actions
*/
bool
TdmaGymEnv::ExecuteActions(Ptr<OpenGymDataContainer> action)
{
  return true;
}


} // ns3 namespace


