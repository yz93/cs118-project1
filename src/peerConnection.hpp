#ifndef SBT_PEERCONNECTION_HPP
#define SBT_PEERCONNECTION_HPP
#include "common.hpp"
#include "tracker-response.hpp"
#include <vector>
#include "meta-info.hpp"
#include "util/buffer.hpp"

namespace sbt {

	class PeerConnection{
	public:
		PeerConnection() {}
		PeerConnection(int sockfd, bool initiated, bool waitingForHandshake);
		PeerConnection(int sockfd, bool initiated, bool waitingForHandshake, std::string peerId);  // sockfd is the unique identifier
		void setPeerBitfield(const ConstBufferPtr& bitfield, int numPieces);
		void setOneBit(int index) { peer_bitField[index] = 1; }
		std::vector<int> getBitfield() { 
			return peer_bitField; 
		}
		bool getInitiated() { 
			return m_initiated; 
		}
		std::string getPeerId() { 
			return m_peerId; 
		}
		void setPeerId(const int& peerId) {
			m_peerId = peerId;
		}
		bool isWaitingHS() { 
			return m_waitingForHandshake; 
		}
		void setNotWaitingHS(){  // indicates no longer waiting for handshake
			m_waitingForHandshake = false;
		}

		void setLastReq(int index){
			lastRequestedPiece = index;
		}

		int getLastReq(){
			return lastRequestedPiece;
		}
	private:
		int m_sockfd;  // peerConnection unique identifer
		bool m_initiated;  // remember if I set up this connction or the other side did
		bool m_waitingForHandshake;
		std::string m_peerId;  // remember who I am talking with
		std::vector<int> peer_bitField;  // remembers what the other side has
		int lastRequestedPiece;
	};

}// namespace sbt

#endif // SBT_PEERCONNECTION_HPP