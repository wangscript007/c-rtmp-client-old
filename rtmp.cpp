#include "rtmp.h"
#include "AMF.h"
#include "amfEncoder.h"
#include "../util.h"
#include "../socket_type.h"
#include "../log4z.h"
#include "../RTMPProcesser/RTMPServer.h"

void RTMPReset(RTMP  &r)
{
	r.socketId = -1;
	r.chunkSize = RTMP_default_chunk_size;
	r.numInvokes = 0;
	r.streamId = 0;
	r.writeable = false;
	r.audioCodecs = 3191;
	r.videoCodecs = 252;
	r.playing = false;
	r.mediaChannel = 0;
	r.targetBW = 2500000;
	r.selfBW = 2500000;
	r.selfBWlimitType = 2;
	r.bytesIn = 0;
	r.bytesInLastSend = 0;
	r.buffMS = RTMP_default_buffer_ms;
	r.link.port = RTMP_default_port;
	r.link.seekTime = 0;
	r.link.stopTime = 0;
}

void RTMPEnableWrite(RTMP & r)
{
	r.writeable = true;
}

void RTMPClose(RTMP & r)
{
	if (r.socketId >= 0)
	{
		closeSocket(r.socketId);
		r.socketId = -1;
	}
	//in out 没有数据
	for (auto i : r.mapRecvCache)
	{
		free(i.second.body);
	}
	r.mapRecvCache.clear();
}

int RTMPSetupUrl(RTMP & r, const std::string & rtmpUrl)
{
	int iret = 0;
	do
	{
		//判断协议类型
		if (strncmp(rtmpUrl.c_str(), RTMP_PROTOCOL_RTMP.c_str(), 4) == 0)
		{
			r.link.protocol = RTMP_protocol_rtmp;
		}
		else if (strncmp(rtmpUrl.c_str(), RTMP_PROTOCOL_RTMPT.c_str(), 5) == 0)
		{
			r.link.protocol = RTMP_protocol_rtmpt;
		}
		else if (strncmp(rtmpUrl.c_str(), RTMP_PROTOCOL_RTMPS.c_str(), 5) == 0)
		{
			r.link.protocol = RTMP_protocol_rtmps;
		}
		else if (strncmp(rtmpUrl.c_str(), RTMP_PROTOCOL_RTMPE.c_str(), 5) == 0)
		{
			r.link.protocol = RTMP_protocol_rtmpte;
		}
		else if (strncmp(rtmpUrl.c_str(), RTMP_PROTOCOL_RTMPP.c_str(), 5) == 0)
		{
			r.link.protocol = RTMP_protocol_rtmpp;
		}
		else if (strncmp(rtmpUrl.c_str(), RTMP_PROTOCOL_RTMPTE.c_str(), 6) == 0)
		{
			r.link.protocol = RTMP_protocol_rtmpte;
		}
		else if (strncmp(rtmpUrl.c_str(), RTMP_PROTOCOL_RTMPTS.c_str(), 6) == 0)
		{
			r.link.protocol = RTMP_protocol_rtmpts;
		}
		else
		{
			iret = -1;
			break;
		}
		char *protocol, *address, *app, *path;
		protocol = new char[rtmpUrl.size()];
		address = new char[rtmpUrl.size()];
		app = new char[rtmpUrl.size()];
		path = new char[rtmpUrl.size()];
		int port = 0;
		//判断端口号
		int ssResult = 0;
		ssResult = sscanf(rtmpUrl.c_str(),
			"%[^:]://%[^/]/%[^/]/%s",
			protocol,
			address,
			app,
			path);

		if (ssResult != 4)
		{
			iret = -1;
			delete []protocol;
			delete []address;
			delete []app;
			delete []path;
			break;
		}
		else
		{
			ssResult = sscanf(address, "%[^:]:%d", protocol, &port);
			if (ssResult != 2)
			{
				port = RTMP_default_port;
			}
		}
		r.link.port = port;
		r.link.address = address;
		//app
		r.link.app = app;
		r.link.path = path;
		//tcUrl
		r.link.tcUrl = rtmpUrl;
		r.link.tcUrl.resize(rtmpUrl.size() - r.link.path.size() - 1);
		//path  去掉后缀
		sscanf(r.link.path.c_str(), "%[^.]", path);
		r.link.path = path;
	} while (0);
	return iret;
}


int RTMPConnect(RTMP & r)
{
	//连接套接字
	r.socketId = socketCreateTcpClient(r.link.address.data(), r.link.port);
	if (r.socketId < 0)
	{
		LOG_ERROR(0, "connect " << r.link.address << " failed");
		return -1;
	}

	//握手
	if (0 != RTMPShakeHandle(r))
	{
		if (r.socketId >= 0)
		{
			closeSocket(r.socketId);
			r.socketId = -1;
		}
		LOG_ERROR(0, "shakeHandle " << r.link.address << " failed");
		return -1;
	}
	//连接指令
	RTMPPacket packet;
	packet.channel = RTMP_channel_invoke;
	packet.packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.headerType = RTMP_HEADER_LARGE;
	packet.timestamp = 0;
	packet.hasAbsTimestamp = true;
	packet.messageStreamId = 0;

	amfEncoder encoder;

	//command name:connect
	encoder.EncodeString(RTMP_CMD_connect);
	//添加执行ID
	encoder.EncodeNumber(++r.numInvokes);
	//command object
	//object 为字符串做key，其他值做value的键值对
	encoder.AppendByte(AMF_object);
	//app
	encoder.EncodeNamedString(connect_CMD_app, r.link.app);
	//flashver
	if (r.link.flashver.size())
	{
		encoder.EncodeNamedString(connect_CMD_flashver, r.link.flashver);
	}
	//swfUrl
	if (r.link.swfUrl.size())
	{
		encoder.EncodeNamedString(connect_CMD_swfUrl, r.link.swfUrl);
	}
	//tcUrl
	if (r.link.tcUrl.size())
	{
		encoder.EncodeNamedString(connect_CMD_tcUrl, r.link.tcUrl);
	}
	if (r.writeable == false)
	{
		encoder.EncodeNamedBool(connect_CMD_fpad, false);
		encoder.EncodeNamedNumber(connect_CMD_capabilities, 15.0);
		encoder.EncodeNamedNumber(connect_CMD_audioCodecs, double(r.audioCodecs));
		encoder.EncodeNamedNumber(connect_CMD_videoCodecs, r.videoCodecs);
		encoder.EncodeNamedNumber(connect_CMD_videoFunction, 1.0);
	}
	//obj end
	encoder.EncodeInt24(AMF_object_end);
	int tmpLength;
	packet.body = encoder.getAmfData(tmpLength);
	packet.messageLength = tmpLength;
	RTMPSendPacket(r, packet, true);
	//free encoder的析构函数会释放amf buffer
	return 0;
}

int RTMPConnectStream(RTMP & r, int seekTime)
{
	if (seekTime > 0)
	{
		r.link.seekTime = seekTime;
	}
	RTMPPacket packet;
	while (!r.playing&&r.socketId >= 0 && 0 == RTMPRecvPacket(r, packet))
	{
		if (packet.packetType == RTMP_PACKET_TYPE_AUDIO ||
			RTMP_PACKET_TYPE_VIDEO == packet.packetType ||
			RTMP_PACKET_TYPE_INFO == packet.packetType)
		{
			if (packet.body)
			{
				free(packet.body);
				packet.body = nullptr;
			}
			continue;
		}
		RTMPClientProcessPacket(r, packet);
		if (packet.body)
		{
			free(packet.body);
			packet.body = nullptr;
		}
	}
	if (r.playing)
	{
		return 0;
	}
	return -1;
}

int RTMPReadStreamData(RTMP & r, RTMPPacket &packet_)
{
	//读取一个rtmp packet
	if (0 != RTMPRecvPacket(r, packet_))
	{
		return -1;
	}
	//如果是metadata 或 音视频，返回数据，data为空
	
	if (RTMPClientProcessPacket(r, packet_) >= 0)
	{
		
		if (RTMP_PACKET_TYPE_INFO == packet_.packetType)
		{
			AMFObject obj;
			std::string amf;
		}
		else if (RTMP_PACKET_TYPE_AUDIO == packet_.packetType)
		{
			int a = 32;
		}
		else if (RTMP_PACKET_TYPE_VIDEO == packet_.packetType)
		{
			int a = 32;
		}
		else if (RTMP_PACKET_TYPE_FLASH_VIDEO == packet_.packetType)
		{

		}
	}
	else
	{
		return -1;
	}
	
	return 0;
}

int RTMPShakeHandle(RTMP & r)
{
	bool successed = false;
	do
	{
		////发送C0
		//int ret;
		//C0S0 c0;
		//c0.version = 3;
		//ret = socketSend(r.socketId, (const char*)&c0.version, 1);
		//if (ret != 1)
		//{
		//	break;
		//}
		////发送C1
		//C1S1 c1;
		//c1.time = 0;
		//c1.zero = 0;
		//std::string tmpString;
		//tmpString = generateRandomString(ShakeHandleRandomSize);
		//memcpy(c1.randomBytes, tmpString.c_str(), ShakeHandleRandomSize);
		//ret = socketSend(r.socketId, (const char*)&c1.time, 4);
		//if (ret != 4)
		//{
		//	break;
		//}
		//ret = socketSend(r.socketId, (const char*)&c1.zero, 4);
		//if (ret != 4)
		//{
		//	break;
		//}
		//ret = socketSend(r.socketId, (const char*)c1.randomBytes, ShakeHandleRandomSize);
		//if (ret != ShakeHandleRandomSize)
		//{
		//	break;
		//}
		////等待S0
		//C0S0 s0;
		//ret = RTMPClientSocketRecv(r, (char *)&s0.version, 1);
		//if (ret != 1)
		//{
		//	break;
		//}
		////等待S1
		//C1S1 s1;
		//ret = RTMPClientSocketRecv(r, (char*)&s1.time, 4);
		//if (ret != 4)
		//{
		//	break;
		//}
		//ret = RTMPClientSocketRecv(r, (char*)&s1.zero, 4);
		//if (ret != 4)
		//{
		//	break;
		//}
		//ret = RTMPClientSocketRecv(r, (char*)s1.randomBytes, ShakeHandleRandomSize);
		//if (ret != ShakeHandleRandomSize)
		//{
		//	break;
		//}
		////发送C2
		//C2S2 c2;
		//c2.time = s1.time;
		//c2.time2 = s1.zero;
		//memcpy(c2.randomEcho, s1.randomBytes, ShakeHandleRandomSize);
		//ret = socketSend(r.socketId, (char*)&c2.time, 4);
		//if (ret != 4)
		//{
		//	break;
		//}
		//ret = socketSend(r.socketId, (char*)&c2.time2, 4);
		//if (ret != 4)
		//{
		//	break;
		//}
		//ret = socketSend(r.socketId, (char*)c2.randomEcho, ShakeHandleRandomSize);
		//if (ret != ShakeHandleRandomSize)
		//{
		//	break;
		//}
		////等待S2
		//C2S2 s2;
		//ret = RTMPClientSocketRecv(r, (char*)&s2.time, 4);
		//if (ret != 4)
		//{
		//	break;
		//}
		//ret = RTMPClientSocketRecv(r, (char*)&s2.time2, 4);
		//if (ret != 4)
		//{
		//	break;
		//}
		//ret = RTMPClientSocketRecv(r, (char*)s2.randomEcho, ShakeHandleRandomSize);
		//if (ret != ShakeHandleRandomSize)
		//{
		//	break;
		//}
		//successed = true;
	} while (0);
	if (successed)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

int RTMPClientProcessPacket(RTMP & r, RTMPPacket & packet)
{
	RTMP_PACKET_TYPE packetType = (RTMP_PACKET_TYPE)packet.packetType;
	int ret = 0;
	//LOGI("packet type:" << packetType << "...");
	switch (packetType)
	{
	case RTMP_PACKET_TYPE_CHUNK_SIZE:
		RTMPHandleChunkSize(r, packet);
		break;
	case RTMP_PACKET_TYPE_ABORT:
		break;
	case RTMP_PACKET_BYTES_READ_REPORT:
		break;
	case RTMP_PACKET_TYPE_CONTROL:
		RTMPHandleCtrl(r, packet);
		break;
	case RTMP_PACKET_TYPE_SERVER_BW:
		RTMPHandleAcknowledgementBW(r, packet);
		break;
	case RTMP_PACKET_TYPE_CLIENT_BW:
		RTMPHandleSetPeerBW(r, packet);
		break;
	case RTMP_PACKET_TYPE_AUDIO:
		break;
	case RTMP_PACKET_TYPE_VIDEO:
		break;
	case RTMP_PACKET_TYPE_FLEX_STREAM_SEND:
		break;
	case RTMP_PACKET_TYPE_FLEX_SHARED_OBJECT:
		break;
	case RTMP_PACKET_TYPE_FLEX_MESSAGE:
		break;
	case RTMP_PACKET_TYPE_INFO:
		break;
	case RTMP_PACKET_TYPE_SHARED_OBJECT:
		break;
	case RTMP_PACKET_TYPE_INVOKE:
		RTMPHandleInvoke(r, packet);
		break;
	case RTMP_PACKET_TYPE_FLASH_VIDEO:
		break;
	default:
		break;
	}
	return ret;
}

int RTMPSendCreateStream(RTMP & r)
{
	RTMPPacket packet;
	packet.channel = RTMP_channel_invoke;
	packet.headerType = RTMP_HEADER_MEDIUM;
	packet.packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.timestamp = 0;
	packet.hasAbsTimestamp = false;
	packet.messageStreamId = 0;

	amfEncoder encoder;
	encoder.EncodeString(RTMP_CMD_createStream);
	encoder.EncodeNumber(++r.numInvokes);
	encoder.AppendByte(AMF_null);
	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0 != RTMPSendPacket(r, packet, true))
	{
		LOGE("send createStream failed");
		return -1;
	}
	return 0;
}

int RTMPSendPlay(RTMP & r)
{
	RTMPPacket packet;

	packet.channel = RTMP_channel_audioVideo;//?why
	packet.headerType = RTMP_HEADER_LARGE;
	packet.packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.timestamp = 0;
	packet.messageStreamId = r.streamId;
	packet.hasAbsTimestamp = false;

	amfEncoder encoder;
	//cmd name
	encoder.EncodeString(RTMP_CMD_play);
	//transaction id
	encoder.EncodeNumber(++r.numInvokes);
	//command object null
	encoder.AppendByte(AMF_null);
	//stream name
	encoder.EncodeString(r.link.path);
	//optional
	//start -2：先尝试直播流，再尝试录制流，如果没有录制流等待直播流。 -1：只播放直播流 非负数：从该值开始播放的录制流
	if (r.link.seekTime > 0)
	{
		encoder.EncodeNumber(r.link.seekTime);
	}
	else
	{
		encoder.EncodeNumber(-2);
	}
	//duration 默认-1，小于-1按-1算
	if (r.link.stopTime)
	{
		encoder.EncodeNumber(r.link.stopTime - r.link.seekTime);
	}

	//reset bool 默认false 这里采用默认值

	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0 != RTMPSendPacket(r, packet, true))
	{
		LOGE("send play packet failed");
		return -1;
	}
	return 0;
}


int RTMPSendPacket(RTMP & r, RTMPPacket & packet, bool queue)
{
	//根据chunk id来决定基本头大小 
	
	int baseHeaderSize = 1;
	if (packet.channel > 319)
	{
		baseHeaderSize = 3;
	}
	else if (packet.channel > 63)
	{
		baseHeaderSize = 2;
	}
	//根据包类型和上一个的包类型来改变包类型
	unsigned int sendTime = 0;
	unsigned int last = 0;
	auto prevPacket = r.mapChannelsOut.find(packet.channel);
	if (prevPacket != r.mapChannelsOut.end() && packet.headerType != RTMP_HEADER_LARGE)
	{
		if (prevPacket->second.messageLength == packet.messageLength &&
			prevPacket->second.packetType == packet.packetType&&
			packet.headerType == RTMP_HEADER_MEDIUM)
		{
			packet.headerType = RTMP_HEADER_SMALL;
		}
		if (prevPacket->second.timestamp == packet.timestamp&&
			packet.headerType == RTMP_HEADER_SMALL)
		{
			packet.headerType = RTMP_HEADER_MINIMUM;
		}
		last = prevPacket->second.timestamp;
	}
	sendTime = packet.timestamp - last;
	char tmp8 = 0;
	//析构时自行释放数据
	amfEncoder dataSend;
	//先发送一个对应类型的包，如果一个包发送不完，剩下的数据以类型3发送
	//basic header
	tmp8 = packet.headerType << 6;
	if (baseHeaderSize > 1)
	{
		tmp8 = tmp8 | (baseHeaderSize - 1);
	}
	else
	{
		tmp8 = (tmp8 | packet.channel);
	}
	dataSend.AppendByte(tmp8);
	if (baseHeaderSize > 1)
	{
		int tmpChannel = packet.channel - 64;
		dataSend.AppendByte(tmpChannel & 0xff);
		if (baseHeaderSize == 3)
		{
			dataSend.AppendByte(tmpChannel >> 8);
		}
	}
	//msg header
	//time 0,1,2
	if (packet.headerType < RTMP_HEADER_MINIMUM)
	{
		dataSend.EncodeInt24(sendTime >= 0xffffff ? 0xffffff : sendTime);
	}
	//msg length 0,1
	//msg type	 0,1
	if (packet.headerType < RTMP_HEADER_SMALL)
	{
		dataSend.EncodeInt24(packet.messageLength);
		dataSend.AppendByte(packet.packetType);
	}
	//stream id	 0
	if (packet.headerType < RTMP_HEADER_MEDIUM)
	{
		dataSend.EncodeInt32LittleEndian(packet.messageStreamId);
	}
	//time extension
	if (sendTime >= 0xffffff && packet.headerType < RTMP_HEADER_MINIMUM)
	{
		dataSend.EncodeInt32(sendTime);
	}
	//如果没有数据，返回0

	//发送chunk size大小的数据
	int lastSendSize = 0;
	int bodyCur = 0;
	int firstChunkSize = packet.messageLength > r.chunkSize ? r.chunkSize : packet.messageLength;
	lastSendSize = packet.messageLength;
	
	dataSend.AppendByteArray(packet.body, firstChunkSize);
	
	int sendSize = 0;
	char *pData = dataSend.getAmfData(sendSize);
	if (sendSize != socketSend(r.socketId, pData, sendSize))
	{
		return -1;
	}

	bodyCur += firstChunkSize;
	//发送剩余数据
	while (bodyCur != packet.messageLength)
	{
		dataSend.reset();
		int lastDataSize = packet.messageLength - bodyCur;
		if (lastDataSize > r.chunkSize)
		{
			lastDataSize = r.chunkSize;
		}
		//header
		//包类型3 chunkid
		tmp8 = 0x3 << 6;
		if (baseHeaderSize == 1)
		{
			tmp8 = tmp8 | packet.channel;
			dataSend.AppendByte(tmp8);
		}
		if (baseHeaderSize > 1)
		{
			tmp8 = tmp8 | (baseHeaderSize - 1);
			dataSend.AppendByte(tmp8);
			int tmpChannel = packet.channel - 64;
			dataSend.AppendByte(tmpChannel & 0xff);
			if (baseHeaderSize == 3)
			{
				dataSend.AppendByte(tmpChannel >> 8);
			}
		}
		//body just data
		dataSend.AppendByteArray(packet.body + bodyCur, lastDataSize);
		int sendSize = 0;
		char *pData = dataSend.getAmfData(sendSize);
		if (sendSize != socketSend(r.socketId, pData, sendSize))
		{
			return -1;
		}

		bodyCur += lastDataSize;

	}

	if (packet.packetType == RTMP_PACKET_TYPE_INVOKE)
	{
		RTMP_METHOD method;
		AMF_DecodeString(packet.body + 1, method.commandName);
		if (queue)
		{
			method.transactionId = (int)AMF_DecodeNumber(packet.body + 4 + method.commandName.size());
			r.methodCalls.push_back(method);
		}
	}

	r.mapChannelsOut[packet.channel] = packet;


	return 0;
}
#if RTMP_EPOLL

int RTMPClientSocketRecv(RTMP & r, char * buf, int len)
{
	int ret = RTMPReadBytes(&r.socketId, buf, len);
	if (ret == len)
	{
		r.bytesIn += ret;
		if (r.bytesIn > r.bytesInLastSend + r.selfBW / 10)
		{
			if (0 != RTMPSendBytesRead(r))
			{
				LOGE("Send bytes readed failed");
				return -1;
			}
		}
	}
	return ret;
}
#else
int RTMPClientSocketRecv(RTMP & r, char * buf, int len)
{
	int ret = socketRecvExactly(r.socketId, buf, len);
	if (ret == len)
	{
		r.bytesIn += ret;
		if (r.bytesIn > r.bytesInLastSend + r.selfBW / 10)
		{
			if (0 != RTMPSendBytesRead(r))
			{
				LOGE("Send bytes readed failed");
				return -1;
			}
		}
	}
	return ret;
}
#endif // RTMP_EPOLL

int RTMPRecvPacket(RTMP & r, RTMPPacket & packet_)
{
	int iRet = RTMPRecvPacketData(r, packet_);
	if (iRet != 0)
	{
		return iRet;
	}
	while (!RTMPPacketValid(packet_))
	{
		iRet = RTMPRecvPacketData(r, packet_);
		if (iRet != 0)
		{
			return iRet;
		}
	}
	return 0;
}

int RTMPRecvPacketData(RTMP & r, RTMPPacket & packet_)
{
	int iRet = 0;
	int recvRet = 0;
	int recvSize = 0;
	unsigned char tmp8 = 0;
	unsigned short tmp16 = 0;
	unsigned int tmp32 = 0;
	char tmpChArr[3];
	do
	{
		//处理packet
		if (packet_.body)
		{
			free(packet_.body);
			packet_.body = nullptr;
		}
		packet_.messageLength = 0;
		packet_.bytesRead = 0;
		//接收基本头
		recvSize = 1;
		recvRet = RTMPClientSocketRecv(r, (char*)&tmp8, recvSize);
		if (recvRet != recvSize)
		{
			LOGE("socket recv exactly failed:" << r.socketId);
			iRet = -1;
			break;
		}
		packet_.headerType = RTMP_HEADER_TYPE((tmp8 & 0xc0) >> 6);
		packet_.channel = tmp8 & 0x3f;
		if (0 == packet_.channel)
		{
			recvSize = 1;
			recvRet = RTMPClientSocketRecv(r, (char*)&tmp8, recvSize);
			if (recvRet != recvSize)
			{
				LOGE("socket recv exactly failed");
				iRet = -1;
				break;
			}
			packet_.channel = tmp8 + 64;
		}
		else if (1 == packet_.channel)
		{
			recvSize = 2;
			recvRet = RTMPClientSocketRecv(r, (char*)&tmp16, recvSize);
			if (recvRet != recvSize)
			{
				LOGE("socket recv exactly failed");
				iRet = -1;
				break;
			}
			tmp16 = (((tmp16 >> 8) & 0xFF) | ((tmp16 & 0xFF) << 8));
			packet_.channel = tmp16 + 64;
		}
		//如果包类型为3，寻找上次接收到的缓存，接着接收,或找到上次的in packet
		if (packet_.headerType == RTMP_HEADER_MINIMUM)
		{
			bool lastPacketRecvEnd = false;
			auto recvIt = r.mapRecvCache.find(packet_.channel);
			if (recvIt == r.mapRecvCache.end())
			{
				lastPacketRecvEnd = true;
			}
			if (lastPacketRecvEnd)
			{
				//这是一个新的最小包，根据inpacket的长度，类型，流ID,相对时间
				auto lastInfo = r.mapChannelsIn.find(packet_.channel);
				if (lastInfo == r.mapChannelsIn.end())
				{
					LOGE("get a rtmp packet type 3,but it is a new channel");
					iRet = -1;
					break;
				}
				packet_.hasAbsTimestamp = false;
				packet_.timestamp = r.mapChannelsIn[packet_.channel].timestamp;
				packet_.messageLength = r.mapChannelsIn[packet_.channel].messageLength;
				packet_.packetType = r.mapChannelsIn[packet_.channel].packetType;
				packet_.messageStreamId = r.mapChannelsIn[packet_.channel].messageStreamId;
				if (packet_.messageLength > r.chunkSize)
				{
					LOGF("a invalid message size for header type 3");
				}
				else
				{
					recvSize = packet_.messageLength;
					packet_.body = (char*)malloc(packet_.messageLength);
					recvRet = RTMPClientSocketRecv(r, packet_.body, recvSize);
					if (recvRet != recvSize)
					{
						LOGE("socket recv exactly failed");
						iRet = -1;
						break;
					}
					packet_.bytesRead = recvRet;
				}
			}
			else
			{
				//这是上一个消息的继续
				RTMPPacket &cachedPacket = r.mapRecvCache[packet_.channel];
				int lastedBytes = cachedPacket.messageLength - cachedPacket.bytesRead;
				if (lastedBytes > r.chunkSize)
				{
					recvSize = r.chunkSize;
				}
				else
				{
					recvSize = lastedBytes;
				}

				recvRet = RTMPClientSocketRecv(r, cachedPacket.body + cachedPacket.bytesRead, recvSize);
				if (recvRet != recvSize)
				{
					LOGE("socket recv exactly failed");
					iRet = -1;
					break;
				}
				cachedPacket.bytesRead += recvSize;
				//是否是一个完整的包
				//是：赋值给packet,移除接收缓存
				if (cachedPacket.bytesRead == cachedPacket.messageLength)
				{
					packet_ = cachedPacket;
					r.mapRecvCache.erase(packet_.channel);
					if (false == packet_.hasAbsTimestamp)
					{
						packet_.timestamp += r.mapChannelsIn[packet_.channel].timestamp;
					}
					//更新In
					r.mapChannelsIn[packet_.channel] = packet_;
				}
				//否：添加到接收缓存
				else
				{
					//已经在缓存中
					packet_.bytesRead = cachedPacket.bytesRead;
					packet_.messageLength = cachedPacket.messageLength;
				}
			}
		}
		//否则重新接收
		else
		{
			if (packet_.headerType == RTMP_HEADER_LARGE)
			{
				packet_.hasAbsTimestamp = true;
			}
			else
			{
				packet_.hasAbsTimestamp = false;
				//寻找上一个该channel的包
				auto lastIt = r.mapChannelsIn.find(packet_.channel);
				if (lastIt == r.mapChannelsIn.end())
				{
					LOGE("this channel :" << packet_.channel << "pkg not found");
					iRet = -1;
					break;
				}
				auto lastThisCahnnelPkt = r.mapChannelsIn[packet_.channel];
				packet_.messageStreamId = lastThisCahnnelPkt.messageStreamId;
				if (packet_.headerType == RTMP_HEADER_SMALL)
				{
					//length
					packet_.messageLength = lastThisCahnnelPkt.messageLength;
					//type
					packet_.packetType = lastThisCahnnelPkt.packetType;
				}
			}
			if (packet_.headerType == RTMP_HEADER_LARGE ||
				packet_.headerType == RTMP_HEADER_MEDIUM ||
				packet_.headerType == RTMP_HEADER_SMALL)
			{
				//timestamp
				char tmpChArr[3];
				recvSize = RTMPClientSocketRecv(r, tmpChArr, 3);
				if (recvSize != 3)
				{
					LOGE("recv timestamp failed");
					iRet = -1;
					break;
				}
				packet_.timestamp = AMF_DecodeInt24(tmpChArr);
			}
			if (packet_.headerType == RTMP_HEADER_LARGE ||
				packet_.headerType == RTMP_HEADER_MEDIUM)
			{
				//msg length
				recvSize = RTMPClientSocketRecv(r, tmpChArr, 3);
				if (recvSize != 3)
				{
					LOGE("recv msglength failed");
					iRet = -1;
					break;
				}
				packet_.messageLength = AMF_DecodeInt24(tmpChArr);
				if (0 == packet_.messageLength)
				{
					LOGF("why 0....message length");
				}
				//msg type
				recvSize = RTMPClientSocketRecv(r, (char*)&tmp8, 1);
				if (recvSize != 1)
				{
					LOGE("recv msg type failed");
					iRet = -1;
					break;
				}
				packet_.packetType = tmp8;
			}
			if (packet_.headerType == RTMP_HEADER_LARGE)
			{
				//stream id, little end
				char tmpChArr[4];
				recvSize = RTMPClientSocketRecv(r, tmpChArr, 4);
				if (recvSize != 4)
				{
					LOGE("recv stream id failed");
					iRet = -1;
					break;
				}
				packet_.messageStreamId = AMF_DecodeInt32LE(tmpChArr);
			}
			if (packet_.timestamp == 0xffffff)
			{
				if (4 != RTMPClientSocketRecv(r, (char*)&packet_.timestampExtension, 4))
				{
					LOGE("recv time extension failed");
					iRet = -1;
					break;
				}
				packet_.timestampExtension = htonl(packet_.timestampExtension);
			}
			
			packet_.body = (char*)malloc(packet_.messageLength);
			packet_.bytesRead = 0;
			if (r.chunkSize < packet_.messageLength - packet_.bytesRead)
			{
				recvSize = r.chunkSize;
			}
			else
			{
				recvSize = packet_.messageLength - packet_.bytesRead;
			}
			if (recvSize == 0)
			{
				recvSize = 0;
			}
			else
			{
				recvRet = RTMPClientSocketRecv(r, packet_.body + packet_.bytesRead, recvSize);
				if (recvRet != recvSize)
				{
					LOGE("recv body failed");
					iRet = -1;
					break;
				}
				packet_.bytesRead += recvRet;
			}
			//没有接收完整，添加到接收缓存
			if (packet_.bytesRead != packet_.messageLength)
			{
				r.mapRecvCache[packet_.channel] = packet_;
				//因为新的包会清空该数据的地址，所以新分配一份
				r.mapRecvCache[packet_.channel].body = (char*)malloc(packet_.messageLength);
				memcpy(r.mapRecvCache[packet_.channel].body, packet_.body, packet_.bytesRead);
			}
			else
			{
				if (false == packet_.hasAbsTimestamp)
				{
					packet_.timestamp += r.mapChannelsIn[packet_.channel].timestamp;
				}
				//更新In
				r.mapChannelsIn[packet_.channel] = packet_;
			}
		}
	} while (0);
	return iRet;
}

bool RTMPPacketValid(RTMPPacket & packet)
{
	if (packet.messageLength <= 0 || packet.bytesRead != packet.messageLength)
	{
		return false;
	}
	return true;
}

void RTMPPacketInfoCopy(const RTMPPacket & src, RTMPPacket & dst)
{
	dst.packetType = src.packetType;
	dst.timestamp = src.timestamp;
	dst.messageLength = src.messageLength;
	dst.messageStreamId = src.messageStreamId;
	dst.timestamp = src.timestamp;
	dst.timestampExtension = src.timestampExtension;
}

int RTMPSendCtrl(RTMP & r, RTMP_CONTROL_TYPE cType, unsigned int obj, unsigned int time)
{
	RTMPPacket packet;
	packet.channel = RTMP_channel_control;
	packet.packetType = RTMP_PACKET_TYPE_CONTROL;
	packet.headerType = RTMP_HEADER_LARGE;
	packet.timestamp = 0;
	packet.hasAbsTimestamp = false;
	packet.messageStreamId = 0;
	if (RTMP_CTRL_INVALID == cType)
	{
		LOGE("invalid ctrl type");
	}
	else if (cType == RTMP_CTRL_SET_BUFFERLENGHT)
	{
		amfEncoder encoder;
		encoder.EncodeInt16((const short)cType);
		encoder.EncodeInt32(obj);
		encoder.EncodeInt32(time);
		int size = 0;
		packet.body = encoder.getAmfData(size);
		packet.messageLength = size;

		if (0 != RTMPSendPacket(r, packet, false))
		{
			LOGE("RTMP send ctrl failed");
			return -1;
		}
	}
	else
	{
		amfEncoder encoder;
		encoder.EncodeInt16((const short)cType);
		encoder.EncodeInt32(obj);
		int size = 0;
		packet.body = encoder.getAmfData(size);
		packet.messageLength = size;

		if (0 != RTMPSendPacket(r, packet, false))
		{
			LOGE("RTMP send ctrl failed");
			return -1;
		}

	}
	return 0;
}

int RTMPSendAcknowledgementBW(RTMP & r)
{
	RTMPPacket packet;
	packet.channel = RTMP_channel_control;
	packet.packetType = RTMP_PACKET_TYPE_SERVER_BW;
	packet.headerType = RTMP_HEADER_LARGE;
	packet.timestamp = 0;
	packet.hasAbsTimestamp = true;
	packet.messageStreamId = 0;

	amfEncoder encoder;
	encoder.EncodeInt32(r.targetBW);
	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0 != RTMPSendPacket(r, packet, false))
	{
		LOGE("RTMP send server BW failed");
		return -1;
	}
	return 0;
}

int RTMPSendSetPeerBW(RTMP & r)
{
	RTMPPacket packet;
	packet.channel = RTMP_channel_control;
	packet.packetType = RTMP_PACKET_TYPE_CLIENT_BW;
	packet.headerType = RTMP_HEADER_LARGE;
	packet.timestamp = 0;
	packet.messageStreamId = 0;
	packet.hasAbsTimestamp = true;

	amfEncoder encoder;
	encoder.EncodeInt32(r.selfBW);
	encoder.AppendByte(r.selfBWlimitType);
	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;

	if (0 != RTMPSendPacket(r, packet, false))
	{
		LOGE("RTMP send client BW failed");
		return -1;
	}

	return 0;
}

int RTMPSendSetChunkSize(RTMP & r)
{
	RTMPPacket packet;
	packet.channel = RTMP_channel_control;
	packet.packetType = RTMP_PACKET_TYPE_CHUNK_SIZE;
	packet.headerType = RTMP_HEADER_LARGE;
	packet.timestamp = 0;
	packet.messageStreamId = 0;
	packet.hasAbsTimestamp = true;

	amfEncoder encoder;
	encoder.EncodeInt32(r.chunkSize);
	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0 != RTMPSendPacket(r, packet, false))
	{
		LOGE("rtmp send set chunk size failed");
		return -1;
	}
	return 0;
}

int RTMPSendFCSubscribe(RTMP & r, std::string path)
{
	RTMPPacket packet;
	packet.channel = RTMP_channel_invoke;
	packet.headerType = RTMP_HEADER_MEDIUM;
	packet.packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.timestamp = 0;
	packet.hasAbsTimestamp = false;
	packet.messageStreamId = 0;

	amfEncoder encoder;
	encoder.EncodeString(RTMP_CMD_FCSubscribe);
	encoder.EncodeNumber(++r.numInvokes);
	encoder.AppendByte(AMF_null);
	encoder.EncodeString(path);
	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0 != RTMPSendPacket(r, packet, true))
	{
		LOGE("RTMP send FCSubscribe failed");
		return -1;
	}
	return 0;
}

int RTMPSendCheckBW(RTMP & r)
{
	RTMPPacket packet;

	packet.channel = RTMP_channel_invoke;
	packet.packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.headerType = RTMP_HEADER_LARGE;
	packet.timestamp = 0;
	packet.hasAbsTimestamp = false;
	packet.messageStreamId = 0;

	amfEncoder encoder;
	encoder.EncodeString(RTMP_CMD__checkbw);
	encoder.EncodeNumber(++r.numInvokes);
	encoder.AppendByte(AMF_null);
	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0 != RTMPSendPacket(r, packet, false))
	{
		LOGE("send check bw failed");
		return -1;
	}
	return 0;
}

int RTMPSendCheckBWResult(RTMP & r, double transcationId)
{
	RTMPPacket packet;
	packet.channel = RTMP_channel_invoke;
	packet.headerType = RTMP_HEADER_MEDIUM;
	packet.packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.messageStreamId = 0;
	packet.hasAbsTimestamp = false;
	packet.timestamp = 0;//WTF?

	amfEncoder encoder;
	encoder.EncodeString(RTMP_CMD__result);
	encoder.EncodeNumber(transcationId);
	encoder.AppendByte(AMF_null);
	encoder.EncodeNumber(0);
	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0 != RTMPSendPacket(r, packet, false))
	{
		LOGE("RTMP send: checkbw result failed");
		return -1;
	}

	return 0;
}

int RTMPSendPause(RTMP & r, bool bPause, double milliSeconds)
{
	RTMPPacket packet;

	packet.channel = RTMP_channel_audioVideo;
	packet.packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.headerType = RTMP_HEADER_MEDIUM;
	packet.timestamp = 0;
	packet.messageStreamId = 0;
	packet.hasAbsTimestamp = false;

	amfEncoder encoder;
	encoder.EncodeString(RTMP_CMD_pause);
	encoder.EncodeNumber(++r.numInvokes);
	encoder.AppendByte(AMF_null);
	encoder.EncodeBool(bPause);
	encoder.EncodeNumber(milliSeconds);
	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0 != RTMPSendPacket(r, packet, true))
	{
		LOGE(r.link.tcUrl << "send pause failed");
		return -1;
	}

	return 0;
}

int RTMPSendBytesRead(RTMP & r)
{
	RTMPPacket packet;
	packet.channel = RTMP_channel_control;
	packet.headerType = RTMP_HEADER_MEDIUM;
	packet.packetType = RTMP_PACKET_BYTES_READ_REPORT;
	packet.timestamp = 0;
	packet.hasAbsTimestamp = false;
	packet.messageStreamId = 0;

	amfEncoder encoder;
	encoder.EncodeInt32(r.bytesIn);
	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	r.bytesInLastSend = r.bytesIn;
	if (0 != RTMPSendPacket(r, packet, false))
	{
		LOGE("rtmp send read report failed");
		return -1;
	}
	return 0;
}

int RTMPSendSeek(RTMP & r, double timeMillSec)
{
	RTMPPacket packet;
	packet.channel = RTMP_channel_audioVideo;
	packet.headerType = RTMP_HEADER_MEDIUM;
	packet.packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.timestamp = 0;
	packet.hasAbsTimestamp = false;
	packet.messageStreamId = 0;

	amfEncoder encoder;
	encoder.EncodeString(RTMP_CMD_seek);
	encoder.EncodeNumber(++r.numInvokes);
	encoder.AppendByte(AMF_null);
	encoder.EncodeNumber(timeMillSec);
	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0 != RTMPSendPacket(r, packet, true))
	{
		LOGE("rtmp send seek " << timeMillSec << " failed");
		return -1;
	}
	return 0;
}

void RTMPHandleChunkSize(RTMP & r, RTMPPacket & packet)
{
	if (packet.messageLength == 4)
	{
		r.chunkSize = AMF_DecodeInt32(packet.body);
	}
}

void RTMPHandleCtrl(RTMP & r, RTMPPacket & packet)
{
	RTMP_CONTROL_TYPE cType = RTMP_CTRL_INVALID;
	if (packet.messageLength >= 2)
	{
		cType = (RTMP_CONTROL_TYPE)AMF_DecodeInt16(packet.body);
	}
	if (packet.messageLength >= 6)
	{
		unsigned int tmp32;
		switch (cType)
		{
		case RTMP_CTRL_INVALID:
			LOGE(r.link.tcUrl << "\t recved invalid ctrl type");
			break;
		case RTMP_CTRL_STREAMBEGIN:
			tmp32 = AMF_DecodeInt32(packet.body + 2);
			LOGI("stream begin ,streamID:" << tmp32);
			break;
		case RTMP_CTRL_STREAMEOF:
			LOGW("rtmp ctrl " << cType << " not processed");
			//流结束
			break;
		case RTMP_CTRL_STREAMDRY:
			LOGW("rtmp ctrl " << cType << " not processed");
			break;
		case RTMP_CTRL_SET_BUFFERLENGHT:
			if (packet.messageLength != 10)
			{
				LOGE("invalid rtmp message:set bufferlength");
			}
			else
			{
				r.streamId = AMF_DecodeInt32(packet.body + 2);
				r.buffMS = AMF_DecodeInt32(packet.body + 4);
				LOGT("stream id:" << r.streamId << "\tbuffer length" << r.buffMS);
			}
			break;
		case RTMP_CTRL_STREAM_IS_RECORDED:
			LOGW("rtmp ctrl " << cType << " not processed");
			break;
		case RTMP_CTRL_PING_REQUEST:
			//ping
			tmp32 = AMF_DecodeInt32(packet.body + 2);

			RTMPSendCtrl(r, RTMP_CTRL_PING_RESPONSE, tmp32, 0);
			break;
		case RTMP_CTRL_PING_RESPONSE:
			LOGW("rtmp ctrl " << cType << " not processed");
			//pong
			break;
		case RTMP_CTRL_STREAM_BUFFER_EMPTY:
			LOGT("rtmp ctrl " << cType << " not processed");
			break;
		case RTMP_CTRL_STREAM_BUFFER_READY:
			LOGT("rtmp ctrl " << cType << " not processed");
			break;
		default:
			break;
		}
	}
}

void RTMPHandleAcknowledgementBW(RTMP & r, RTMPPacket & packet)
{
	r.targetBW = AMF_DecodeInt32(packet.body);
}

void RTMPHandleSetPeerBW(RTMP & r, RTMPPacket & packet)
{
	if (packet.messageLength >= 5)
	{
		r.selfBW = AMF_DecodeInt32(packet.body);
		r.selfBWlimitType = packet.body[4];
	}
	//RTMPSendAcknowledgementBW(r);
}

void RTMPHandleAudio(RTMP & r, RTMPPacket & packet)
{
}

void RTMPHandleVideo(RTMP & r, RTMPPacket & packet)
{
}

void RTMPHandleMetadata(RTMP & r, RTMPPacket & packet)
{
}

void RTMPHandleInvoke(RTMP & r, RTMPPacket & packet)
{
	AMFObject obj;
	int iRet = 0;
	if (packet.messageLength == 140)
	{
		iRet = 0;
	}
	iRet = AMF_Decode(&obj, packet.body, packet.messageLength, false);
	if (0 > iRet)
	{
		LOGE("decode amf object failed:" << packet.body);
		AMF_Clear(&obj);
		return;
	}
	std::string method;
	double transcationId;
	if (obj.objectProps.size() < 2)
	{
		LOGE("invalid object decoded");
		return;
	}
	
	method = AMF_GetPropByIndex(&obj, 0)->vu.strValue;
	transcationId = AMF_GetPropByIndex(&obj, 1)->vu.number;

	if (method == RTMP_CMD__result)
	{
		std::string methodInvoke;
		//移除对应指令
		for (auto i : r.methodCalls)
		{
			if (i.transactionId == transcationId)
			{
				methodInvoke = i.commandName;
				r.methodCalls.remove(i);
				break;
			}
		}
		if (0 == methodInvoke.size())
		{
			return;
		}
		if (methodInvoke == RTMP_CMD_connect)
		{
			//token ?
			//write push未处理 
			if (r.writeable)
			{
				LOGW("推流的连接返回还没有处理 ");
			}
			//read pull
			else
			{
				RTMPSendAcknowledgementBW(r);
				//buffer ms
				RTMPSendCtrl(r, RTMP_CTRL_SET_BUFFERLENGHT, 0, RTMP_default_buffer_ms);
			}
			//createStream
			RTMPSendCreateStream(r);
			//subscribe 可有可无？
			if (!r.writeable)
			{
				RTMPSendFCSubscribe(r, r.link.path);
			}
		}
		else if (RTMP_CMD_FCSubscribe == methodInvoke)
		{
			//不处理
			LOGI("get method invoke " << methodInvoke);
		}
		else if (RTMP_CMD_createStream == methodInvoke)
		{
			r.streamId = AMF_GetPropByIndex(&obj, 3)->vu.number;
			if (r.writeable)
			{
				LOGW("shuold send publish here,not coded now");
			}
			else
			{
				RTMPSendPlay(r);
			}
		}
		else
		{
			LOGW("not processed method:" << methodInvoke);
		}
	}
	else if (RTMP_CMD_onBWDone == method)
	{
		RTMPSendCheckBW(r);
	}
	else if (RTMP_CMD__onbwcheck == method)
	{
		RTMPSendCheckBWResult(r, transcationId);
	}
	else if (RTMP_CMD__onbwdone == method)
	{
		for (auto i : r.methodCalls)
		{
			if (RTMP_CMD__onbwdone == i.commandName)
			{
				r.methodCalls.remove(i);
				break;
			}
		}
	}
	else if (RTMP_CMD__error == method)
	{
		LOGE("RTMP error");
		if (obj.objectProps.size() >= 4 && AMF_GetPropByIndex(&obj, 3)->vu.object.objectProps.size() >= 2)
		{
			AMFObjectProperty *pEProp = AMF_GetPropByIndex(&obj, 3);
			if (pEProp)
			{
				LOGE(AMF_GetPropByIndex(&pEProp->vu.object, 2)->vu.strValue);
			}
		}
		std::string methodInvoke;
		//移除对应指令
		for (auto i : r.methodCalls)
		{
			if (i.transactionId == transcationId)
			{
				methodInvoke = i.commandName;
				r.methodCalls.remove(i);
				break;
			}
		}
		if (0 == methodInvoke.size())
		{
			return;
		}
	}
	else if (RTMP_CMD_onStatus == method)
	{
		std::string code, level, description;
		if (obj.objectProps.size() >= 4)
		{
			AMFObject objStatus;
			objStatus = AMF_GetPropByIndex(&obj, 3)->vu.object;
			//5个才对
			for (auto i : objStatus.objectProps)
			{
				if (RTMP_CMD_code == i->name)
				{
					code = i->vu.strValue;
				}
				else if (RTMP_CMD_level == i->name)
				{
					level = i->vu.strValue;
				}
				else if (RTMP_CMD_description == i->name)
				{
					description = i->vu.strValue;
				}
			}
			LOGI(code << " RTMP status:" << level);

		}

		//异常关闭rtmp连接
		if (code == RTMP_status_NetStream_Failed ||
			RTMP_status_NetStream_Play_Failed == code ||
			RTMP_status_NetStream_Play_StreamNotFound == code ||
			RTMP_status_NetConnection_Connect_InvalidApp == code ||
			RTMP_status_NetConnection_Connect_Closed == code ||
			RTMP_status_NetStream_Publish_Rejected == code ||
			RTMP_status_NetStream_Publish_Denied == code)
		{
			LOGE(r.link.tcUrl << ":" << code << "\t" << description);
			RTMPClose(r);
		}
		//停止play
		else if (RTMP_status_NetStream_Play_Complete == code ||
			RTMP_status_NetStream_Play_Stop == code ||
			RTMP_status_NetStream_Play_UnpublishNotify == code)
		{
			LOGI(r.link.tcUrl << ":play stoped");
			RTMPClose(r);
		}
		//开始play
		else if (RTMP_status_NetStream_Play_start == code ||
			RTMP_status_NetStream_Play_PublishNotify == code)
		{
			r.playing = true;
			for (auto i : r.methodCalls)
			{
				if (i.commandName == RTMP_CMD_play)
				{
					r.methodCalls.remove(i);
					break;
				}
			}
		}
		else if (RTMP_status_NetStream_Publish_Start == code)
		{
			r.playing = true;
			for (auto i : r.methodCalls)
			{
				if (RTMP_CMD_publish == i.commandName)
				{
					r.methodCalls.remove(i);
					break;
				}
			}
		}
		//reset
		else if (RTMP_status_NetStream_Play_Reset == code)
		{
			LOGI(r.link.tcUrl << ":reset");
		}
		//seek
		else if (RTMP_status_NetStream_Seek_Notify == code)
		{
			LOGI(r.link.tcUrl << ":seek successed");
		}
		//暂停
		else if (RTMP_status_NetStream_Pause_Notify == code)
		{
			LOGI(r.link.tcUrl << ":pause successed");
		}
		//其他
		else
		{
			LOGW("not processed:" << code);
		}

	}
	else if (RTMP_CMD_close == method)
	{
		LOGI("RTMP server send close...now close ");
		RTMPClose(r);
	}
	else
	{
		LOGW("not processed method " << method);
	}
	AMF_Clear(&obj);
	return;
}

bool RTMP_METHOD::operator==(const RTMP_METHOD &src) const
{
	return this->commandName == src.commandName;
}

RTMPPacket::RTMPPacket()
{
	hasAbsTimestamp = false;
	channel = 0;
	timestamp = 0;
	timestampExtension = 0;
	messageLength = 0;
	packetType = 0;
	messageStreamId = 0;
	bytesRead = 0;
	body = 0;
}

