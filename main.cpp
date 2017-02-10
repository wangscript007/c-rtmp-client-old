#include <stdio.h>
#include "log4z.h"
#include "RTMP/shakeHandle.h"
#include "RTMP/rtmp.h"
#include "socket_type.h"
#include "RTMP/rtmpFlvProducer.h"
#include <Windows.h>
#include <timeapi.h>
#pragma comment(lib,"winmm.lib")

extern bool g_bOpen;
extern bool g_bClose;

//const std::string rtmpUrl = "rtmp://live.hkstv.hk.lxdns.com/live/hks";
//const std::string rtmpUrl = "rtmp://127.0.0.1/live/test";
//const std::string rtmpUrl = "rtmp://127.0.0.1/vod/111";
const std::string rtmpUrl = "rtmp://127.0.0.1:2935/vod/test";
//const std::string rtmpUrl = "rtmp://124.205.16.52:6536/vod/111.flv";
int main(int argc, char **argv)
{
	socketInit();
	
	zsummer::log4z::ILog4zManager::getInstance()->start();
	
	int iRet = 0;
	iRet = 3191 & 0x0400;
	RTMP rtmp;
	RTMPReset(rtmp);
	//RTMPEnableWrite(rtmp);
	//rtmp.socketId = rtmpSockket;
	rtmp.chunkSize = RTMP_default_chunk_size;
	RTMPSetupUrl(rtmp, rtmpUrl);
	iRet = RTMPConnect(rtmp);

	iRet = RTMPConnectStream(rtmp, 0);
	//iRet = RTMPSendCreateStream(&rtmp);

	RTMPPacket packet;
	std::vector<int> time;
	int timeFirst = timeGetTime();

	std::string flvName = "rtmp2.flv";
	rtmpFlvProducer flvProducer(flvName);
	int aa = 0;
	/*g_bClose = true;
	g_bOpen = false;*/
	while (RTMPReadStreamData(rtmp,packet)>=0)
	{
		time.push_back(packet.timestamp);
		flvProducer.addRTMPPacket(packet);
		if (packet.timestamp>6000&&aa==0)
		{
			aa = 1;
			//RTMPSendSeek(rtmp, 0);
			//break;
		}
	}
	socketDestory();
	return 0;
}