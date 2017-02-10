#include "rtmp.h"
#include "amfEncoder.h"
#include "../util.h"
#include "../socket_type.h"
#include "../log4z.h"


int RTMPSendConnectResult(RTMP & r, AMFObject *obj)
{
	RTMPPacket packet;
	packet.channel = RTMP_channel_invoke;
	packet.headerType = RTMP_HEADER_LARGE;
	packet.packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.timestamp = 0;
	packet.hasAbsTimestamp = true;
	packet.messageStreamId = 0;

	double idx = AMF_GetPropByIndex(obj, 1)->vu.number;
	amfEncoder encoder;
	encoder.EncodeString("_result");
	encoder.EncodeNumber(idx);
	//cmd object
	encoder.AppendByte(AMF_object);
	encoder.EncodeNamedString("fmsVer", "FMS/5,0,3,3029");
	encoder.EncodeNamedNumber("capabilities", 255);
	encoder.EncodeNamedNumber("mode", 1);
	encoder.EncodeInt24(AMF_object_end);
	//parameters
	encoder.AppendByte(AMF_object);
	encoder.EncodeNamedString("level", "status");
	encoder.EncodeNamedString("code", "NetConnection.Connect.Success");
	encoder.EncodeNamedString("description", "Connection succeeded.");
	double objectEncodingNumber = 3.0;
	if (obj->objectProps.size()==3)
	{
		auto cmdObj = AMF_GetPropByIndex(obj, 2);
		for (auto i:cmdObj->vu.object.objectProps)
		{
			if (i->name=="objectEncoding")
			{
				objectEncodingNumber = i->vu.number;
				break;
			}
		}
	}
	encoder.EncodeNamedNumber("objectEncoding", objectEncodingNumber);
		//ecma array
		encoder.EncodeInt16(4);
		encoder.AppendByteArray("data", 4);
		encoder.AppendByte(AMF_ecma_array);
		encoder.EncodeInt32(0);
		encoder.EncodeNamedString("version", "5,0,3,3029");
		//encoder.AppendByte
		encoder.EncodeInt24(AMF_object_end);
		//ecma array end
	encoder.EncodeInt24(AMF_object_end);

	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0!=RTMPSendPacket(r,packet,false))
	{
		LOGE("send connect result failed");
		return -1;
	}
	return 0;
}

int RTMPSendOnBWDone(RTMP & r)
{
	RTMPPacket packet;
	packet.channel = RTMP_channel_invoke;
	packet.headerType = RTMP_HEADER_LARGE;
	packet.packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.timestamp = 0;
	packet.hasAbsTimestamp = true;
	packet.messageStreamId = 0;

	amfEncoder encoder;
	encoder.EncodeString(RTMP_CMD_onBWDone);
	encoder.EncodeNumber(0);
	encoder.AppendByte(AMF_null);

	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;

	

	if (0 != RTMPSendPacket(r, packet, false))
	{
		LOGE("send onBWDone result failed");
		return -1;
	}
	return 0;
}

int RTMPSendCreateStreamResult(RTMP & r, AMFObject * obj)
{
	RTMPPacket packet;

	packet.channel = RTMP_channel_invoke;
	packet.headerType = RTMP_HEADER_LARGE;
	packet.packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.timestamp = 0;
	packet.hasAbsTimestamp = true;
	packet.messageStreamId = 0;

	amfEncoder encoder;

	encoder.EncodeString(RTMP_CMD__result);
	double idx = AMF_GetPropByIndex(obj, 1)->vu.number;
	encoder.EncodeNumber(idx);
	encoder.AppendByte(AMF_null);
	encoder.EncodeNumber(1);

	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0 != RTMPSendPacket(r, packet, false))
	{
		LOGE("send createStream result failed");
		return -1;
	}
	return 0;
}

int RTMPServerHandleCtrl(RTMP & r, RTMPPacket & packet)
{
	unsigned short type;
	type = AMF_DecodeInt16(packet.body);
	if (3==type)
	{
		int streamId = AMF_DecodeInt32(packet.body + 2);
		int buffMS = AMF_DecodeInt32(packet.body + 6);
		r.buffMS = buffMS;
	}
	else if (7==type)
	{

	}
	else
	{
		LOGW("this type ctrl:" << type << " is unknown or should sended by svr");
	}
	return 0;
}

int RTMPSendGetStreamLengthResult(RTMP & r, AMFObject * obj)
{
	RTMPPacket packet;

	packet.channel = RTMP_channel_invoke;
	packet.headerType = RTMP_HEADER_LARGE;
	packet.packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.timestamp = 0;
	packet.hasAbsTimestamp = true;
	packet.messageStreamId = 0;

	amfEncoder encoder;

	encoder.EncodeString(RTMP_CMD__result);
	double idx = AMF_GetPropByIndex(obj, 1)->vu.number;
	encoder.EncodeNumber(idx);
	encoder.AppendByte(AMF_null);
	encoder.EncodeNumber(10);

	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0 != RTMPSendPacket(r, packet, false))
	{
		LOGE("send getStreamLength result failed");
		return -1;
	}
	return 0;
}

int RTMPSendStatus(RTMP &r, std::string &level, std::string &code, std::string &description, std::string &details, std::string &clientid,int channel)
{
	RTMPPacket packet;

	packet.channel = channel;
	packet.headerType = RTMP_HEADER_LARGE;
	packet.packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.timestamp = 0;
	packet.hasAbsTimestamp = true;
	packet.messageStreamId = 1;

	amfEncoder encoder;

	encoder.EncodeString(RTMP_CMD_onStatus);
	encoder.EncodeNumber(0.0);
	encoder.AppendByte(AMF_null);

	encoder.AppendByte(AMF_object);
	encoder.EncodeNamedString("level", level);
	encoder.EncodeNamedString("code", code);
	encoder.EncodeNamedString("description", description);
	encoder.EncodeNamedString("details", details);
	encoder.EncodeNamedString("clientid", clientid);
	encoder.EncodeInt24(AMF_object_end);

	AMFObject obj;
	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;


	AMF_Decode(&obj, packet.body, packet.messageLength, false);

	if (0 != RTMPSendPacket(r, packet, false))
	{
		LOGE("send on status failed");
		return -1;
	}
	return 0;
}

int RTMPSendBufferEmpty(RTMP & r, int timestamp)
{
	RTMPPacket packet;

	packet.channel = RTMP_channel_control;
	packet.headerType = RTMP_HEADER_LARGE;
	packet.packetType = RTMP_PACKET_TYPE_CONTROL;
	packet.timestamp = timestamp;
	packet.hasAbsTimestamp = true;
	packet.messageStreamId = 0;
	amfEncoder encoder;

	encoder.EncodeInt16(RTMP_CTRL_STREAM_BUFFER_EMPTY);
	encoder.EncodeInt32(1);

	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0 != RTMPSendPacket(r, packet, false))
	{
		LOGE("send buffer empty failed");
		return -1;
	}
	return 0;
}

int RTMPSendBufferReady(RTMP &r, int timestamp)
{
	RTMPPacket packet;

	packet.channel = RTMP_channel_control;
	packet.headerType = RTMP_HEADER_LARGE;
	packet.packetType = RTMP_PACKET_TYPE_CONTROL;
	packet.timestamp = timestamp;
	packet.hasAbsTimestamp = true;
	packet.messageStreamId = 0;
	amfEncoder encoder;

	encoder.EncodeInt16(RTMP_CTRL_STREAM_BUFFER_READY);
	encoder.EncodeInt32(1);

	int size = 0;
	packet.body = encoder.getAmfData(size);
	packet.messageLength = size;
	if (0 != RTMPSendPacket(r, packet, false))
	{
		LOGE("send buffer ready failed");
		return -1;
	}
	return 0;
}
