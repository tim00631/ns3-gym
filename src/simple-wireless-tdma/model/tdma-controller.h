/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Hemanth Narra
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Hemanth Narra <hemanthnarra222@gmail.com>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 */
#ifndef TDMA_CONTROLLER_H
#define TDMA_CONTROLLER_H

#include "ns3/nstime.h"
#include "ns3/event-id.h"
#include "ns3/data-rate.h"
#include "ns3/packet.h"
#include "ns3/simple-wireless-channel.h"
#include "ns3/timer.h"
#include <vector>
#include <map>
#include <algorithm>

namespace ns3 {

enum TdmaMode
{
  CENTRALIZED = 1,
};

class TdmaMac;
class TdmaMacLow;
class SimpleWirelessChannel;
class TdmaNetDevice;

class TdmaController : public Object
{
public:
  static TypeId GetTypeId (void);
  TdmaController ();
  ~TdmaController ();

  //typedef std::map<uint32_t, Ptr<TdmaMac> > TdmaMacPtrMap;
  typedef std::map<uint32_t,std::vector< Ptr<TdmaMac> > > TdmaMacPtrMap;

  /**
   * \param slotTime the duration of a slot.
   *
   * It is a bad idea to call this method after RequestAccess or
   * one of the Notify methods has been invoked.
   */
  void SetSlotTime (Time slotTime);
  /**
   */
  void SetCtrlSlotTime (Time slotTime);
  /**
   */
  void SetGuardTime (Time guardTime);
  /**
   */
  void SetDataRate (DataRate bps);
  /**
   */
  void AddTdmaSlot (uint32_t slot, Ptr<TdmaMac> macPtr, uint32_t nodeId);
  /**
   */
  void SetInterFrameTimeInterval (Time interFrameTime);
  /**
   */
  void SetTotalSlotsAllowed (uint32_t slotsAllowed);
  /**
   */
  Time GetSlotTime (void) const;
  /**
   */
  Time GetCtrlSlotTime (void) const;
  /**
   */
  Time GetGuardTime (void) const;
  /**
   */
  DataRate GetDataRate (void) const;
  /**
   */
  Time GetInterFrameTimeInterval (void) const;
  /**
   */
  uint32_t GetTotalSlotsAllowed (void) const;
  /**
   * \param duration expected duration of reception
   *
   * Notify the DCF that a packet reception started
   * for the expected duration.
   */
  void NotifyRxStartNow (Time duration);
  /**
   * Notify the DCF that a packet reception was just
   * completed successfully.
   */
  void NotifyRxEndOkNow (void);
  /**
   * Notify the DCF that a packet reception was just
   * completed unsuccessfully.
   */
  void NotifyRxEndErrorNow (void);
  /**
   * \param duration expected duration of transmission
   *
   * Notify the DCF that a packet transmission was
   * just started and is expected to last for the specified
   * duration.
   */
  void NotifyTxStartNow (Time duration);
  Time CalculateTxTime (Ptr<const Packet> packet);
  void StartTdmaSessions (void);
  void SetChannel (Ptr<SimpleWirelessChannel> c);
  virtual void Start (void);

  void AddNetDevice (uint32_t nodeId,Ptr<TdmaNetDevice> device);
  void SetNodeNum (uint32_t);
  uint32_t GetnNodes();
  void UpdateList (std::string s, uint32_t NodeId);
  void DeleteTdmaSlot (uint32_t slot, Ptr<TdmaMac> macPtr);
  std::vector<std::pair<uint32_t,uint32_t> > GetNodeUsedList (uint32_t NodeId);

  void AddNonDataBytes (uint64_t bytes);
  uint64_t GetNonDataBytes (void);
  void AddTotalBytes (uint64_t bytes);
  uint64_t GetTotalBytes (void);
  void AddDataSuccessfulBytes (uint64_t bytes);
  uint64_t GetDataSuccessfulBytes (void);
  void AddDataBytes (uint64_t bytes);
  uint64_t GetDataBytes (void);
  void SetPacketType (uint64_t uid, bool type);
  bool GetPacketType (uint64_t uid);
  void DeletePacketType (uint64_t uid);
  void SetPacketStart (uint64_t uid, int64_t time);
  void DeletePacketStart (uint64_t uid, int64_t time);
  int64_t GetPacketStart (uint64_t uid);
  void AddDelay (int64_t delay);
  int64_t GetDelay (void);
  void AddpktCount (void);
  uint64_t GetpktCount (void);

  void AddRemainSlotTime (uint64_t time);
  uint64_t GetRemainSlotTime (void);
  std::vector<std::pair<Ipv4Address, uint32_t> > GetTop3QueuePktStatus (uint32_t slotNum);

private:
  static Time GetDefaultSlotTime (void);
  static Time GetDefaultCtrlSlotTime (void);
  static Time GetDefaultEntrySlotTime (void);
  static Time GetDefaultGuardTime (void);
  static DataRate GetDefaultDataRate (void);
  void DoRestartAccessTimeoutIfNeeded (void);
  void AccessTimeout (void);
  void DoGrantAccess (void);
  bool IsBusy (void) const;
  void UpdateFrameLength (void);
  void ScheduleTdmaSession (const uint32_t slotNum);
  void SendUsed (Ptr<TdmaNetDevice> device);
  void ShiftCtrlSlot(void);
  void ShiftCtrlSlotMap(void);
  uint32_t GetCtrlNode (uint32_t slotNum);
  uint32_t GetCtrlSlot (uint32_t nodeId);
  void RotateUsedList (void);
  void ClearTdmaDataSlot(void);

  Ptr<SimpleWirelessChannel> GetChannel (void) const;

//  Time m_lastRxStart;
//  Time m_lastRxDuration;
//  bool m_lastRxReceivedOk;
//  Time m_lastRxEnd;
//  Time m_lastTxStart;
//  Time m_lastTxDuration;
//  EventId m_accessTimeout;
  DataRate m_bps;
  uint32_t m_slotTime;
  uint32_t m_ctrlslotTime;
  uint32_t m_guardTime;
  uint32_t m_tdmaFrameLength; //total frameLength in microseconds
  uint32_t m_tdmaInterFrameTime;
  uint32_t m_totalSlotsAllowed;
  bool m_activeEpoch;
  TdmaMode m_tdmaMode;
  TdmaMacPtrMap m_slotPtrs;
  Ptr<SimpleWirelessChannel> m_channel;

  uint32_t m_nNodes;
  uint32_t m_tdmaCtrlSlotMap[64];
  uint32_t m_tdmaCtrlSlotMapRev[64];
  std::map<uint32_t,Ptr<TdmaNetDevice> > m_tdmaDeviceList;  // (NodeId,(device,IsEntry))
  std::map<Ptr<TdmaMac>,uint32_t> m_mac2Id; // use TdmaMac to get nodeId
  std::map<uint32_t,Ptr<TdmaMac>> m_id2mac; // use nodeId to get TdmaMac
  std::pair<uint32_t,uint32_t> m_tdmaUsedListCur[64][100]; // store each node's Data slot Used list (current frame) [nodeId][slotNum].(priority,nodeId)
  std::pair<uint32_t,uint32_t> m_tdmaUsedListPre[64][100]; // store each node's Data slot Used list (previous frame) [nodeId][slotNum].(priority,nodeId)
  int32_t m_tdmaEntrySlotNum[64];

  uint64_t m_tdmaNonDataBytes;
  uint64_t m_tdmaTotalBytes;
  uint64_t m_tdmaDataSuccessfulBytes;
  uint64_t m_tdmaDataBytes;
  std::map<uint64_t,bool> m_tdmaPacketTypeList;
  std::map<uint64_t,int64_t> m_tdmaPacketStartList;
  int64_t m_tdmaDelay;
  uint64_t m_pktCount;

  uint64_t m_tdmaRemainSlotTime;  
};

} // namespace ns3

#endif /* AERO_TDMA_CONTROLLER_H */
