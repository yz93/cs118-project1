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
#include "http/http-request.hpp"
#include "http/http-response.hpp"
#include "util/bencoding.hpp"
#include "tracker-response.hpp"
#include <string.h>
#include "meta-info.hpp"
#include "http/url-encoding.hpp"
#include <cstdio>  // sscanf
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
using namespace std;

int
main(int argc, char** argv)
{
  try
  {
    // Check command line arguments.
    if (argc != 3)
    {
      std::cerr << "Usage: simple-bt <port> <torrent_file>\n";
      return 1;
    }

    // Initialise the client.
    sbt::Client client(argv[1], argv[2]);
	// argv[1] is port number string; argv[2] is torrent path string.

	sbt::MetaInfo m_metaInfo;
	std::ifstream torrentFile(argv[2]);  // open torrent file
	m_metaInfo.wireDecode(torrentFile);  // decode (parse) torrent file

	std::string announce = m_metaInfo.getAnnounce();  // get tracker information
	sbt::ConstBufferPtr cbp = m_metaInfo.getHash();  // getHash to see what it is.
	std::string info_hash = sbt::url::encode(cbp->get(), cbp->size());  // sizeof(cbp) or sizeof(*cbp)?

	//string s = "http://localhost:12345/announce.php";
	stringstream ss(announce);
	string hostName, portNumStr, path;
	getline(ss, hostName, '/');
	getline(ss, hostName, '/');
	getline(ss, hostName, ':');
	getline(ss, portNumStr, '/');
	getline(ss, path);
	istringstream iss(portNumStr);
	int portNum;
	iss >> portNum;

	
	//size_t requestSize = bufLastPos - buf;  // assume the last position is always after the start of buf. And that the difference is the # of chars.
	/*----------------------------------------*/
	// find tracker ip from its hostname
	struct addrinfo hints;
	struct addrinfo* res;

	// prepare hints
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_STREAM;

	// get address
	int status = 0;
	if ((status = getaddrinfo(hostName.c_str(), portNumStr.c_str(), &hints, &res)) != 0) {
		std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
		return 2;
	}
	// how to get tracker ip? why is there a for loop?
	// because one hostname links to multiple ips. Choose anyone should work.

	struct addrinfo* p = res;
	struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
	char ipstr[INET_ADDRSTRLEN] = { '\0' };
	inet_ntop(p->ai_family, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
	// always use the first ip address. ipstr now has the ip of the tracker, in c string format.

	freeaddrinfo(res); // free the linked list
	

	/* repeatedly send requests until tracker shuts the connection down*/
	
	//string url2 = announce + "?info_hash=" + info_hash + "&peer_id=ABCDEFGHIJKLMNOPQRST&port=" + argv[1] +
	//	"&uploaded=0&downloaded=0&left=0";  // no event, for middle requests
	int count = 0;  // count how many requests have been sent
	int countRes = 0; // count responses
	while (true) 
	{
		// generate url with GET parameters
		string url = "";
		if (count != 0)  // if not the first request
		{
			url = announce + "?info_hash=" + info_hash + "&peer_id=ABCDEFGHIJKLMNOPQRST&port=" + argv[1] +
				"&uploaded=0&downloaded=0&left=0&event=started";  // assume client ip is localhost; argv[1] is peer port
		}
		else
			url = announce + "?info_hash=" + info_hash + "&peer_id=ABCDEFGHIJKLMNOPQRST&port=" + argv[1] +
			"&uploaded=0&downloaded=0&left=0";  // no event, for middle requests
		// generate http request with headers
		sbt::HttpRequest req;
		req.setHost(hostName);
		req.setPort(portNum);
		req.setMethod(sbt::HttpRequest::GET);
		req.setPath(url);
		req.setVersion("1.0");
		req.addHeader("Accept-Language", "en-US");
		size_t reqLen = req.getTotalLength();  // assume http request class generates correct http request message
		char *buf = new char[reqLen];
		req.formatRequest(buf);
	

	// create a socket using TCP IP
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// struct sockaddr_in addr;
	// addr.sin_family = AF_INET;
	// addr.sin_port = htons(40001);     // short, network byte order
	// addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	// memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));
	// if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
	//   perror("bind");
	//   return 1;
	// }

	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;

	serverAddr.sin_port = htons(portNum);     // short, network byte order
	serverAddr.sin_addr.s_addr = inet_addr(ipstr);  // input should be c_str

	memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

	// connect to the server
	if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
		perror("connect");
		return 2;
	}
	/*THIS IS THE SHIT: CONNECT!*/

	// don't know why this is needed. Maybe can get client's ip and port. But do not need this anymore
	struct sockaddr_in clientAddr;
	socklen_t clientAddrLen = sizeof(clientAddr);
	if (getsockname(sockfd, (struct sockaddr *)&clientAddr, &clientAddrLen) == -1) {
		perror("getsockname");
		return 3;
	}
	//------------------------------------//

	// buf has the http request
	if (send(sockfd, buf, reqLen, 0) == -1) {
		perror("send");
		return 4;
	}

	++count;  // increment count of requests sent
	
	char responseBuff[1024] = { 0 };

	//memset(buff, '\0', sizeof(buff));

	if (recv(sockfd, responseBuff, 1024, 0) == -1) {
		perror("recv");
		return 5;
	}
	countRes++;

	std::stringstream ssss;
	ssss << responseBuff;
	string temp = ssss.str();
	string delim2 = ""; delim2 += '\r'; delim2 += '\n'; delim2 += '\r'; delim2 += '\n';
	//string beforeBody = temp.substr(0,temp.find(delim2));
	unsigned int index = temp.find(delim2);
	if (index == std::string::npos)
	{
		perror("no \r\n\r\n");
		return 666;
	}
	//string delim1 = "Content-Length:";
	
	string sizeOfBody = "";  // = temp.substr(temp.find(delim1) + 15, temp.find(delim2));
	const char * cPtr = (const char*)memmem(responseBuff, 1024, "Content-Length:", 15);
	// assume cPtr will never be NULL
	cPtr += 15;  // now cPtr points to the first char of number
	while (*cPtr != '\r'&& *(cPtr + 1) != '\n')
		sizeOfBody += *cPtr;
	istringstream iiss(sizeOfBody);
	int sizeOfBodyNum;
	iiss >> sizeOfBodyNum;  // convert body size to int type
	
	sbt::HttpResponse httpResponse;
	httpResponse.parseResponse(responseBuff, index+4);
	const char* startOfBody = responseBuff + index + 4;  // index is the location of first '\r' in '\r''\n''\r''\n'

	string message_body = "";
	for (int i = 0; i < sizeOfBodyNum; i++)
		message_body += *(startOfBody+i);

	istringstream anotherIS(message_body);
	sbt::bencoding::Dictionary d;
	d.wireDecode(anotherIS);
	sbt::TrackerResponse tr;
	tr.decode(d);

	if (countRes == 1)
	{
		vector<sbt::PeerInfo> vec = tr.getPeers();
		for (auto i : vec)
			std::cout << i.ip << ":" << i.port << endl;
	}
	/*std::string ip;
  uint16_t port;*/

	sleep(tr.getInterval());
	//ssss.clear();
	//ssss.str("");

	close(sockfd);

	}  // end while

	delete[] buf;
	
	/*cout << "Host is: " << hostName << endl;
	cout << "portNum is: " << portNum << endl;
	cout << "Path is: " << path << endl;*/
	/*std::cout <<"This is announce: "<< announce << std::endl;
	std::cout << "This info_hash: " << info_hash << std::endl;*/

  }
  catch (std::exception& e)
  {
    std::cerr << "exception: " << e.what() << "\n";
  }

  return 0;
}
