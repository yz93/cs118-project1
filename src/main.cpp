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
#include "meta-info.hpp"
#include "http/url-encoding.hpp"
#include <cstdio>  // sscanf
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
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
	std::string info_hash = sbt::url::encode(cbp->get(), sizeof(*cbp));  // sizeof(cbp) or sizeof(*cbp)?

	//string s = "http://localhost:12345/announce.php";
	stringstream ss(announce);
	string hostName, portNum, path;
	getline(ss, hostName, '/');
	getline(ss, hostName, '/');
	getline(ss, hostName, ':');
	getline(ss, portNum, '/');
	getline(ss, path);
	cout << "Host is: " << hostName << endl;
	cout << "portNum is: " << portNum << endl;
	cout << "Path is: " << path << endl;
	/*std::cout <<"This is announce: "<< announce << std::endl;
	std::cout << "This info_hash: " << info_hash << std::endl;*/

  }
  catch (std::exception& e)
  {
    std::cerr << "exception: " << e.what() << "\n";
  }

  return 0;
}
