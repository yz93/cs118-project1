/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014,  Regents of the University of California
 *
 * This file is part of Simple BT.
 * See AUTHORS.md for complete list of Simple BT authors and contributors.
 *
 * NSL is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NSL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NSL, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * \author Yingdi Yu <yingdi@cs.ucla.edu>
 */

#ifndef SBT_CLIENT_HPP
#define SBT_CLIENT_HPP

#include "common.hpp"
#include "tracker-response.hpp"
#include "peerConnection.hpp"
#include "msg/msg-base.hpp"
#include <vector>
#include "meta-info.hpp"
#include <unordered_map>
//using namespace std;

namespace sbt {

class Client
{
public:
  class Error : public std::runtime_error
  {
  public:
    explicit
    Error(const std::string& what)
      : std::runtime_error(what)
    {
    }
  };

public:
  Client(const std::string& port,
         const std::string& torrent);

  void
  run();

  const std::string&
  getTrackerHost() {
    return m_trackerHost;
  }

  const std::string&
  getTrackerPort() {
    return m_trackerPort;
  }

  const std::string&
  getTrackerFile() {
    return m_trackerFile;
  }

private:
  void
  loadMetaInfo(const std::string& torrent);

  void
  connectTracker();

  void
  sendTrackerRequest();

  void
  recvTrackerResponse();

  int downloadAndUpload();

  void sendHandshake(const int& fd);

  void sendBitfield(const int& fd);

  void receiveBitfield();

  void sendInterested(const int& fd);

  void sendUnchoke(const int& fd);

  void sendRequest(const int& fd);

  void sendPiece(const int& fd, const int& index, const int& offset, const int& length);

  void sendHave(const int& fd, const int& index);
  
  void connectPeers();

  void vectorToBitfield(const std::vector<uint8_t>& bitFieldVec, char* cPtr);

  std::vector<int> bitfieldToVector(ConstBufferPtr bitfield);
  
  bool checkPieceHash(const msg::Piece& piece);

  //void sendPeerRequest();

  //void recvPeerResponse();

private:
  MetaInfo m_metaInfo;
  std::string m_id;
  std::vector<PeerInfo> m_peers;
  std::vector<std::string> m_peerIdList;  // also connection list, by peer id
  std::vector<int> m_client_socketFd;
  std::vector<uint8_t> m_bitfield;
  std::vector<std::string> m_hashPieces;
  std::vector<bool> m_requestSent;
  std::unordered_map<int, PeerConnection> m_peerConnections;  // connection list, by fd
  int m_fileLen;
  int m_pieceLen;
  int m_numPieces;
  int m_numBytes;
  int m_uploaded;
  int m_downloaded;
  int m_left;


  std::string m_trackerHost;
  std::string m_trackerPort;
  std::string m_trackerFile;

  uint16_t m_clientPort;

  int m_trackerSock;
  int m_serverSock = -1;

  fd_set m_readSocks;

  uint64_t m_interval;
  bool m_isFirstReq;
  bool m_isFirstRes;

};

} // namespace sbt

#endif // SBT_CLIENT_HPP
