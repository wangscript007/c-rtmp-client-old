#include "handleShake.h"
#include "../socket_type.h"
#include "../util.h"
#include "../hmacsha256.h"
#include "../RTMPProcesser/RTMPServer.h"
#include "../log4z.h"

bool handleShake(int socketId)
{
	bool result = true;
	unsigned char *buf = nullptr;
	do
	{
		buf = new unsigned char[HANDLE_SHAKE_SIZE + 1];
#if RTMP_EPOLL
		if (HANDLE_SHAKE_SIZE + 1 != RTMPReadBytes(&socketId, (char*)buf, HANDLE_SHAKE_SIZE + 1))
#else
			if (HANDLE_SHAKE_SIZE + 1 != socketRecvExactly(socketId, (char*)buf, HANDLE_SHAKE_SIZE + 1))
#endif // 
		
			{
				result = false;
				break;
			}
		if (buf[0] != 0x03)
		{
			result = false;
			break;
		}
		unsigned int tmp32;
		memcpy(&tmp32, buf + 5, 4);
		tmp32 = htonl(tmp32);
		if (tmp32 == 0)
		{
			result = sampleHandleShake(socketId, buf + 1);
		}
		else
		{
			result = complexHandleShake(socketId, buf + 1);
		}
	} while (0);
	if (buf)
	{
		delete []buf;
		buf = nullptr;
	}
	return result;
}

bool sampleHandleShake(int socketId, unsigned char * input)
{
	bool result = true;
	unsigned char *s1 = nullptr;
	do
	{
		//send s0
		unsigned char tmp8 = 0x03;
		if (1 != socketSend(socketId, (const char*)&tmp8, 1))
		{
			result = false;
			break;
		}
		//gen s1
		s1 = new unsigned char[HANDLE_SHAKE_SIZE];
		memset(s1, 0, 8);
		for (int i = 8; i < HANDLE_SHAKE_SIZE; i++)
		{
			s1[i] = rand() % 256;
		}
		//send s1
		if (HANDLE_SHAKE_SIZE != socketSend(socketId, (const char*)s1, HANDLE_SHAKE_SIZE))
		{
			result = false;
			break;
		}
		//send s2=input
		if (HANDLE_SHAKE_SIZE != socketSend(socketId, (const char*)input, HANDLE_SHAKE_SIZE))
		{
			result = false;
			break;
		}
		//recv c2
#if RTMP_EPOLL
		if (HANDLE_SHAKE_SIZE  != RTMPReadBytes(&socketId, (char*)s1, HANDLE_SHAKE_SIZE))
#else
			if (HANDLE_SHAKE_SIZE != socketRecvExactly(socketId, (char*)s1, HANDLE_SHAKE_SIZE))
#endif // 
			{
				result = false;
				LOGF("recv c2 failed");
				break;
			}
	} while (0);
	if (s1)
	{
		delete []s1;
	}
	return result;
}

bool complexHandleShake(int socketId, unsigned char * input)
{
	bool result = true;
	unsigned char *challenge = nullptr;
	unsigned char *digest = nullptr;
	unsigned char *s1 = nullptr;
	unsigned char *s1p = nullptr;
	unsigned char *tmpHash = nullptr;
	unsigned char *randBytes = nullptr;
	unsigned char *lastHash = nullptr;
	unsigned char *c2 = nullptr;
	do
	{
		int scheme;
		challenge = new unsigned char[128];
		digest = new unsigned char[SHA256_DIGEST_SIZE];
		result = validClient(input, scheme, challenge, digest);
		if (!result)
		{
			break;
		}
		//s1
		s1 = new unsigned char[HANDLE_SHAKE_SIZE];
		result = createS1(s1);
		if (!result)
		{
			break;
		}
		int off = getDigestOffset(s1, scheme);
		s1p = new unsigned char[HANDLE_SHAKE_SIZE - SHA256_DIGEST_SIZE];
		memcpy(s1p, s1, off);
		memcpy(s1p + off, s1 + off + SHA256_DIGEST_SIZE, HANDLE_SHAKE_SIZE - SHA256_DIGEST_SIZE - off);
		tmpHash = new unsigned char[SHA256_DIGEST_SIZE];
		hmac_sha256(GENUINE_FMS_KEY, 36, s1p, HANDLE_SHAKE_SIZE - SHA256_DIGEST_SIZE, tmpHash, SHA256_DIGEST_SIZE);
		memcpy(s1 + off, tmpHash, SHA256_DIGEST_SIZE);
		//s2
		hmac_sha256(GENUINE_FMS_KEY, 68, digest, SHA256_DIGEST_SIZE, tmpHash, SHA256_DIGEST_SIZE);
		randBytes = new unsigned char[HANDLE_SHAKE_SIZE - SHA256_DIGEST_SIZE];
		result = createS2(randBytes);
		if (!result)
		{
			break;
		}
		lastHash = new unsigned char[SHA256_DIGEST_SIZE];
		hmac_sha256(tmpHash,
			SHA256_DIGEST_SIZE,
			randBytes,
			HANDLE_SHAKE_SIZE - SHA256_DIGEST_SIZE,
			lastHash,
			SHA256_DIGEST_SIZE);
		//send
		//s0 s1 s2
		unsigned char tmp8 = 0x03;
		if (1 != socketSend(socketId, (const char*)&tmp8, 1))
		{
			result = false;
			break;
		}
		if (HANDLE_SHAKE_SIZE != socketSend(socketId, (const char*)s1, HANDLE_SHAKE_SIZE))
		{
			result = false;
			break;
		}
		if (HANDLE_SHAKE_SIZE - SHA256_DIGEST_SIZE != socketSend(socketId, (const char*)randBytes, HANDLE_SHAKE_SIZE - SHA256_DIGEST_SIZE))
		{
			result = false;
			break;
		}
		if (SHA256_DIGEST_SIZE != socketSend(socketId, (const char*)lastHash, SHA256_DIGEST_SIZE))
		{
			result = false;
			break;
		}
		//recv c2
		c2 = new unsigned char[HANDLE_SHAKE_SIZE];
#if RTMP_EPOLL
		if (HANDLE_SHAKE_SIZE != RTMPReadBytes(&socketId, (char*)c2, HANDLE_SHAKE_SIZE))
#else
			if (HANDLE_SHAKE_SIZE != socketRecvExactly(socketId, (char*)c2, HANDLE_SHAKE_SIZE))
#endif // 
			{
				result = false;
				break;
			}
	} while (0);
	if (challenge)
	{
		delete []challenge;
		challenge = nullptr;
	}
	if (digest)
	{
		delete []digest;
		digest = nullptr;
	}
	if (s1)
	{
		delete []s1;
		s1 = nullptr;
	}
	if (s1p)
	{
		delete []s1p;
		s1p = nullptr;
	}
	if (tmpHash)
	{
		delete []tmpHash;
		tmpHash = nullptr;
	}
	if (randBytes)
	{
		delete []randBytes;
		randBytes = nullptr;
	}
	if (lastHash)
	{
		delete []lastHash;
		lastHash = nullptr;
	}
	if (c2)
	{
		delete []c2;
		c2 = nullptr;
	}
	return result;
}

bool createS2(unsigned char * output)
{
	for (int i = 0; i < HANDLE_SHAKE_SIZE - 32; i++)
	{
		output[i] = rand() % 256;
	}
	return true;
}

bool createS1(unsigned char * output)
{
	unsigned int tmp32 = 0;
	//time 
	memcpy(output, &tmp32, 4);
	//version
	tmp32 = htonl(0x5033029);
	//random
	for (int i = 8; i < HANDLE_SHAKE_SIZE; i++)
	{
		output[i] = rand() % 256;
	}
	return true;
}

bool validClient(unsigned char * input, int &scheme, unsigned char * challenge, unsigned char * digest)
{
	if (validClientScheme(input, 0, challenge, digest))
	{
		scheme = 0;
		return true;
	}
	else if (validClientScheme(input, 1, challenge, digest))
	{
		scheme = 1;
		return true;
	}
	return false;
}

bool validClientScheme(unsigned char * pBuffer, int  scheme, unsigned char * challenge, unsigned char * digest)
{
	bool result = true;
	unsigned char *p = nullptr;
	unsigned char *tmpHash = nullptr;
	do
	{
		int digestOffset = -1;
		int challengeOffset = -1;
		digestOffset = getDigestOffset(pBuffer, scheme);
		challengeOffset = getDHOffset(pBuffer, scheme);
		if (digestOffset == -1 || challengeOffset == -1)
		{
			result = false;
			break;
		}
		//p1+p2
		p = new unsigned char[HANDLE_SHAKE_SIZE - SHA256_DIGEST_SIZE];
		memcpy(digest, pBuffer + digestOffset, SHA256_DIGEST_SIZE);
		memcpy(p, pBuffer, digestOffset);
		memcpy(p + digestOffset,
			pBuffer + digestOffset + SHA256_DIGEST_SIZE,
			HANDLE_SHAKE_SIZE - digestOffset - SHA256_DIGEST_SIZE);

		tmpHash = new unsigned char[SHA256_DIGEST_SIZE];
		hmac_sha256(GENUINE_FP_KEY,
			30,
			p,
			HANDLE_SHAKE_SIZE - SHA256_DIGEST_SIZE,
			tmpHash,
			SHA256_DIGEST_SIZE);

		for (int i = 0; i < SHA256_DIGEST_SIZE; i++)
		{
			if (tmpHash[i] != digest[i])
			{
				result = false;
				break;
			}
		}
		memcpy(challenge, pBuffer + challengeOffset, 128);
	} while (0);
	if (p)
	{
		delete []p;
		p = nullptr;
	}
	if (tmpHash)
	{
		delete []tmpHash;
		tmpHash = nullptr;
	}
	return result;
}

int getDigestOffset(unsigned char * pBuffer, int scheme)
{
	if (scheme == 0)
	{
		return getDigestOffset0(pBuffer);
	}
	else if (scheme == 1)
	{
		return getDigestOffset1(pBuffer);
	}
	return -1;
}

int getDigestOffset0(unsigned char * pBuffer)
{
	int offset = 0;
	offset = (int)pBuffer[8] +
		(int)pBuffer[9] +
		(int)pBuffer[10] +
		(int)pBuffer[11];
	offset = (offset % 728) + 8 + 4;
	if (offset + 32 > 1536)
	{
		offset = -1;
	}
	return offset;
}

int getDigestOffset1(unsigned char * pBuffer)
{
	int offset = 0;
	offset = (int)pBuffer[772] +
		(int)pBuffer[773] +
		(int)pBuffer[774] +
		(int)pBuffer[775];
	offset = (offset % 728) + 772 + 4;
	if (offset + 32 > 1536)
	{
		offset = -1;
	}
	return offset;
}

int getDHOffset(unsigned char * handleShakeBytes, int scheme)
{
	if (scheme == 0)
	{
		return getDHOffset0(handleShakeBytes);
	}
	else if (scheme == 1)
	{
		return getDHOffset1(handleShakeBytes);
	}
	else
	{
		return -1;
	}
	return -1;
}

int getDHOffset0(unsigned char * HandleShakeBytes)
{
	int offset = 0;
	offset = (int)HandleShakeBytes[1532] +
		(int)HandleShakeBytes[1533] +
		(int)HandleShakeBytes[1534] +
		(int)HandleShakeBytes[1535];
	offset = (offset % 632) + 772;
	if (offset + 128 > 1536)
	{
		offset = -1;
	}
	return offset;
}

int getDHOffset1(unsigned char * HandleShakeBytes)
{
	int offset = 0;
	offset = (int)HandleShakeBytes[768] +
		(int)HandleShakeBytes[769] +
		(int)HandleShakeBytes[770] +
		(int)HandleShakeBytes[771];
	offset = (offset % 632) + 8;
	if (offset + 128 > 1536)
	{
		offset = -1;
	}
	return offset;
}
