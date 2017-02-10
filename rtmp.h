#pragma once
#include "AMF.h"
#include <vector>
#include <list>
#include <map>

const int RTMP_default_port = 1935;

const int RTMP_default_chunk_size = 128;	//默认块长度，不包括数据头，最小128，最大0xffff
const int RTMP_better_chunk_size = 0x1000;  //128太小
const int RTMP_max_chunk_size = 0xffff;
const int RTMP_buffer_cache_size = 16384;	//tcp缓存
const int RTMP_default_buffer_ms = 3000;

const int RTMP_channel_control = 0x02;
const int RTMP_channel_invoke = 0x03;
const int RTMP_channel_SendLive = 0x04;
const int RTMP_channel_SendPlayback = 0x05;
const int RTMP_channel_audioVideo = 0x08;

const std::string RTMP_CMD__result = "_result";
const std::string RTMP_CMD_connect = "connect";
const std::string RTMP_CMD_publish = "publish";
const std::string RTMP_CMD_pause = "pause";
const std::string RTMP_CMD_seek = "seek";
const std::string RTMP_CMD_createStream = "createStream";
const std::string RTMP_CMD_play = "play";
const std::string RTMP_CMD_FCSubscribe = "FCSubscribe";
const std::string RTMP_CMD_onBWDone = "onBWDone";
const std::string RTMP_CMD_onStatus = "onStatus";
const std::string RTMP_CMD_onFCSubscribe = "onFCSubscribe";
const std::string RTMP_CMD_onFCUnsubscribe = "onFCUnsubscribe";
const std::string RTMP_CMD__checkbw = "_checkbw";
const std::string RTMP_CMD__onbwcheck = "_onbwcheck";
const std::string RTMP_CMD__onbwdone = "_onbwdone";
const std::string RTMP_CMD__error = "_error";
const std::string RTMP_CMD_close = "close";
const std::string RTMP_CMD_code = "code";
const std::string RTMP_CMD_level = "level";
const std::string RTMP_CMD_description = "description";
const std::string RTMP_CMD_deleteStream = "deleteStream";
const std::string RTMP_CMD_playlist_ready = "playlist_ready";
const std::string RTMP_CMD_getStreamLength = "getStreamLength";

const std::string RTMP_status_NetStream_Failed = "NetStream.Failed";
const std::string RTMP_status_NetStream_Play_Failed = "NetStream.Play.Failed";
const std::string RTMP_status_NetStream_Play_StreamNotFound = "NetStream.Play.StreamNotFound";
const std::string RTMP_status_NetConnection_Connect_InvalidApp = "NetConnection.Connect.InvalidApp";
const std::string RTMP_status_NetConnection_Connect_Closed = "NetConnection.Connect.Closed";
const std::string RTMP_status_NetStream_Play_start = "NetStream.Play.Start";
const std::string RTMP_status_NetStream_Play_Complete = "NetStream.Play.Complete";
const std::string RTMP_status_NetStream_Play_Stop = "NetStream.Play.Stop";
const std::string RTMP_status_NetStream_Play_Reset = "NetStream.Play.Reset";
const std::string RTMP_status_NetStream_Seek_Notify = "NetStream.Seek.Notify";
const std::string RTMP_status_NetStream_Pause_Notify = "NetStream.Pause.Notify";
const std::string RTMP_status_NetStream_Play_PublishNotify = "NetStream.Play.PublishNotify";
const std::string RTMP_status_NetStream_Play_UnpublishNotify = "NetStream.Play.UnpublishNotify";
const std::string RTMP_status_NetStream_Publish_Start = "NetStream.Publish.Start";
const std::string RTMP_status_NetStream_Publish_Rejected = "NetStream.Publish.Rejected";
const std::string RTMP_status_NetStream_Publish_Denied = "NetStream.Publish.Denied";
const std::string RTMP_status_NetConnection_Connect_Rejected = "NetConnection.Connect.Rejected";

//rtmp://localhost:1935/testapp/instance1
const std::string connect_CMD_app = "app";	//rtmp url中端口后到下一个/为止的字符串，上面例子中的testapp
const std::string connect_CMD_flashver = "flashver";
const std::string connect_CMD_swfUrl = "swfUrl";
const std::string connect_CMD_tcUrl = "tcUrl";
const std::string connect_CMD_fpad = "fpad";
const std::string connect_CMD_capabilities = "capabilities";
const std::string connect_CMD_audioCodecs = "audioCodecs";
const std::string connect_CMD_videoCodecs = "videoCodecs";
const std::string connect_CMD_videoFunction = "videoFunction";

enum RTMP_PACKET_TYPE
{
	RTMP_PACKET_TYPE_CHUNK_SIZE         = 0x01,
	RTMP_PACKET_TYPE_ABORT              = 0x02,
	RTMP_PACKET_BYTES_READ_REPORT       = 0x03,
	RTMP_PACKET_TYPE_CONTROL            = 0x04,
	RTMP_PACKET_TYPE_SERVER_BW          = 0x05,
	RTMP_PACKET_TYPE_CLIENT_BW          = 0x06,
	RTMP_PACKET_TYPE_AUDIO              = 0x08,
	RTMP_PACKET_TYPE_VIDEO              = 0x09,
	RTMP_PACKET_TYPE_FLEX_STREAM_SEND   = 0x0F,	//amf3
	RTMP_PACKET_TYPE_FLEX_SHARED_OBJECT = 0x10,//amf3
	RTMP_PACKET_TYPE_FLEX_MESSAGE       = 0x11,		//amf3 message
	RTMP_PACKET_TYPE_INFO               = 0x12,//onMetaData info is sent as such
	RTMP_PACKET_TYPE_SHARED_OBJECT      = 0x13,
	RTMP_PACKET_TYPE_INVOKE             = 0x14,//amf0 message
	RTMP_PACKET_TYPE_FLASH_VIDEO        = 0x16
};

enum RTMP_CONTROL_TYPE
{
	RTMP_CTRL_INVALID             = -1,
	RTMP_CTRL_STREAMBEGIN         = 0,
	RTMP_CTRL_STREAMEOF           = 1,
	RTMP_CTRL_STREAMDRY           = 2,
	RTMP_CTRL_SET_BUFFERLENGHT    = 3,	//8字节  前4个字节表示流ID 后四个字节表示缓存长度
	RTMP_CTRL_STREAM_IS_RECORDED  = 4,
	RTMP_CTRL_PING_REQUEST        = 6,
	RTMP_CTRL_PING_RESPONSE       = 7,
	RTMP_CTRL_STREAM_BUFFER_EMPTY = 31,
	RTMP_CTRL_STREAM_BUFFER_READY = 32
};

struct RTMPChunkBasicHeader
{
	//大端
	unsigned char fmt;//包格式：2bit
	unsigned char csId;//块流ID channel id ：6bit.0表示2字节形式 1表示3字节形式 2被保留 3-63可直接写入该值
	//如果是控制消息，块流ID为2
	//StreamID=(ChannelID-4)/5+1
	//02 Ping 和ByteRead通道  control
	//03 Invoke通道 我们的connect() publish()和自字写的NetConnection.Call() 数据都是在这个通道的
	//04  Audio和Vidio通道 
	//05 06 07 服务器保留,经观察FMS2用这些Channel也用来发送音频或视频数据
	unsigned char csId2byte;//2字节形式，该字段的值为流ID-64；
	unsigned short csId3byte;//3字节形式，该字段的值为流ID-64;
};

enum RTMP_HEADER_TYPE
{
	RTMP_HEADER_LARGE,
	RTMP_HEADER_MEDIUM,
	RTMP_HEADER_SMALL,
	RTMP_HEADER_MINIMUM
};

//根据BasicHeader的fmt的值有四种格式
//类型0用在流起始位置或时间重置时
//类型1不包含消息流ID，与上一个块同样的流ID
//类型2不包含消息长度，与上一个块同样的流ID和流长度
//类型3没有任何字段
//chunk组装成消息,chunk的数据就是消息

struct RTMPMessageHeader
{
	unsigned char messageType;	//消息类型 1-6为控制消息 同chunk的messageType
	unsigned char payloadLength[3];//有效负载长度
	unsigned int timestamp;		//时间
	unsigned char streamId[3];	//消息流ID 协议控制消息时为0、chunk的流id为2。	音频为1 视频为2
};

struct RTMPPacket
{
	RTMPPacket();
	RTMP_HEADER_TYPE headerType;
	
	bool	hasAbsTimestamp;
	int channel;

	RTMPChunkBasicHeader basicHeader;
	unsigned int timestamp;				//0 1 2 0为时间 1、2为前一块和当前块的时间差，如果大于0xffffff，则这里等于0xffffff，表明用扩展时间 3byte
	unsigned int messageLength;			//0 1	消息长度		3byte
	unsigned char packetType;			//0 1	消息类型		1byte
	unsigned int  messageStreamId;			//0			小端。 4byte
	unsigned int timestampExtension;			//扩展时间
	unsigned int bytesRead;
	char		*body;
};

const std::string RTMP_PROTOCOL_RTMP = "rtmp";
const std::string RTMP_PROTOCOL_RTMPT = "rtmpt";
const std::string RTMP_PROTOCOL_RTMPS = "rtmps";
const std::string RTMP_PROTOCOL_RTMPE = "rtmpe";
const std::string RTMP_PROTOCOL_RTMPP = "rtmpp";
const std::string RTMP_PROTOCOL_RTMPTE = "rtmpte";
const std::string RTMP_PROTOCOL_RTMPTS = "rtmpts";

enum RTMP_protocol
{
	RTMP_protocol_rtmp,
	RTMP_protocol_rtmpt,
	RTMP_protocol_rtmps,
	RTMP_protocol_rtmpe,
	RTMP_protocol_rtmpp,
	RTMP_protocol_rtmpte,
	RTMP_protocol_rtmpts
};

struct RTMP_LINK
{
	RTMP_protocol protocol;
	std::string app;
	std::string flashver;
	std::string swfUrl;
	std::string tcUrl;
	std::string fpad;
	std::string audioCodecs;
	std::string videoCodecs;
	std::string videoFunctio;
	std::string path;
	std::string address;
	int seekTime;
	int stopTime;
	unsigned short port;
};

struct RTMP_METHOD
{
	bool operator ==(const RTMP_METHOD &)const;
	int transactionId;
	std::string commandName;
};

struct RTMP
{
	int socketId;
	int chunkSize;
	int numInvokes;
	int streamId;
	bool	writeable;
	bool	playing;
	RTMP_LINK link;
	int audioCodecs;
	int videoCodecs;
	int mediaChannel;
	std::map<int, RTMPPacket> mapChannelsOut;
	std::map<int, RTMPPacket> mapChannelsIn;
	std::map<int, RTMPPacket> mapRecvCache;//前两个cache都不用删除数据，这个cache如果存在，就说明有包没接收完，最后需释放
	int targetBW;//位宽,对方通知过来对方的BW
	int selfBW;//位宽，对方设置你的BW
	char selfBWlimitType;
	std::list<RTMP_METHOD> methodCalls;
	int bytesIn;
	int bytesInLastSend;
	int buffMS;
};

//对外接口
void RTMPReset(RTMP &r);
void RTMPEnableWrite(RTMP &r);
void RTMPClose(RTMP &r);
int RTMPSetupUrl(RTMP &r, const std::string &rtmpUrl);
int RTMPConnect(RTMP &r);
int RTMPConnectStream(RTMP &r, int seekTime);
int RTMPReadStreamData(RTMP &r, RTMPPacket &packet_);//失败返回-1
//对内
int RTMPShakeHandle(RTMP &r);
//error :-1 successed:0 
int RTMPClientProcessPacket(RTMP &r, RTMPPacket &packet);
int RTMPSendCreateStream(RTMP &r);
int RTMPSendPlay(RTMP &r);
int RTMPSendPacket(RTMP &r, RTMPPacket &packet, bool queue);
int RTMPClientSocketRecv(RTMP &r, char *buf, int len);
int RTMPRecvPacket(RTMP &r, RTMPPacket &packet_);
int RTMPRecvPacketData(RTMP &r, RTMPPacket &packet_);
bool RTMPPacketValid(RTMPPacket &packet);
void RTMPPacketInfoCopy(const RTMPPacket &src, RTMPPacket &dst);
int RTMPSendCtrl(RTMP &r, RTMP_CONTROL_TYPE cType, unsigned int obj, unsigned int time);
int RTMPSendAcknowledgementBW(RTMP &r);
int RTMPSendSetPeerBW(RTMP &r);
int RTMPSendSetChunkSize(RTMP &r);
int RTMPSendFCSubscribe(RTMP &r, std::string path);
int RTMPSendCheckBW(RTMP &r);
int RTMPSendCheckBWResult(RTMP &r, double transcationId);
int RTMPSendPause(RTMP &r, bool bPause, double milliSeconds);
int RTMPSendBytesRead(RTMP &r);
int RTMPSendSeek(RTMP &r, double timeMillSec);
//服务端发送响应
int RTMPSendConnectResult(RTMP &r, AMFObject *obj);
int RTMPSendOnBWDone(RTMP &r);
int RTMPSendCreateStreamResult(RTMP &r, AMFObject *obj);
int RTMPServerHandleCtrl(RTMP &r, RTMPPacket &packet);
int RTMPSendGetStreamLengthResult(RTMP &r, AMFObject *obj);
int RTMPSendStatus(RTMP &r, std::string &level, std::string &code, std::string &description, std::string &details, std::string &clientid, int channel = 5);
int RTMPSendBufferEmpty(RTMP &r, int timestamp);
int RTMPSendBufferReady(RTMP &r, int timestamp);

//处理各种类型的包
void RTMPHandleChunkSize(RTMP &r, RTMPPacket &packet);
void RTMPHandleCtrl(RTMP &r, RTMPPacket &packet);
void RTMPHandleAcknowledgementBW(RTMP &r, RTMPPacket &packet);
void RTMPHandleSetPeerBW(RTMP &r, RTMPPacket &packet);
void RTMPHandleAudio(RTMP &r, RTMPPacket &packet);
void RTMPHandleVideo(RTMP &r, RTMPPacket &packet);
void RTMPHandleMetadata(RTMP &r, RTMPPacket &packet);
void RTMPHandleInvoke(RTMP &r, RTMPPacket &packet);