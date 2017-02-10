#include "../socket_type.h"
#include "../util.h"
#include "../log4z.h"
#include  "rtmpFlvProducer.h"
#include "AMF.h"
#include "amfEncoder.h"

rtmpFlvProducer::rtmpFlvProducer(const std::string & fileName):m_fp(nullptr),m_fileName(fileName),m_inited(false),
m_metaDataGeted(false)
{
	init();
}

bool rtmpFlvProducer::addRTMPPacket(RTMPPacket & packet_)
{
	bool result = true;
	do
	{
		switch (packet_.packetType)
		{
		case RTMP_PACKET_TYPE_INFO:
			result = processMetaData(packet_);
			break;
		case RTMP_PACKET_TYPE_AUDIO:
			result = processAudio(packet_);
			break;
		case RTMP_PACKET_TYPE_VIDEO:
			result = processVideo(packet_);
			break;
		case RTMP_PACKET_TYPE_FLASH_VIDEO:
			result = processFlashVideo(packet_);
			break;
		default:
			break;
		}
	} while (0);
	if (packet_.body)
	{
		free(packet_.body);
		packet_.body = nullptr;
	}
	return result;
}

rtmpFlvProducer::~rtmpFlvProducer()
{
	shutdown();
}

void rtmpFlvProducer::init()
{
	if (m_fileName.size()==0)
	{
		return;
	}
	//创建文件
	if (0==access(m_fileName.c_str(),0))
	{
		LOGW(m_fileName << " is created");
	}
	m_fp = fopen(m_fileName.c_str(), "wb");
	if (!m_fp)
	{
		LOGE("create file:" << m_fileName << " failed");
		return;
	}
	m_inited = true;
}

void rtmpFlvProducer::shutdown()
{
	if (!m_inited)
	{
		return;
	}
	if (m_fp)
	{
		fclose(m_fp);
		m_fp = nullptr;
	}
}

bool rtmpFlvProducer::processMetaData(RTMPPacket & packet)
{
	AMFObject obj;
	bool result = true;
	do
	{
		if (!m_inited)
		{
			result = false;
			break;
		}
		if (0 > AMF_Decode(&obj, packet.body, packet.messageLength, false))
		{
			result = false;
			break;
		}
		if (obj.objectProps.size()>1)
		{
			if (AMF_GetPropByIndex(&obj, 0)->vu.strValue == "onMetaData")
			{
				m_metaDataGeted = true;
				AMFObject metaObj = AMF_GetPropByIndex(&obj, 1)->vu.object;
				unsigned char typeFlags = 0;
				unsigned char tmp8 = 0;
				unsigned int tmp32 = 0;
				//audo video
				for (auto i:metaObj.objectProps)
				{
					if (strncmp(i->name.c_str(),"audio",5)==0)
					{
						typeFlags |= 4;
					}
					if (strncmp(i->name.c_str(),"video",5)==0)
					{
						typeFlags |= 1;
					}
				}
				amfEncoder flvHeaderEncoder;
				//flv header
				flvHeaderEncoder.AppendByte('F');
				flvHeaderEncoder.AppendByte('L');
				flvHeaderEncoder.AppendByte('V');
				flvHeaderEncoder.AppendByte(1);//version 1
				flvHeaderEncoder.AppendByte(typeFlags);//type flags
				flvHeaderEncoder.EncodeInt32(9);//data offset
				//prev size
				flvHeaderEncoder.EncodeInt32(0);
				//tag
				flvHeaderEncoder.AppendByte(18);
				//size
				flvHeaderEncoder.EncodeInt24(packet.messageLength);
				//timestamp
				flvHeaderEncoder.EncodeInt24(packet.timestamp);
				flvHeaderEncoder.AppendByte(((packet.timestamp & 0xff000000) >> 24));
				//streamid
				flvHeaderEncoder.EncodeInt24(0);
				//metadata
				flvHeaderEncoder.AppendByteArray(packet.body, packet.messageLength);
				//prev size
				flvHeaderEncoder.EncodeInt32(11 + packet.messageLength);
				//write to file
				int dataSize;
				char *data = flvHeaderEncoder.getAmfData(dataSize);
				fwrite(data, dataSize, 1, m_fp);
			}
		}
	} while (0);
	AMF_Clear(&obj);
	return result;
}

bool rtmpFlvProducer::processAudio(RTMPPacket & packet)
{
	bool result = true;
	char *data_ = nullptr;
	do
	{
		if (!m_inited)
		{
			result = false;
			break;
		}
		data_ = new char[11 + packet.messageLength + 4];
		int cur = 0;
		//flv tag
		data_[cur++] = 0x8;
		amfEncoder::EncodeInt24(data_ + cur, packet.messageLength);
		cur += 3;
		amfEncoder::EncodeInt24(data_ + cur, packet.timestamp);
		cur += 3;
		data_[cur++] = (char)((packet.timestamp & 0xff000000) >> 24);
		amfEncoder::EncodeInt24(data_ + cur, 0);
		cur += 3;
		//audio tag 
		//data
		memcpy(data_ + cur, packet.body, packet.messageLength);
		cur += packet.messageLength;
		//prev size
		amfEncoder::EncodeInt32(data_ + cur, 11 + packet.messageLength);
		cur += 4;
		fwrite(data_, 11 + packet.messageLength + 4, 1, m_fp);
	} while (0);
	if (data_)
	{
		delete[]data_;
	}
	return result;
}

bool rtmpFlvProducer::processVideo(RTMPPacket & packet)
{
	bool result = true;
	do
	{
		char *data_ = nullptr;
		if (!m_inited)
		{
			result = false;
			break;
		}
		data_ = new char[11 + packet.messageLength + 4];
		int cur = 0;
		//flv tag
		data_[cur++] = 0x9;
		amfEncoder::EncodeInt24(data_ + cur, packet.messageLength);
		cur += 3;
		amfEncoder::EncodeInt24(data_ + cur, packet.timestamp);
		cur += 3;
		data_[cur++] = (char)((packet.timestamp & 0xff000000) >> 24);
		amfEncoder::EncodeInt24(data_ + cur, 0);
		cur += 3;
		//video tag 
		//data
		memcpy(data_ + cur, packet.body, packet.messageLength);
		cur += packet.messageLength;
		//prev size
		amfEncoder::EncodeInt32(data_ + cur, 11 + packet.messageLength);
		cur += 4;
		fwrite(data_, 11 + packet.messageLength + 4, 1, m_fp);
		if (data_)
		{
			delete[]data_;
		}
	} while (0);
	return result;
}

bool rtmpFlvProducer::processFlashVideo(RTMPPacket & packet)
{
	bool result = true;
	do
	{
		amfEncoder encoder;
		char streamType;
		int cur = 0;
		int mediaSize, time;
		int prevSize;
		time = AMF_DecodeInt24(packet.body + 4);
		time = (time | (packet.body[7] << 24));
		int timeDelta = packet.timestamp - time;
		while (cur<packet.messageLength)
		{
			streamType = packet.body[cur];
			cur++;
			mediaSize = AMF_DecodeInt24(packet.body + cur);
			cur += 3;
			time = AMF_DecodeInt24(packet.body + cur);
			cur += 3;
			time = (time | (packet.body[cur] << 24));
			cur += 1;
			cur += 3;
			time += timeDelta;
			if (RTMP_PACKET_TYPE_AUDIO!=streamType&&RTMP_PACKET_TYPE_VIDEO != streamType)
			{
				LOGF("this type:" << streamType << " in flash video?");
				result = false;
				break;
			}
			//tag
			encoder.AppendByte(streamType);
			encoder.EncodeInt24(mediaSize);
			encoder.EncodeInt24(time);
			encoder.AppendByte((time & 0xff000000) >> 24);
			encoder.EncodeInt24(0);
			//data
			encoder.AppendByteArray(packet.body + cur, mediaSize);
			cur += mediaSize;
			//prevsize
			encoder.EncodeInt32(mediaSize + 11);
			cur += 4;
		}
		if (false==result)
		{
			break;
		}
		else
		{
			int dataSize = 0;
			char *pData = encoder.getAmfData(dataSize);
			fwrite(pData, dataSize, 1, m_fp);
		}
	} while (0);
	return result;
}
