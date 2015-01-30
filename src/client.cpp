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
//#include "msg/handshake.cpp"
#include "msg/msg-base.hpp"
#include "tracker-request-param.hpp"
#include "tracker-response.hpp"
#include "http/http-request.hpp"
#include "http/http-response.hpp"
#include <fstream>
#include <iostream>
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
  : m_interval(3600)
  , m_isFirstReq(true)
  , m_isFirstRes(true)
  , m_id("SIMPLEBT-TEST-PEERID")
{
  srand(time(NULL));

  m_clientPort = boost::lexical_cast<uint16_t>(port);

  loadMetaInfo(torrent);

  run();
}

void
Client::run()
{
  while (true) {
    connectTracker();
    sendTrackerRequest();
    m_isFirstReq = false;
    recvTrackerResponse();  // now m_peers have a list of peers that have my requested file
	connectPeers();
	sendPeerRequest();
	//download();
    close(m_trackerSock);
    sleep(m_interval);
  }
}

/*used by download()*/
void Client::connectPeers()
{
	for (auto peer : m_peers)
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
	}
}

void Client::sendPeerRequest()
{
	std::string fileName = m_metaInfo.getName();
	// first create a file using the file name from the meta info file
	std::ofstream myFile;
	myFile.open(fileName);

	for (auto fd : m_client_socketFd)
	{
		// send and receive hand shake
		msg::HandShake hsA(m_metaInfo.getHash(), m_id);
		ConstBufferPtr t = hsA.encode();
		send(fd, t.get(), (*t).size(), 0);
		char buf[68] = { 0 };
		//memset(buf, '\0', sizeof(buf));
		//memcpy(buf, lastTree, 3);

		ssize_t res = recv(fd, buf, 68, 0);
		
		if (res == -1) {
			perror("recv");
			return;
		}

		ConstBufferPtr tt = make_shared<const Buffer>(buf, 68);

		msg::HandShake hsB;
		hsB.decode(tt);

		// now calculate how many bytes are needed for the bit field
		// send and receive bit field
		int fileLen = m_metaInfo.getLength();
		int pieceLen = m_metaInfo.getPieceLength();
		int numPieces = 0;
		if (fileLen % pieceLen == 0)
			numPieces = fileLen / pieceLen;
		else
			numPieces = (fileLen / pieceLen) + 1;

		char* bitField = new char[numPieces];

		memset(bitField, '\0', numPieces);  // bitField is all zero because I have nothing

		ConstBufferPtr ttt = make_shared<const Buffer>(bitField, numPieces);
		msg::Bitfield bf(ttt);
		
		ConstBufferPtr tttt = bf.encode();
		send(fd, tttt.get(), (*tttt).size(), 0);

		char* buf2 = new char[numPieces];
		ssize_t ress = recv(fd, buf2, numPieces, 0);

		if (ress == -1) {
			perror("recv");
			return;
		}

		// send interest and receive unchoke messages
		msg::Interested interestMsg;
		ConstBufferPtr q = interestMsg.encode();
		send(fd, q.get(), (*q).size(), 0);
		//because interest msg is only 1-byte long
		char buf3[1] = { 0 };

		ssize_t rees = recv(fd, buf3, 1, 0);  // receives unchoke message

		if (rees == -1) {
			perror("recv");
			return;
		}
		/*ConstBufferPtr tt = make_shared<const Buffer>(buf, 68);
		msg::HandShake hsB;
		hsB.decode(tt);*/

		// now assume the peer has every single piece
		// so I just keep sending request from 0 to the last piece
		char* data = new char[pieceLen];
		for (int i = 0; i < numPieces; ++i)
		{
			memset(data, '\0', pieceLen);
			// send 1st request msg
			msg::Request req(i,0,pieceLen);
			ConstBufferPtr d = req.encode();
			send(fd, d.get(), (*d).size(), 0);
			recv(fd, data, pieceLen, 0);

			ConstBufferPtr ttttt = make_shared<const Buffer>(data, pieceLen);

			msg::Piece piece;
			piece.decode(ttttt);
			piece.getBlock();
			myFile << piece.getBlock()->buf(); 
		}
		myFile.close();
		delete[] buf2;
		delete[] bitField;
		delete[] data;
	}
	
}

	//int maxSockfd = 0;

	//fd_set readFds;
	//fd_set tmpFds;
	//FD_ZERO(&readFds);
	//FD_ZERO(&tmpFds);
	//// create a socket using TCP IP
	//int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	//maxSockfd = sockfd;
	//// put the socket in the socket set
	//FD_SET(sockfd, &tmpFds);
	//// allow others to reuse the address
	//int yes = 1;
	//if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
	//	perror("setsockopt");
	//	return 1;
	//}
	//// bind address to socket
	//struct sockaddr_in addr;
	//addr.sin_family = AF_INET;
	//addr.sin_port = htons(40000);     // short, network byte order
	//addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	//memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));
	//if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
	//	perror("bind");
	//	return 2;
	//}
	//// set the socket in listen status
	//if (listen(sockfd, 10) == -1) {
	//	perror("listen");
	//	return 3;
	//}

	//// initialize timer
	//struct timeval tv;
	//tv.tv_sec = 10;
	//tv.tv_usec = 0;

	//while (true) {
	//	readFds = tmpFds;

	//	// set up watcher
	//	if (select(maxSockfd + 1, &readFds, NULL, NULL, &tv) == -1) {
	//		perror("select");
	//		return 4;
	//	}

	//	for (int fd = 0; fd <= maxSockfd; fd++) {
	//		// get one socket for reading
	//		if (FD_ISSET(fd, &readFds)) {
	//			if (fd == sockfd) { // this is the listen socket
	//				struct sockaddr_in clientAddr;
	//				socklen_t clientAddrSize;
	//				int clientSockfd = accept(fd, (struct sockaddr*)&clientAddr, &clientAddrSize);

	//				if (clientSockfd == -1) {
	//					perror("accept");
	//					return 5;
	//				}

	//				char ipstr[INET_ADDRSTRLEN] = { '\0' };
	//				inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
	//				std::cout << "Accept a connection from: " << ipstr << ":" <<
	//					ntohs(clientAddr.sin_port) << std::endl;

	//				// update maxSockfd
	//				if (maxSockfd < clientSockfd)
	//					maxSockfd = clientSockfd;

	//				// add the socket into the socket set
	//				FD_SET(clientSockfd, &tmpFds);
	//			}
	//			else { // this is the normal socket
	//				char buf[20] = { 0 };
	//				std::stringstream ss;

	//				memset(buf, '\0', sizeof(buf));
	//				if (recv(fd, buf, 20, 0) == -1) {
	//					perror("recv");
	//					return 6;
	//				}
	//				ss << buf << std::endl;

	//				struct sockaddr_in clientAddr;
	//				socklen_t clientAddrLen = sizeof(clientAddr);
	//				if (getpeername(fd, (struct sockaddr *)&clientAddr, &clientAddrLen) == -1) {
	//					perror("getpeername");
	//					return 7;
	//				}

	//				char ipstr[INET_ADDRSTRLEN] = { '\0' };
	//				inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
	//				std::cout << "receive data connection from " << ipstr << ":" <<
	//					ntohs(clientAddr.sin_port) << ": " << buf << std::endl;

	//				if (send(fd, buf, 20, 0) == -1) {
	//					perror("send");
	//					return 8;
	//				}

	//				if (ss.str() == "close\n") {
	//					// close the socket
	//					close(fd);

	//					// remove the socket from the socket set
	//					FD_CLR(fd, &tmpFds);
	//				}
	//			}
	//		}
	//	}

	//	std::cout << "keep going" << std::endl;
	//}
	//return 0;

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
  param.setPeerId("01234567890123456789"); //TODO:
  param.setIp("127.0.0.1"); //TODO:
  param.setPort(m_clientPort); //TODO:
  param.setUploaded(100); //TODO:
  param.setDownloaded(200); //TODO:
  param.setLeft(300); //TODO:
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
  const std::vector<PeerInfo>& peers = trackerResponse.getPeers();
 
  m_interval = trackerResponse.getInterval();

  if (m_isFirstRes) {
    for (const auto& peer : peers) {
      //std::cout << peer.ip << ":" << peer.port << std::endl;
		m_peers.push_back(peer);
    }
  }

  m_isFirstRes = false;
}

} // namespace sbt
