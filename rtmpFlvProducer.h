#pragma once
#include <string>
#include "rtmp.h"

class rtmpFlvProducer
{
public:
	rtmpFlvProducer(const std::string &fileName);
	rtmpFlvProducer(const rtmpFlvProducer&) = delete;
	rtmpFlvProducer &operator =(const rtmpFlvProducer&) = delete;
	bool addRTMPPacket(RTMPPacket &packet_);
protected:
	~rtmpFlvProducer();
private:
	void init();
	void shutdown();
	bool processMetaData(RTMPPacket &packet);
	bool processAudio(RTMPPacket &packet);
	bool processVideo(RTMPPacket &packet);
	bool processFlashVideo(RTMPPacket &packet);
	FILE *m_fp;
	std::string m_fileName;
	bool m_inited;
	bool m_metaDataGeted;
};

