/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 CTTC
 * Copyright (c) 2010 TELEMATICS LAB, DEE - Politecnico di Bari
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
 * Authors: 
 *   Nicola Baldo <nbaldo@cttc.es> (the EpsTftClassifier class)
 *   Giuseppe Piro <g.piro@poliba.it> (part of the code in EpsTftClassifier::Classify () 
 *       which comes from RrcEntity::Classify of the GSoC 2010 LTE module)
 *
 */




#include "eps-tft-classifier.h"
#include "lte-tft.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/udp-header.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-l4-protocol.h"
#include "ns3/tcp-l4-protocol.h"

NS_LOG_COMPONENT_DEFINE ("EpsTftClassifier");

namespace ns3 {

EpsTftClassifier::EpsTftClassifier ()
{
  NS_LOG_FUNCTION (this);
}

void
EpsTftClassifier::Add (Ptr<LteTft> tft, uint32_t id)
{
  NS_LOG_FUNCTION (this << tft);
  
  m_tftMap[id] = tft;  
  
  // simple sanity check: there shouldn't be more than 16 bearers (hence TFTs) per UE
  NS_ASSERT (m_tftMap.size () <= 16);
}

void
EpsTftClassifier::Delete (uint32_t id)
{
  NS_LOG_FUNCTION (this << id);
  m_tftMap.erase (id);
}

 
uint32_t 
EpsTftClassifier::Classify (Ptr<Packet> p, LteTft::Direction direction)
{
  NS_LOG_FUNCTION (this << p << direction);

  Ptr<Packet> pCopy = p->Copy ();

  Ipv4Header ipv4Header;
  pCopy->RemoveHeader (ipv4Header);

  Ipv4Address localAddress;
  Ipv4Address remoteAddress;

  
  if (direction ==  LteTft::UPLINK)
    {
      localAddress = ipv4Header.GetSource ();
      remoteAddress = ipv4Header.GetDestination ();
    }
  else
    { 
      NS_ASSERT (direction ==  LteTft::DOWNLINK);
      remoteAddress = ipv4Header.GetSource ();
      localAddress = ipv4Header.GetDestination ();      
    }
  
  uint8_t protocol = ipv4Header.GetProtocol ();

  uint8_t tos = ipv4Header.GetTos ();

  uint16_t localPort = 0;
  uint16_t remotePort = 0;

  if (protocol == UdpL4Protocol::PROT_NUMBER)
    {
      UdpHeader udpHeader;
      pCopy->RemoveHeader (udpHeader);

      if (direction ==  LteTft::UPLINK)
	{
	  localPort = udpHeader.GetSourcePort ();
	  remotePort = udpHeader.GetDestinationPort ();
	}
      else
	{
	  remotePort = udpHeader.GetSourcePort ();
	  localPort = udpHeader.GetDestinationPort ();
	}
    }
  else if (protocol == TcpL4Protocol::PROT_NUMBER)
    {
      TcpHeader tcpHeader;
      pCopy->RemoveHeader (tcpHeader);
      if (direction ==  LteTft::UPLINK)
	{
	  localPort = tcpHeader.GetSourcePort ();
	  remotePort = tcpHeader.GetDestinationPort ();
	}
      else
	{
	  remotePort = tcpHeader.GetSourcePort ();
	  localPort = tcpHeader.GetDestinationPort ();
	}
    }
  else
    {
      NS_LOG_INFO ("Unknown protocol: " << protocol);
      return 0;  // no match
    }

  NS_LOG_INFO ("Classifing packet:"
	       << " localAddr="  << localAddress 
	       << " remoteAddr=" << remoteAddress 
	       << " localPort="  << localPort 
	       << " remotePort=" << remotePort 
	       << " tos=0x" << (uint16_t) tos );

  // now it is possible to classify the packet!
  // we use a reverse iterator since filter priority is not implemented properly.
  // This way, since the default bearer is expected to be added first, it will be evaluated last.
  std::map <uint32_t, Ptr<LteTft> >::const_reverse_iterator it;
  NS_LOG_LOGIC ("TFT MAP size: " << m_tftMap.size ());

  for (it = m_tftMap.rbegin (); it != m_tftMap.rend (); ++it)
    {
      NS_LOG_LOGIC ("TFT id: " << it->first );
      NS_LOG_LOGIC (" Ptr<LteTft>: " << it->second);
      Ptr<LteTft> tft = it->second;         
      if (tft->Matches (direction, remoteAddress, localAddress, remotePort, localPort, tos))
        {
	  NS_LOG_LOGIC ("matches with TFT ID = " << it->first);
	  return it->first; // the id of the matching TFT
        }
    }
  NS_LOG_LOGIC ("no match");
  return 0;  // no match
}


} // namespace ns3