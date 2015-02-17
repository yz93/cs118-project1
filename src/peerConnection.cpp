#include "peerConnection.hpp"
#include "tracker-response.hpp"
#include <vector>
#include "meta-info.hpp"
#include "util/buffer.hpp"

namespace sbt{
	PeerConnection::PeerConnection(int sockfd, bool initiated, bool waitingForHandshake)
		: m_sockfd(sockfd), m_initiated(initiated), m_waitingForHandshake(waitingForHandshake), lastRequestedPiece(-1)
	{
		if (m_initiated)
			for (int i = 0; i < 24; ++i)
				peer_bitField.push_back(1);
		else
			for (int i = 0; i < 24; ++i)
				peer_bitField.push_back(0);
	}
	PeerConnection::PeerConnection(int sockfd, bool initiated, bool waitingForHandshake, std::string peerId)
		: m_sockfd(sockfd), m_initiated(initiated), m_waitingForHandshake(waitingForHandshake), m_peerId(peerId), lastRequestedPiece(-1)
	{
		if (m_initiated)
			for (int i = 0; i < 24; ++i)
				peer_bitField.push_back(1);
		else
			for (int i = 0; i < 24; ++i)
				peer_bitField.push_back(0);
	}

	void PeerConnection::setPeerBitfield(const ConstBufferPtr& bitfield, int numPieces)
	{
		int size = bitfield->size();
		const uint8_t* buf = bitfield->buf();
		for (int i = 0, count = 0; i<size; i++)
			for (int j = 0; j<8; j++, count++)
				if (count > numPieces)
					peer_bitField.push_back(0);
				else
					peer_bitField.push_back((1 << (8 - 1 - j) & buf[i]) >> (8 - 1 - j));
	}


}