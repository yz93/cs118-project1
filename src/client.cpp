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

#include "client.hpp"
#include "util/buffer.hpp"
#include "msg/handshake.hpp"
#include "util/hash.hpp"
//#include "msg/handshake.cpp"
#include "msg/msg-base.hpp"
#include "tracker-request-param.hpp"
#include "tracker-response.hpp"
#include "http/http-request.hpp"
#include "http/http-response.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sstream>


namespace sbt {

Client::Client(const std::string& port, const std::string& torrent)
  : m_id("SIMPLEBT.TEST.PEERID")
  , m_interval(3600)
  , m_uploaded(0)
  , m_downloaded(0)
  , m_isFirstReq(true)
  , m_isFirstRes(true)
{
  srand(time(NULL));

  m_peerIdList.push_back(m_id);

  int fileLen = m_metaInfo.getLength();
  m_left = fileLen;

  m_clientPort = boost::lexical_cast<uint16_t>(port);

  loadMetaInfo(torrent);

  m_fileLen = m_metaInfo.getLength();
  m_pieceLen = m_metaInfo.getPieceLength();

  if (m_fileLen % m_pieceLen == 0)
	  m_numPieces = m_fileLen / m_pieceLen;
  else
	  m_numPieces = (m_fileLen / m_pieceLen) + 1;

  if (m_numPieces % 8 == 0)
	  m_numBytes = m_numPieces / 8;
  else
	  m_numBytes = (m_numPieces / 8) + 1;

  std::ifstream in(m_metaInfo.getName());
  if (in){
	  for (int i = 0; i < m_numPieces; ++i)
		  m_bitfield.push_back(1);
	  /*std::ofstream out(m_metaInfo.getName());
	  out.close();*/
  }
  else
	  for (int i = 0; i < m_numPieces; ++i)
		  m_bitfield.push_back(0);

  // build a vector of hash strings with index starting at 0.
  // used after downloading a piece to check against the computed hash of that piece
  std::vector<uint8_t> tempVec = m_metaInfo.getPieces();
  for (int i = 0; i < tempVec.size();)
	  for (int j = 0; j < 20; ++j)
	  {
		  std::string str = "";
		  char c = (char)tempVec[i];
		  str += c;
		  m_hashPieces.push_back(str);
		  ++i;
	  }

  for (int i = 0; i < m_numPieces; ++i)
  {
	  m_requestSent.push_back(false);
  }

  run();
}

void
Client::run()
{
	while (true) {
	connectTracker();
	sendTrackerRequest();
	m_isFirstReq = false;
	recvTrackerResponse();
    // now m_peers have a list of peers that have my requested file
	connectPeers();
	downloadAndUpload();
	//sendPeerRequest();
	//download();
    close(m_trackerSock);
    //sleep(m_interval);
  }
}

int Client::downloadAndUpload()
{
	int maxSockfd = 0;

	fd_set readFds;
	fd_set tmpFds;
	FD_ZERO(&readFds);
	FD_ZERO(&tmpFds);

	// create a socket using TCP IP
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	maxSockfd = sockfd;

	// put the socket in the socket set
	FD_SET(sockfd, &tmpFds);

	// allow others to reuse the address
	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		return 1;
	}

	// bind address to socket
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(m_clientPort);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));
	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("bind");
		return 2;
	}

	// set the socket in listen status
	if (listen(sockfd, 10) == -1) {
		perror("listen");
		return 3;
	}

	// initialize timer
	struct timeval tv;
	tv.tv_sec = m_interval;
	tv.tv_usec = 0;
	uint64_t expectedTimeOut = time(nullptr) + m_interval;  // time() gets current time

	while (true) {
		readFds = tmpFds;

		/*get current time; update timer*/
		if (expectedTimeOut - time(nullptr) < 0)
		{
			connectTracker();
			sendTrackerRequest();
			m_isFirstReq = false;
			recvTrackerResponse();
			connectPeers();
			expectedTimeOut = time(nullptr) + m_interval;
		}
		tv.tv_sec = expectedTimeOut - time(nullptr);
		// set up watcher
		int returnVal = select(maxSockfd + 1, &readFds, NULL, NULL, &tv);
		if (returnVal == -1) {
			perror("select");
			return 4;
		}

		else if (returnVal == 0)
		{
			connectTracker();
			sendTrackerRequest();
			m_isFirstReq = false;
			recvTrackerResponse();
			connectPeers();
			expectedTimeOut = time(nullptr) + m_interval;
		}
		else
		{
			for (int fd = 0; fd <= maxSockfd; fd++) {
				// get one socket for reading
				if (FD_ISSET(fd, &readFds)) {
					if (fd == sockfd) { // this is the listen socket
						struct sockaddr_in clientAddr;
						socklen_t clientAddrSize;
						// I can assume that if a peer already set up a connection with me, it would not try again
						int clientSockfd = accept(fd, (struct sockaddr*)&clientAddr, &clientAddrSize);

						if (clientSockfd == -1) {
							perror("accept");
							return 5;
						}

						PeerConnection newConn(clientSockfd, false, true);
						m_peerConnections[clientSockfd] = newConn;

						// update maxSockfd
						if (maxSockfd < clientSockfd)
							maxSockfd = clientSockfd;

						// add the socket into the socket set
						FD_SET(clientSockfd, &tmpFds);
					}
					else { // this is the normal socket
						PeerConnection peerConn = m_peerConnections[fd];
						if (peerConn.isWaitingHS())
						{
							char* buf = new char[68];
							memset(buf, '\0', sizeof(buf));
							if (recv(fd, buf, 68, 0) == -1) {
								perror("recv");
								return 6;
							}
							// convert received char* buffer to ConstBufferPtr so I can extract the id (type) of the message
							ConstBufferPtr recvData = make_shared<const Buffer>(buf, sizeof(buf));
							if (peerConn.getInitiated())  // if i first sent a handshake, now I need to send a bitfield
								sendBitfield(fd);
							else
								sendHandshake(fd);
							peerConn.setNotWaitingHS();
						}
						else  // not a handshake
						{
							char* buf = new char[5];  // read first 5 bytes to determine length and id
							memset(buf, '\0', sizeof(buf));
							if (recv(fd, buf, 5, 0) == -1) {
								perror("recv");
								return 6;
							}
							// convert received char* buffer to ConstBufferPtr so I can extract the id (type) of the message
							ConstBufferPtr recvData = make_shared<const Buffer>(buf, sizeof(buf));
							const uint32_t* value = reinterpret_cast<const uint32_t*>(recvData->buf());
							uint32_t len = ntohl(*value);
							uint8_t msgId = (*recvData)[4];  // ID_OFFSET
							switch (msgId)
							{
							case 1:  // unchoke
							{
								char buf2[1] = { 0 };
								recv(fd, buf2, 1, 0);  // // unchoke is not special so do not need any processing; unchoke msg is one-byte long
								sendRequest(fd);  // inputs should be index wanted (need to check bitfield of peer to see availability)
								// setLastReq() and m_requestSent done in sendRequest()
								break; 
							}
							case 2:  // interested
							{
								char buf3[1] = { 0 };
								recv(fd, buf3, 1, 0);  // // interested is not special so do not need any processing; interested msg is one-byte long
								sendUnchoke(fd);
								break;
							}
							case 4:  // have
							{
								char buf4[4] = { 0 };
								recv(fd, buf4, 4, 0);  // receive the piece index
								char integrated[9];  // concatenated with length
								for (int j = 0; j < 5; ++j)
									integrated[j] = buf[j];
								for (int j = 5; j < 9; ++j)
									integrated[j] = buf4[j - 5];
								ConstBufferPtr hptr = make_shared<const Buffer>(integrated, 9);
								msg::Have haveMsg;
								haveMsg.decode(hptr);
								uint32_t newIndex = haveMsg.getIndex();
								peerConn.setOneBit(newIndex);  //update peer bitfield
								m_uploaded += m_pieceLen;
								break; 
							}
							case 5: // bitfield
							{
								char * newBuf = new char[m_numBytes];
								memset(newBuf, 0, m_numBytes);
								recv(fd, newBuf, m_numBytes, 0);
								ConstBufferPtr ttt = make_shared<const Buffer>(newBuf, m_numBytes);  // should be constbufferptr bitfield
								// initialize the peer's bitfield
								peerConn.setPeerBitfield(ttt, m_numPieces);
								if (peerConn.getInitiated())  //"I have initiated this socket connection (this socket is for downloading)"
									// can compare fd with a list of fd's populated when I set up the connections to peers before the while loop
									// assume I am always interested :p
									sendInterested(fd);
								else  // this socket is an uploader
									sendBitfield(fd);
								break;
							}
							case 6:  // request
							{
								char * buf5 = new char[12];
								memset(buf5, 0, 12);
								recv(fd, buf5, 12, 0);
								char * totalBuf = new char[5 + 12];
								for (int k = 0; k < 5; ++k)
									totalBuf[k] = buf[k];
								for (int k = 5; k < 17; ++k)
									totalBuf[k] = buf5[k - 5];
								ConstBufferPtr setUp = make_shared<const Buffer>(totalBuf, 17);
								msg::Request req;
								req.decode(setUp);
								int ind = req.getIndex();
								int off = req.getBegin();
								int length = req.getLength();
								sendPiece(fd, ind, off, length);  // when sending, check that I do have the requested piece. Then use index and offset to upload the correct data
								break; 
							}
							case 7:  //piece
							{
								char* tempBuff = new char[len - 1];  //it should be that len == m_pieceLen+9
								memset(tempBuff, 0, len - 1);
								recv(fd, tempBuff, len - 1, 0);  // receive block and index and offset
								char * totalBuf2 = new char[5 + len - 1];  // concatentate with length
								for (int k = 0; k < 5; ++k)
									totalBuf2[k] = buf[k];
								for (int k = 5; k < 5 + len - 1; ++k)
									totalBuf2[k] = tempBuff[k - 5];
								ConstBufferPtr ttttt = make_shared<const Buffer>(totalBuf2, len - 1 + 5);
								//myFile2<<"debugging seg fault: "<<"check 7"<<std::endl;
								msg::Piece piece;
								piece.decode(ttttt);  // now piece is ready to be checked
								uint32_t pieceIndex = piece.getIndex();
								if (peerConn.getLastReq() == pieceIndex)  // check the index is the one I requested
								{
									if (checkPieceHash(piece)){  // check hash
										uint32_t pieceOffset = piece.getBegin();
										std::ofstream out(m_metaInfo.getName(), std::ofstream::binary | std::ofstream::app);
										std::ofstream test_out(m_metaInfo.getName() + "Zhao", std::ofstream::binary | std::ofstream::app);
										out.seekp(pieceIndex*m_pieceLen + pieceOffset);
										test_out.seekp(pieceIndex*m_pieceLen + pieceOffset);
										ConstBufferPtr anotherBuf = piece.getBlock();
										out.write((char*)(anotherBuf->buf()), anotherBuf->size());
										test_out.write((char*)(anotherBuf->buf()), anotherBuf->size());
										out.close();
										test_out.close();
										m_bitfield[pieceIndex] = 1;  // update my bitfield
										m_left -= anotherBuf->size();
										sendHave(fd, pieceIndex);  // if piece is good, send have_msg.
									}
								}
								break;
							}
							}
						}
					}
				}
			}
		}
	}
	return 0;
}

void
Client::sendHave(const int& fd, const int& index){
	msg::Have haveMsg(index);
	ConstBufferPtr whatever = haveMsg.encode();
	send(fd, whatever->get(),whatever->size(),0);
}

void
Client::sendRequest(const int& fd)
{
	PeerConnection pc = m_peerConnections[fd];
	std::vector<int> pbf = pc.getBitfield();
	for (int i = 0; i < m_bitfield.size();++i)
		if (m_bitfield[i] == 0)
			if (m_requestSent[i]==false)
				if (pbf[i] == 1)
				{
					msg::Request req(i, 0, m_pieceLen);
					//myFile2<<"debugging seg fault: "<<"check 3"<<std::endl;
					ConstBufferPtr d = req.encode();
					//myFile2<<"debugging seg fault: "<<"check 4"<<std::endl;
					send(fd, d->get(), d->size(), 0);
					m_peerConnections[fd].setLastReq(i);
					m_uploaded += m_pieceLen;
					m_requestSent[i] = true;
				}
}
// returns true if piece is good
// returns false if piece is bad
bool Client::checkPieceHash(const msg::Piece& piece)
{
	ConstBufferPtr block = piece.getBlock();
	uint32_t index = piece.getIndex();

	// should be 20 bytes of length, because a block is a piece
	ConstBufferPtr hash = util::sha1(block);
	std::string hashStr = "";
	char* c = (char*)(hash->buf());
	for (int i = 0; i < hash->size(); ++i){
			hashStr += *(c + i);
	}

	return (hashStr == m_hashPieces[index]);

}


/*used by download()*/
void Client::connectPeers()
{
	for (auto peer : m_peers)
	{	//if I haven't set up a connection with this peer before
		if (std::find(m_peerIdList.begin(), m_peerIdList.end(), peer.peerId) == m_peerIdList.end())
		{
			int sockfd = socket(AF_INET, SOCK_STREAM, 0);
			m_client_socketFd.push_back(sockfd);

			struct sockaddr_in serverAddr;
			serverAddr.sin_family = AF_INET;
			serverAddr.sin_port = htons(peer.port);     // short, network byte order
			serverAddr.sin_addr.s_addr = inet_addr(peer.ip.c_str());
			memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

			// connect to the server
			if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
				perror("connect");
				throw Error("Cannot connect to peer");
			}

			sendHandshake(sockfd);

			PeerConnection newConn(sockfd, true, true, peer.peerId);
			m_peerConnections[sockfd] = newConn;
			m_peerIdList.push_back(peer.peerId);
		}
	}
}

void
Client::sendPiece(const int& fd, const int& index, const int& offset, const int& length)
{
	std::ifstream is(m_metaInfo.getName(), std::ifstream::binary);  // is the stream of the entire file
	if (is)
	{
		std::streampos pos = index*m_pieceLen + offset;
		is.seekg(pos);  // now read pointer points to the start position
		char * buffer = new char[length];
		memset(buffer, 0, length);
		is.read(buffer, length);
		ConstBufferPtr block = make_shared<const Buffer>(buffer, is.gcount());
		
		msg::Piece p(index, offset, block);
		ConstBufferPtr ptr = p.encode();
		send(fd, ptr->buf(), ptr->size(), 0);
	}

}

// send trivial message
void
Client::sendUnchoke(const int& fd){
	msg::Unchoke unchokeMsg;
	ConstBufferPtr q = unchokeMsg.encode();
	send(fd, q->get(), q->size(), 0);
}

void
Client::sendInterested(const int& fd){
	msg::Interested interestMsg;
	ConstBufferPtr q = interestMsg.encode();
	send(fd, q->get(), q->size(), 0);
}

void 
Client::vectorToBitfield(const std::vector<uint8_t>& bitFieldVec, char* cPtr)
{
	for (int i = 0, count = 0; i<m_numBytes; i++)
	{
		for (int j = 0; j<8 && count < m_numPieces; j++, count++)
		{
			cPtr[i] += (bitFieldVec[count] << (8 - 1 - j));
		}
	}
	//result = make_shared<Buffer>(buf, m_numBytes);
	//return result;
}

std::vector<int>
Client::bitfieldToVector(ConstBufferPtr bitfield)
{
	std::vector<int> result;

	int size = bitfield->size();
	const uint8_t* buf = bitfield->buf();
	for (int i = 0, count = 0; i<size; i++)
	{
		for (int j = 0; j<8; j++, count++)
		{
			if (count > m_numPieces)
			{
				result.push_back(0);
			}
			else
			{
				result.push_back((1 << (8 - 1 - j) & buf[i]) >> (8 - 1 - j));
			}
		}
	}
	return result;
}

void Client::sendHandshake(const int& fd)
{
	msg::HandShake hsA(m_metaInfo.getHash(), "SIMPLEBT.TEST.PEERID");
	ConstBufferPtr t = hsA.encode();
	send(fd, t->get(), t->size(), 0);
}

void
Client::sendBitfield(const int& fd){
	char* bitField = new char[m_numBytes];
	vectorToBitfield(m_bitfield, bitField);  // assume this function works
	ConstBufferPtr ttt = make_shared<const Buffer>(bitField, m_numBytes);
	msg::Bitfield bf(ttt);

	ConstBufferPtr tttt = bf.encode();
	send(fd, tttt->get(), tttt->size(), 0);
}

//void Client::sendPeerRequest()
//{
//	std::string fileName = m_metaInfo.getName();
//	// first create a file using the file name from the meta info file
//	std::ofstream myFile2("testZhao.txt",std::ios::app);
//	std::ofstream myFile;
//	myFile.open(fileName.c_str(), std::ios::app);
//
//	for (auto fd : m_client_socketFd)
//	{
//		// send and receive hand shake
//		msg::HandShake hsA(m_metaInfo.getHash(), "SIMPLEBT-TEST-PEERID");
//		ConstBufferPtr t = hsA.encode();
//		send(fd, t->get(), t->size(), 0);
//		char buf[68] = { 0 };
//		//memset(buf, '\0', sizeof(buf));
//		//memcpy(buf, lastTree, 3);
//
//		ssize_t res = recv(fd, buf, 68, 0);
//		
//		if (res == -1) {
//			perror("recv");
//			return;
//		}
//
//		ConstBufferPtr tt = make_shared<const Buffer>(buf, 68);
//
//		msg::HandShake hsB;
//		hsB.decode(tt);
//		//std::ofstream out("handshake");
//		//out<<hsB.getInfoHash()->buf()<<std::endl;
//		//out.close();
//		// now calculate how many bytes are needed for the bit field
//		// send and receive bit field
//		int fileLen = m_metaInfo.getLength();
//		int pieceLen = m_metaInfo.getPieceLength();
//		int numPieces = 0;
//		if (fileLen % pieceLen == 0)
//			numPieces = fileLen / pieceLen;
//		else
//			numPieces = (fileLen / pieceLen) + 1;
//		int numBytes=0;
//		if (numPieces%8 == 0)
//			numBytes = numPieces/8;
//		else
//			numBytes = (numPieces/8) + 1;
//		char* bitField = new char[numBytes];
//
//		memset(bitField, '\0', numBytes);  // bitField is all zero because I have nothing
//
//		ConstBufferPtr ttt = make_shared<const Buffer>(bitField, numBytes);
//		msg::Bitfield bf(ttt);
//		
//		ConstBufferPtr tttt = bf.encode();
//		send(fd, tttt->get(), tttt->size(), 0);
//
//		char* buf2 = new char[numBytes];
//		ssize_t ress = recv(fd, buf2, numBytes, 0);
//
//		if (ress == -1) {
//			perror("recv");
//			return;
//		}
//
//		// send interest and receive unchoke messages
//		msg::Interested interestMsg;
//		ConstBufferPtr q = interestMsg.encode();
//		send(fd, q->get(), q->size(), 0);
//		//because interest msg is only 1-byte long
//		char buf3[2048] = { 0 };
//
//		ssize_t rees = recv(fd, buf3, 2048, 0);  // receives unchoke message
//
//		if (rees == -1) {
//			perror("recv");
//			return;
//		}
//		/*ConstBufferPtr tt = make_shared<const Buffer>(buf, 68);
//		msg::HandShake hsB;
//		hsB.decode(tt);*/
//
//		// now assume the peer has every single piece
//		// so I just keep sending request from 0 to the last piece
//		//char* data = new char[pieceLen];
//		for (int i = 0; i < numPieces; ++i)
//		{
//			char* data = new char[pieceLen+512];
//			//myFile<<std::endl<<"piece count: "<<i<<std::endl;
//			//myFile2<<"debugging seg fault: "<<"check 1"<<std::endl;
//			memset(data, '\0', pieceLen+512);
//			// send 1st request msg
//			//myFile2<<"debugging seg fault: "<<"check 2"<<std::endl;
//			msg::Request req(i,0,pieceLen);
//			//myFile2<<"debugging seg fault: "<<"check 3"<<std::endl;
//			ConstBufferPtr d = req.encode();
//			//myFile2<<"debugging seg fault: "<<"check 4"<<std::endl;
//			send(fd, d->get(), d->size(), 0);
//			//myFile2<<"debugging seg fault: "<<"check 5"<<std::endl;
//			sleep(1);
//			ssize_t dddd = recv(fd, data, pieceLen+512, 0);
//			if (dddd == -1) {
//				perror("recv");
//				return;
//			}
//			//myFile2<<"debugging seg fault: "<<"check 6"<<std::endl;
//			ConstBufferPtr ttttt = make_shared<const Buffer>(data, pieceLen+512);
//			//myFile2<<"debugging seg fault: "<<"check 7"<<std::endl;
//			msg::Piece piece;
//			piece.decode(ttttt);
//			//piece.getBlock();
//			//myFile2<<"debugging seg fault: "<<"check 8"<<std::endl;
//			myFile << piece.getBlock()->buf(); 
//			//myFile2<<"debugging seg fault: "<<"check 9"<<std::endl;
//			myFile2 << piece.getBlock()->buf();
//			
//			uint32_t index = piece.getIndex();
//			msg::Have haveMsg(index);
//			ConstBufferPtr whatever = haveMsg.encode();
//			send(fd, whatever->get(),whatever->size(),0);
//			delete[] data;
//		}
//		myFile.close();
//		myFile2.close();
//		delete[] buf2;
//		delete[] bitField;
//		//delete[] data;
//		buf2 = nullptr;
//		bitField = nullptr;
//		//data = nullptr;
//	}
//	
//}

void
Client::loadMetaInfo(const std::string& torrent)
{
  std::ifstream is(torrent);
  m_metaInfo.wireDecode(is);

  std::string announce = m_metaInfo.getAnnounce();
  std::string url;
  std::string defaultPort;
  if (announce.substr(0, 5) == "https") {
    url = announce.substr(8);
    defaultPort = "443";
  }
  else if (announce.substr(0, 4) == "http") {
    url = announce.substr(7);
    defaultPort = "80";
  }
  else
    throw Error("Wrong tracker url, wrong scheme");

  size_t slashPos = url.find('/');
  if (slashPos == std::string::npos) {
    throw Error("Wrong tracker url, no file");
  }
  m_trackerFile = url.substr(slashPos);

  std::string host = url.substr(0, slashPos);

  size_t colonPos = host.find(':');
  if (colonPos == std::string::npos) {
    m_trackerHost = host;
    m_trackerPort = defaultPort;
  }
  else {
    m_trackerHost = host.substr(0, colonPos);
    m_trackerPort = host.substr(colonPos + 1);
  }
}

void
Client::connectTracker()
{
  m_trackerSock = socket(AF_INET, SOCK_STREAM, 0);

  struct addrinfo hints;
  struct addrinfo* res;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; // IPv4
  hints.ai_socktype = SOCK_STREAM;

  // get address
  int status = 0;
  if ((status = getaddrinfo(m_trackerHost.c_str(), m_trackerPort.c_str(), &hints, &res)) != 0)
    throw Error("Cannot resolver tracker ip");

  struct sockaddr_in* ipv4 = (struct sockaddr_in*)res->ai_addr;
  char ipstr[INET_ADDRSTRLEN] = {'\0'};
  inet_ntop(res->ai_family, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
  // std::cout << "tracker address: " << ipstr << ":" << ntohs(ipv4->sin_port) << std::endl;

  if (connect(m_trackerSock, res->ai_addr, res->ai_addrlen) == -1) {
    perror("connect");
    throw Error("Cannot connect tracker");
  }

  freeaddrinfo(res);
}

void
Client::sendTrackerRequest()
{
  TrackerRequestParam param;

  param.setInfoHash(m_metaInfo.getHash());
  param.setPeerId(m_id); //TODO:
  param.setIp("127.0.0.1"); //TODO:
  param.setPort(m_clientPort); //TODO:
  param.setUploaded(m_uploaded); //TODO:
  param.setDownloaded(m_downloaded); //TODO:
  param.setLeft(m_left); //TODO:
  if (m_isFirstReq)
    param.setEvent(TrackerRequestParam::STARTED);

  // std::string path = m_trackerFile;
  std::string path = m_metaInfo.getAnnounce();
  path += param.encode();

  HttpRequest request;
  request.setMethod(HttpRequest::GET);
  request.setHost(m_trackerHost);
  request.setPort(boost::lexical_cast<uint16_t>(m_trackerPort));
  request.setPath(path);
  request.setVersion("1.0");

  Buffer buffer(request.getTotalLength());

  request.formatRequest(reinterpret_cast<char *>(buffer.buf()));

  send(m_trackerSock, buffer.buf(), buffer.size(), 0);
}

void
Client::recvTrackerResponse()
{
  std::stringstream headerOs;
  std::stringstream bodyOs;

  char buf[512] = {0};
  char lastTree[3] = {0};

  bool hasEnd = false;
  bool hasParseHeader = false;
  HttpResponse response;

  uint64_t bodyLength = 0;

  while (true) {
    memset(buf, '\0', sizeof(buf));
    memcpy(buf, lastTree, 3);

    ssize_t res = recv(m_trackerSock, buf + 3, 512 - 3, 0);

    if (res == -1) {
      perror("recv");
      return;
    }

    const char* endline = 0;

    if (!hasEnd)
      endline = (const char*)memmem(buf, res, "\r\n\r\n", 4);

    if (endline != 0) {
      const char* headerEnd = endline + 4;

      headerOs.write(buf + 3, (endline + 4 - buf - 3));

      if (headerEnd < (buf + 3 + res)) {
        bodyOs.write(headerEnd, (buf + 3 + res - headerEnd));
      }

      hasEnd = true;
    }
    else {
      if (!hasEnd) {
        memcpy(lastTree, buf + res, 3);
        headerOs.write(buf + 3, res);
      }
      else
        bodyOs.write(buf + 3, res);
    }

    if (hasEnd) {
      if (!hasParseHeader) {
        response.parseResponse(headerOs.str().c_str(), headerOs.str().size());
        hasParseHeader = true;

        bodyLength = boost::lexical_cast<uint64_t>(response.findHeader("Content-Length"));
      }
    }

    if (hasParseHeader && bodyOs.str().size() >= bodyLength)
      break;
  }

  close(m_trackerSock);
  FD_CLR(m_trackerSock, &m_readSocks);


  bencoding::Dictionary dict;

  std::stringstream tss;
  tss.str(bodyOs.str());
  dict.wireDecode(tss);

  TrackerResponse trackerResponse;
  trackerResponse.decode(dict);
  //  const std::vector<PeerInfo>&
  m_peers = trackerResponse.getPeers();
 
  m_interval = trackerResponse.getInterval();

//  if (m_isFirstRes) {
  //  for (const auto& peer : peers) {
      //std::cout << peer.ip << ":" << peer.port << std::endl;
//		m_peers.push_back(peer);
  //  }
//  }

  m_isFirstRes = false;
}

} // namespace sbt
