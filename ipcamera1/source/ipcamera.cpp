#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common_basetypes.h"

#include "ipcamera.h"
#include "openRTSP.h"
#include "I13.h"

//add by liu 
#include <time.h>  


#ifdef __cplusplus
extern "C" {
#endif

#include "yzonvif.h"

#ifndef WIN32
#define INVALID_SOCKET	(-1)
#define SOCKET_ERROR	(-1)
#endif

unsigned int GetLocalIp();

#ifdef __cplusplus  
}
#endif

#include "ls_discovery.h"
#include <iostream>
using namespace std;

extern int Onvif_GetNetworkParam(ipc_unit *ipcam, ipc_neteork_para_t *pnw);
extern int Onvif_SetNetworkParam(ipc_unit *ipcam, ipc_neteork_para_t *pnw);

extern int Onvif_CMD_Open(int chn);
extern int Onvif_CMD_Close(int chn);
extern int Onvif_CMD_SetImageParam(int chn, video_image_para_t *para);
extern int Onvif_CMD_GetImageParam(int chn, video_image_para_t *para);
extern int Onvif_CMD_PtzCtrl(int chn, EMPTZCMDTYPE cmd, void* data);
extern int Onvif_CMD_SetTime(int chn, time_t t, int force);
extern int Onvif_CMD_Reboot(int chn);
extern int Onvif_CMD_RequestIFrame(int chn);

extern int IPC_URL(ipc_unit *ipcam, unsigned char stream_type, char *rtspURL, int *w, int *h);

//static pthread_mutex_t onvif_lock = PTHREAD_MUTEX_INITIALIZER;

#define ONVIF_LOCK()	//pthread_mutex_lock(&onvif_lock)
#define ONVIF_UNLOCK()	//pthread_mutex_unlock(&onvif_lock)

typedef struct
{
	ipc_unit cam;
	ipc_unit newcam;
	unsigned char changed;
	ipc_cmd_op_set ops;
	pthread_mutex_t lock;
}ipc_chn_info;

typedef struct
{
	real_stream_op_set ops;
	int video_width;
	int video_height;
	unsigned int frame_count;
	unsigned int last_frame_count;
	unsigned int link_failed_count;
	pthread_mutex_t lock;
	//add by liu
	unsigned int streamKbps;
}real_stream_info;

//add by liu
typedef struct
{
	unsigned long preTime;
	unsigned int preStreamLen;
	unsigned long totalStreamLen;
	unsigned int streamKbps;
}cnt_stream_info;

static cnt_stream_info g_stream_cnt[16];


static unsigned int g_chn_count = 0;

static ipc_chn_info *g_chn_info = NULL;
static real_stream_info *g_stream_info = NULL;

static unsigned char g_init_flag = 0;

static RealStreamCB g_pStreamCB = NULL;
static StreamStateCB g_pStateCB = NULL;

static pthread_t g_pid;

//add by liu
typedef struct recordStreamInfo{
	unsigned int preKbps;
	unsigned char Samenum;
}RECSTRINFO;
RECSTRINFO g_recordStreamInfo[16];

int is_ipcamera_same(ipc_unit *cam1, ipc_unit *cam2)
{
	if(cam1 == NULL || cam2 == NULL)
	{
		return 0;
	}
	if(cam1->channel_no != cam2->channel_no)
	{
		return 0;
	}
	if(cam1->enable != cam2->enable)
	{
		return 0;
	}
	if(cam1->protocol_type != cam2->protocol_type)
	{
		return 0;
	}
	if(cam1->trans_type != cam2->trans_type)
	{
		return 0;
	}
	if(cam1->dwIp != cam2->dwIp)
	{
		return 0;
	}
	if(cam1->wPort != cam2->wPort)
	{
		return 0;
	}
	if(strcmp(cam1->user, cam2->user))
	{
		return 0;
	}
	if(strcmp(cam1->pwd, cam2->pwd))
	{
		return 0;
	}
	if(strcmp(cam1->address, cam2->address))
	{
		return 0;
	}
	if(strcmp(cam1->uuid, cam2->uuid))
	{
		return 0;
	}
	return 1;
}


//static int zcm_find_flag = 0;

void *videoConnFxn(void *arg)
{
	printf("videoConnFxn, pid:%ld\n", pthread_self());
	
	unsigned int loop = 0;
	unsigned int n = 0;
	
	while(g_init_flag)
	{
		unsigned char reconn = 0;
		
		int i = 0;
		for(i = 0; i < (int)g_chn_count; i++)
		{
			ipc_unit ipcam;
			unsigned char changed = 0;
			
			pthread_mutex_lock(&g_chn_info[i].lock);
			
			ipcam = g_chn_info[i].newcam;
			changed = g_chn_info[i].changed;
			
			if(changed)
			{
				g_chn_info[i].changed = 0;
				if(!is_ipcamera_same(&ipcam, &g_chn_info[i].cam))
				{
					printf("videoConnFxn, chn%d param changed......\n", i);
					g_chn_info[i].cam = ipcam;
				}
				else
				{
					printf("videoConnFxn, chn%d param same......\n", i);
					changed = 0;
				}
			}
			
			pthread_mutex_unlock(&g_chn_info[i].lock);
			
			if(changed)
			{
				//printf("videoConnFxn, main chn%d stop\n",i);
				
				IPC_Stop(i);
				
				//printf("videoConnFxn, sub chn%d stop\n",i);
				
				IPC_Stop(i+g_chn_count);
				
				//printf("videoConnFxn, chn%d stop over\n",i);
				
				reconn = 1;
			}
		}
		
		usleep(100*1000);
		
		if(!reconn)
		{
			//if(++loop != 30)
			if(++loop != 50)
			{
				continue;
			}
			n++;
		}
		loop = 0;
		
		//int j = 0;
		//for(j = 0; j < 2; j++)
		{
			for(i = 0; i < (int)g_chn_count; i++)
			{
				ipc_unit *ipcam = &g_chn_info[i].cam;
				
				int j = 0;
				for(j = 0; j < 2; j++)
				{
					int channel = i + (j * g_chn_count);
					
					unsigned char byCurStreamOpen = 0;
					if(ipcam->enable)
					{
						byCurStreamOpen = 1;
					}
					
					if(!byCurStreamOpen)
					{
						g_stream_info[channel].link_failed_count = 0;
						
						if(IPC_GetLinkStatus(channel))
						{
							printf("chn%d stop-1...\n",channel);
							IPC_Stop(channel);
						}
					}
					else
					{
						//判断两个码流,两个profile,两个通道
						//有一个通道不行就ipc_start
						//这里改为只检查主码流
						if(!IPC_GetLinkStatus(channel))// && channel < 8)
						{
							g_stream_info[channel].link_failed_count++;
							
							if(g_stream_info[channel].link_failed_count > 20)
							{
								//g_stream_info[channel].link_failed_count = 0;
								g_stream_info[i].link_failed_count = 0;
								g_stream_info[i+g_chn_count].link_failed_count = 0;
								
								pthread_mutex_lock(&g_stream_info[channel].lock);
								
								char rtspURL[128] = {0};
								int w = 0;
								int h = 0;
								if(IPC_URL(ipcam, (channel < (int)g_chn_count)?STREAM_TYPE_MAIN:STREAM_TYPE_SUB, rtspURL, &w, &h) == 0)
								{
									g_stream_info[channel].video_width = 0;//w;
									g_stream_info[channel].video_height = 0;//h;
									
									pthread_mutex_unlock(&g_stream_info[channel].lock);
									
									printf("##########chn%d ipcamera reboot##########\n",i);
									
									IPC_CMD_Reboot(i);
								}
								else
								{
									pthread_mutex_unlock(&g_stream_info[channel].lock);
								}
							}
							else
							{
								//printf("videoConnFxn chn%d IPC_Start-1\n",channel);
								//if(zcm_find_flag == 0) {
								IPC_Start(channel);
								//zcm_find_flag =1;
								//}
								//printf("videoConnFxn chn%d IPC_Start-2\n",channel);
							}
						}
						else
						{
							g_stream_info[channel].link_failed_count = 0;
							
							if(n%3 == 0)
							{
								if(g_stream_info[channel].frame_count == g_stream_info[channel].last_frame_count)
								{
									if(g_stream_info[channel].frame_count)
									{
										printf("chn%d stop-2...\n",channel);
										IPC_Stop(channel);
									}
									else
									{
										if(n%30 == 0)
										{
											printf("chn%d stop-3...\n",channel);
											IPC_Stop(channel);
										}
									}
								}
								
								g_stream_info[channel].last_frame_count = g_stream_info[channel].frame_count;
							}
						}
					}
				}
				
				usleep(1);
			}
		}
	}
	
	return 0;
}

int IPC_Init(unsigned int chn_num)
{
	if(chn_num <= 0)
	{
		return -1;
	}
	
	if(g_init_flag)
	{
		return 0;
	}
	
	g_chn_count = chn_num;
	
	g_chn_info = (ipc_chn_info *)malloc(g_chn_count*sizeof(ipc_chn_info));
	if(g_chn_info == NULL)
	{
		return -1;
	}
	memset(g_chn_info,0,g_chn_count*sizeof(ipc_chn_info));
	
	g_stream_info = (real_stream_info *)malloc(g_chn_count*2*sizeof(real_stream_info));
	if(g_stream_info == NULL)
	{
		free(g_chn_info);
		g_chn_info = NULL;
		return -1; 
	}
	memset(g_stream_info,0,g_chn_count*2*sizeof(real_stream_info));

	
	int i = 0;
	//add by liu
	for(i=0;i<g_chn_count;i++)
	{
		memset(&g_stream_cnt[i],0,sizeof(cnt_stream_info));
		memset(&g_recordStreamInfo[i],0,sizeof(RECSTRINFO));
	}
	
	
	for(i = 0; i < (int)g_chn_count; i++)
	{
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE_NP);
		pthread_mutex_init(&g_chn_info[i].lock, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	for(i = 0; i < (int)(g_chn_count*2); i++)
	{
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE_NP);
		pthread_mutex_init(&g_stream_info[i].lock, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	
	RTSPC_Init(g_chn_count*2);
	I13_Init(g_chn_count*2);
	
	g_init_flag = 1;
	
	pthread_create(&g_pid, NULL, videoConnFxn, NULL);
	
	return 0;
}

int IPC_DeInit()
{
	return 0;
}

static unsigned char g_tz_index = 27;
static unsigned char g_sync_time = 0;

int IPC_SetTimeZone(int nTimeZone, int syncflag, int syncing)
{
	g_sync_time = syncflag;
	
	if(g_tz_index != nTimeZone)
	{
		g_tz_index = nTimeZone;
		
		if(g_sync_time && syncing)
		{
			int i;
			for(i=0;i<(int)g_chn_count;i++)
			{
				IPC_CMD_SetTime(i, 0, 0);
			}
		}
	}
	
	return g_tz_index;
}

int IPC_GetTimeZone()
{
	return g_tz_index;
}

int IPC_Set(int chn, ipc_unit *ipcam)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(chn < 0 || chn >= (int)g_chn_count)
	{
		return -1;
	}
	
	if(ipcam == NULL)
	{
		return -1;
	}
	
	//printf("chn%d set ipcam, locking...\n",chn);
	
	pthread_mutex_lock(&g_chn_info[chn].lock);
	
	//printf("chn%d set ipcam, start setting\n",chn);
	
	g_chn_info[chn].newcam = *ipcam;
	g_chn_info[chn].changed = 1;
	
	if(ipcam->protocol_type == PRO_TYPE_HAIXIN)
	{
		g_chn_info[chn].ops.Open = I13_CMD_Open;
		g_chn_info[chn].ops.Close = I13_CMD_Close;
		g_chn_info[chn].ops.SetImageParam = I13_CMD_SetImageParam;
		g_chn_info[chn].ops.GetImageParam = I13_CMD_GetImageParam;
		g_chn_info[chn].ops.PtzCtrl = I13_CMD_PtzCtrl;
		g_chn_info[chn].ops.SetTime = I13_CMD_SetTime;
		g_chn_info[chn].ops.Reboot = I13_CMD_Reboot;
		g_chn_info[chn].ops.RequestIFrame = I13_CMD_RequestIFrame;
	}
	else
	{
		g_chn_info[chn].ops.Open = Onvif_CMD_Open;
		g_chn_info[chn].ops.Close = Onvif_CMD_Close;
		g_chn_info[chn].ops.SetImageParam = Onvif_CMD_SetImageParam;
		g_chn_info[chn].ops.GetImageParam = Onvif_CMD_GetImageParam;
		g_chn_info[chn].ops.PtzCtrl = Onvif_CMD_PtzCtrl;
		g_chn_info[chn].ops.SetTime = Onvif_CMD_SetTime;
		g_chn_info[chn].ops.Reboot = Onvif_CMD_Reboot;
		g_chn_info[chn].ops.RequestIFrame = Onvif_CMD_RequestIFrame;
	}
	
	pthread_mutex_unlock(&g_chn_info[chn].lock);
	
	return 0;
}

int IPC_Get(int chn, ipc_unit *ipcam)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(chn < 0 || chn >= (int)g_chn_count)
	{
		return -1;
	}
	
	if(ipcam == NULL)
	{
		return -1;
	}
	
	pthread_mutex_lock(&g_chn_info[chn].lock);
	
	*ipcam = g_chn_info[chn].newcam;
	
	pthread_mutex_unlock(&g_chn_info[chn].lock);
	
	return 0;
}

int IPC_GetStreamResolution(int chn, int *w, int *h)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(chn < 0 || chn >= (int)(g_chn_count*2))
	{
		return -1;
	}
	
	if(w == NULL || h == NULL)
	{
		return -1;
	}
	
	*w = g_stream_info[chn].video_width;
	*h = g_stream_info[chn].video_height;
	
	return 0;
}

int IPC_SetStreamResolution(int chn, int w, int h)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(chn < 0 || chn >= (int)(g_chn_count*2))
	{
		return -1;
	}
	
	g_stream_info[chn].video_width = w;
	g_stream_info[chn].video_height = h;
	
	return 0;
}

int IPC_GetLinkStatus(int chn)
{
	if(!g_init_flag)
	{
		return 0;
	}
	
	if(chn < 0 || chn >= (int)(g_chn_count*2))
	{
		return 0;
	}
	
	int status = 0;
	
	pthread_mutex_lock(&g_stream_info[chn].lock);
	
	if(g_stream_info[chn].ops.GetLinkStatus != NULL)
	{
		status = g_stream_info[chn].ops.GetLinkStatus(chn);
	}
	
	pthread_mutex_unlock(&g_stream_info[chn].lock);
	
	return status;
}

int DoStreamStateCallBack(int chn, real_stream_state_e msg)
{
	if(g_pStateCB != NULL)
	{
		if(msg == REAL_STREAM_STATE_STOP || msg == REAL_STREAM_STATE_LOST)
		{
			if(chn < (int)(g_chn_count*2))
			{
				g_stream_info[chn].video_width = 0;
				g_stream_info[chn].video_height = 0;
			}
		}
		
		(g_pStateCB)(chn, msg);
	}
	
	return 0;
}

//add by liu chn:0-15
int Get_StreamKbps(unsigned int chn)
{
	unsigned int getKbps;
	if(chn >= g_chn_count)
	{
		printf("Get_Stream: chn 0-15 error\n");
		return -1;
	}

	pthread_mutex_lock(&g_stream_info[chn].lock);
	getKbps = g_stream_info[chn].streamKbps;
	pthread_mutex_unlock(&g_stream_info[chn].lock);

	if(0 == getKbps)
		return 0;

	if(0 == g_recordStreamInfo[chn].Samenum)
	{
		g_recordStreamInfo[chn].preKbps = getKbps;
		g_recordStreamInfo[chn].Samenum = 1;
		return (int)getKbps;
	}
	if(getKbps == g_recordStreamInfo[chn].preKbps)
	{
		g_recordStreamInfo[chn].Samenum++;
		if(g_recordStreamInfo[chn].Samenum >= 10) //说明通道已断开
		{
			pthread_mutex_lock(&g_stream_info[chn].lock);
			g_stream_info[chn].streamKbps = 0;
			pthread_mutex_unlock(&g_stream_info[chn].lock);
			getKbps = 0;
			g_recordStreamInfo[chn].preKbps = 0;
			g_recordStreamInfo[chn].Samenum = 0;
		}
	}
	else
	{
		g_recordStreamInfo[chn].Samenum = 0;

	}

	return (int)getKbps;
}

int DoRealStreamCallBack(real_stream_s *stream, unsigned int dwContext)
{
	if(stream == NULL || stream->chn != (int)dwContext)
	{
		printf("DoRealStreamCallBack: param error-1\n");
		return -1;
	}
	
	int chn = stream->chn;
	if(chn < 0 || chn >= (int)(g_chn_count*2))
	{
		printf("DoRealStreamCallBack: param error-2\n");
		return -1;
	}
	
	//add by liu
	if(chn < g_chn_count) //统计主码流
	{
		if(0 == g_stream_cnt[chn].preTime)
		{
			g_stream_cnt[chn].preTime = time(NULL);
			g_stream_cnt[chn].preStreamLen += stream->len;
			g_stream_cnt[chn].totalStreamLen += stream->len;
		}
		else
		{
			int nowTime = 0;
			double diftime = 0;
			nowTime = time(NULL);
			 
			g_stream_cnt[chn].totalStreamLen += stream->len;
			//得到两次机器时间差，单位为秒
			diftime = difftime(nowTime,g_stream_cnt[chn].preTime);
			if(diftime >= 3.0)
			{
				int StreamLen = 0;
				StreamLen = g_stream_cnt[chn].totalStreamLen - g_stream_cnt[chn].preStreamLen;
				g_stream_cnt[chn].streamKbps = (unsigned int)((double)StreamLen*8/(1024*diftime));
				g_stream_cnt[chn].preTime = nowTime;
				g_stream_cnt[chn].preStreamLen = g_stream_cnt[chn].totalStreamLen;
				//printf("DoRealStreamCallBack: stream[%d]-Kbps:%d\n",chn,g_stream_cnt[chn].streamKbps);

				pthread_mutex_lock(&g_stream_info[chn].lock);
				g_stream_info[chn].streamKbps = g_stream_cnt[chn].streamKbps;		
				pthread_mutex_unlock(&g_stream_info[chn].lock);
				
			}
		}
	}

	
	//printf("DoRealStreamCallBack: here...\n");
	
	g_stream_info[chn].frame_count++;
	
	if(g_pStreamCB != NULL)
	{
		return (g_pStreamCB)(stream, dwContext);
	}
	
	return 0;
}

int IPC_URL(ipc_unit *ipcam, unsigned char stream_type, char *rtspURL, int *w, int *h)
{
	
	if(ipcam == NULL || rtspURL == NULL || w == NULL || h == NULL)
	{
		return -1;
	}
	
	*w = 0;
	*h = 0;
	
	
	int bFind = FALSE;
	
	ONVIF_LOCK();
	
#if 1
	
	discovery_device *receiver_onvif = new device_ptz();
	if(NULL == receiver_onvif)
	{
		ONVIF_UNLOCK();
		return -1;
	}
	
	
	struct in_addr serv;
	serv.s_addr = ipcam->dwIp;
	char ipaddr[32] = {0};
	strcpy(ipaddr, inet_ntoa(serv));
	receiver_onvif->set_ip(ipaddr, strlen(ipaddr));
	
	receiver_onvif->set_xaddrs(ipcam->address, strlen(ipcam->address));
	
	((device_ptz*)receiver_onvif)->set_username_password(ipcam->user,ipcam->pwd);


	cout<<receiver_onvif->get_xaddrs()<<endl;
	
	//这个函数里面也先得到了capabilities
	//这么快就得到media profile ?  要先配置一下吧!
	vector<media_profile *> *pMediaProfileVector = ((device_ptz*)receiver_onvif)->get_profiles();


	//配置analytics , rules , 再从rule  engine  模块中得到移动侦测数据
//	((device_ptz*)receiver_onvif)->DefineAnalytics();


	//得到数据要配置profile，先看video analytics 和metadata的配置,不需要了
	//((device_ptz*)receiver_onvif)->MediaProfileConfig();
	

//	((device_ptz*)receiver_onvif)->GetEventProperties();
//	((device_ptz*)receiver_onvif)->SubscribeEvent();

//	while(1) {
//		((device_ptz*)receiver_onvif)->PullMessages();
//		sleep(10);
//	}
	
	if(pMediaProfileVector == NULL || pMediaProfileVector->empty())
	{
		printf("pMediaProfileVector is empty\n");
		
		delete receiver_onvif;
		ONVIF_UNLOCK();
		return -1;
	}
	
	if(pMediaProfileVector != NULL)
	{
		vector<media_profile *>::iterator media_it = pMediaProfileVector->begin();
		
		int idx = 0;

		//打印出各个media profile  的信息?
		while(media_it != pMediaProfileVector->end())
		{
			printf("stream%d token:%s w:%d h:%d %s %s\n",
				idx,(*media_it)->stream_token,(*media_it)->resolution_width,(*media_it)->resolution_height,
				(*media_it)->image_token,(*media_it)->ptz_token);
			idx++;
			media_it++;
		}
	}

	//由各个media profile 得到stream URI
	vector<std::string *> *pVector = ((device_ptz*)receiver_onvif)->get_stream_uri();
	
	
	if(pVector == NULL || pVector->empty())
	{
		printf("IPC_URL-4.3.1\n");
		
		delete receiver_onvif;
		ONVIF_UNLOCK();
		return -1;
	}
	
	//多个Stream URI
	vector<std::string *>::iterator uri_it = pVector->begin();
	vector<media_profile *>::iterator media_it = pMediaProfileVector->begin();
	
	int idx = 0;
	while(uri_it != pVector->end())
	{
		
		if(idx == stream_type)
		{
			strcpy(rtspURL, (*uri_it)->c_str());
			
			if(strstr(rtspURL, "jpeg") || strstr(rtspURL, "jpg"))
			{
				idx--;
			}
			else if(strncasecmp(rtspURL, "rtsp://", strlen("rtsp://")) != 0)
			{
				idx--;
			}
			else
			{
				bFind = TRUE;
				
				if(media_it != pMediaProfileVector->end())
				{
					*w = (*media_it)->resolution_width;
					*h = (*media_it)->resolution_height;
					//printf("ipc stream type:%d,url:%s,w:%d,h:%d\n",stream_type,rtspURL,*w,*h);
				}
				
				break;
			}
		}
		
		idx++;
		uri_it++;
		media_it++;
	}
	
	//printf("IPC_URL-6\n");
	
	delete receiver_onvif;


	ONVIF_UNLOCK();
	
	if(!bFind)
	{
		return -1;
	}
	
	return 0;
	
	
#else
	DeviceAddrs device; //one device address which we want to get its media address and ptz address
	device.addr = ipcam->address;
	
	MediaPtzAddr media_ptz;
	memset(&media_ptz, 0, sizeof(media_ptz));
	
	printf("############IPC [%s] [%s:%s] get media addr...\n", device.addr, ipcam->user, ipcam->pwd);
	
	if(!GetMediaPtzAddr(ipcam->user, ipcam->pwd, &device, &media_ptz))
	{
		printf("media profile:%s\n", media_ptz.media_addr);
		//printf("ptz profile:%s\n", media_ptz.ptz_addr);
		
		int j = 0;
		
		/* get profiletoken */
		ProfileToken profile;
		memset(&profile, 0, sizeof(profile));
		GetProfileToken(ipcam->user, ipcam->pwd, &media_ptz, &profile);
		printf("token num:%d\n", profile.len);
		for(j = 0; j < profile.len; j++)
		{
			//printf("token[%d] : %s\n", j, profile.token[j]);
		}
		
		/* get stream uri */
		//media->addr and profileToken are needed
		MediaUri uri;
		memset(&uri, 0, sizeof(uri));
		GetStreamUri(ipcam->user, ipcam->pwd, &media_ptz, &profile, &uri);
		printf("url num:%d\n", uri.len);
		for(j = 0; j < uri.len; j++)
		{
			//printf("url[%d] : %s\n", j, uri.uri[j]);
			
			if(j == stream_type)
			{
				strcpy(rtspURL, uri.uri[j]);
				printf("ipc stream type:%d,url:%s\n",stream_type,rtspURL);
				
				if(strncasecmp(rtspURL, "rtsp://", strlen("rtsp://")) == 0)
				{
					bFind = TRUE;
				}
				
				break;
			}
		}
		
		DestoryProfileToken(&profile);
		DestoryMediaUri(&uri);
		
		if(!bFind)
		{
			//printf("IPC [%s] get media addr failed\n", device.addr);
		}
	}
	else
	{
		printf("IPC [%s] get media addr error\n", device.addr);
	}
	
	DestoryMediaAddr(&media_ptz);
	
	printf("@@@@@@@@@@@@IPC [%s] get media addr over:%s\n", device.addr, bFind?"success":"failed");
#endif
	
	
}

int IPC_Start(int chn)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	
	if(chn < 0 || chn >= (int)(g_chn_count*2))
	{
		return -1;
	}
	
	ipc_unit *ipcam = NULL;
	if(chn < (int)g_chn_count)
	{
		ipcam = &g_chn_info[chn].cam;
	}
	else
	{
		ipcam = &g_chn_info[chn-g_chn_count].cam;
	}
	
	//printf("IPC_Start: chn%d enable:%d protocol_type:%d\n", chn, ipcam->enable, ipcam->protocol_type);
	
	if(!ipcam->enable)
	{
		return -1;
	}
	
	int ret = -1;
	
	//printf("IPC_Start: chn%d protocol_type:%d\n", chn, ipcam->protocol_type);
	
	pthread_mutex_lock(&g_stream_info[chn].lock);
	
	//printf("IPC_Start - 10\n");
	
	g_stream_info[chn].frame_count = 0;
	g_stream_info[chn].last_frame_count = 0;
	
	if(ipcam->protocol_type == PRO_TYPE_ONVIF)
	{
		g_stream_info[chn].ops.Start = RTSPC_Start;
		g_stream_info[chn].ops.Startbyurl = RTSPC_Startbyurl;
		g_stream_info[chn].ops.Stop = RTSPC_Stop;
		g_stream_info[chn].ops.GetLinkStatus = RTSPC_GetLinkStatus;
		
		char rtspURL[128] = {0};
		int w = 0;
		int h = 0;
		
		if(IPC_URL(ipcam, (chn < (int)g_chn_count)?STREAM_TYPE_MAIN:STREAM_TYPE_SUB, rtspURL, &w, &h) == 0)
		{
			g_stream_info[chn].video_width = w;
			g_stream_info[chn].video_height = h;
			
			//IPC_CMD_SetTime(chn, 0, 1);	
			
			ret = RTSPC_Startbyurl(chn, DoRealStreamCallBack, chn, rtspURL, ipcam->user, ipcam->pwd, ipcam->trans_type);	
		}
		else
		{
			//printf("IPC_Start - 15\n");
		}
	}
	else if(ipcam->protocol_type == PRO_TYPE_HAIXIN)
	{
		g_stream_info[chn].ops.Start = I13_Start;
		g_stream_info[chn].ops.Startbyurl = NULL;
		g_stream_info[chn].ops.Stop = I13_Stop;
		g_stream_info[chn].ops.GetLinkStatus = I13_GetLinkStatus;
		
		IPC_CMD_SetTime(chn, 0, 1);
		
		ret = I13_Start(chn, DoRealStreamCallBack, chn, NULL, ipcam->dwIp, ipcam->wPort, ipcam->user, ipcam->pwd, ipcam->trans_type);
	}
	else
	{
		g_stream_info[chn].ops.Start = NULL;
		g_stream_info[chn].ops.Startbyurl = NULL;
		g_stream_info[chn].ops.Stop = NULL;
		g_stream_info[chn].ops.GetLinkStatus = NULL;
	}
	
	pthread_mutex_unlock(&g_stream_info[chn].lock);
	
	if(ret == 0)
	{
		if(g_pStateCB != NULL)
		{
			(g_pStateCB)(chn, REAL_STREAM_STATE_START);
		}
	}
	
	//printf("IPC_Start over\n");
	
	return ret;
}

int IPC_Stop(int chn)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	//printf("IPC_Stop: chn=%d\n", chn);
	
	if(chn < 0 || chn >= (int)(g_chn_count*2))
	{
		return -1;
	}
	
	//printf("IPC_Stop: chn%d lock...\n", chn);
	
	pthread_mutex_lock(&g_stream_info[chn].lock);
	
	//printf("IPC_Stop: chn%d stop...\n", chn);
	
	if(g_stream_info[chn].ops.Stop != NULL)
	{
		g_stream_info[chn].ops.Stop(chn);
	}
	
	g_stream_info[chn].ops.Start = NULL;
	g_stream_info[chn].ops.Startbyurl = NULL;
	g_stream_info[chn].ops.Stop = NULL;
	g_stream_info[chn].ops.GetLinkStatus = NULL;
	
	g_stream_info[chn].frame_count = 0;
	g_stream_info[chn].last_frame_count = 0;
	
	pthread_mutex_unlock(&g_stream_info[chn].lock);
	
	//printf("IPC_Stop: chn%d over...\n", chn);
	
	if(g_pStateCB != NULL)
	{
		(g_pStateCB)(chn, REAL_STREAM_STATE_STOP);
	}
	
	//printf("IPC_Stop over\n");
	
	return 0;
}

int IPC_RegisterCallback(RealStreamCB pStreamCB, StreamStateCB pStateCB)
{
	g_pStreamCB = pStreamCB;
	g_pStateCB = pStateCB;
	return 0;
}

int IPC_Find(ipc_node* head, ipc_node *pNode)
{
	if(head == NULL || pNode == NULL)
	{
		return 0;
	}
	
	if(pNode->ipcam.protocol_type == PRO_TYPE_ONVIF)
	{
		ipc_node *p = head;
		while(p)
		{
			if(p->ipcam.protocol_type != PRO_TYPE_ONVIF && p->ipcam.dwIp == pNode->ipcam.dwIp)
			{
				return 1;
			}
			p = p->next;
		}
	}
	else
	{
		ipc_node *p = head;
		while(p)
		{
			if(p->ipcam.dwIp == pNode->ipcam.dwIp)
			{
				if((p->ipcam.protocol_type == pNode->ipcam.protocol_type) && (strcmp(p->ipcam.uuid, pNode->ipcam.uuid) == 0))
				{
					return 2;
				}
				else
				{
					return 1;
				}
			}
			p = p->next;
		}
	}
	
	return 0;
}

/*
wsdd:type:tdn:NetworkVideoTransmitter
wsdd:uuid:urn:uuid:5f5a69c2-e0ae-504f-829b-000389111004
http://192.168.1.189:8080/onvif/device_service

wsdd:type:tdn:NetworkVideoTransmitter
wsdd:uuid:urn:uuid:11223344-5566-7788-99aa-003e0b04015d
http://192.168.1.11:8899/onvif/device_service
*/
//搜索网络摄像机
int IPC_Search(ipc_node** head, unsigned int protocol_type, unsigned char check_conflict)
{
	if(head == NULL)
	{
		return -1;
	}
	
	*head = NULL;
	ipc_node *tail = NULL;
	
	int count = 0;
	
	int i = 0;

	
	if(protocol_type & PRO_TYPE_HAIXIN)
	{
		unsigned char conflict_flag = 0;
		
		struct sockaddr_in local, remote, from;
		
		int sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
		if(sock == -1)
		{
			printf("Search IPNC:socket error\n");
			return -1;
		}
		
		int reuseflag = 1;
		if(setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char *)&reuseflag,sizeof(reuseflag)) == SOCKET_ERROR) 
		{
			printf("Search IPNC:port mulit error\n");
			close(sock);
			return -1;
		}
		
		u32 dwInterface = INADDR_ANY;
		u32 dwMulticastGroup = inet_addr(MCASTADDR);
		u16 iPort = MCASTPORT;
		
		local.sin_family = AF_INET;
		local.sin_port   = htons(iPort);
		local.sin_addr.s_addr = dwInterface;
		
		if(bind(sock, (struct sockaddr *)&local, sizeof(local)) == SOCKET_ERROR)
	    {
			printf("Search IPNC:bind error\n");
			close(sock);
			return -1;
		}
		
		struct ip_mreq mreq;
		memset(&mreq,0,sizeof(struct ip_mreq));
		mreq.imr_multiaddr.s_addr = inet_addr(MCASTADDR);
		mreq.imr_interface.s_addr = GetLocalIp();
		if(setsockopt(sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,(const char*)&mreq,sizeof(mreq)) == SOCKET_ERROR)
		{
			printf("Search IPNC:join mulit error\n");
			close(sock);
			return -1;
		}
		
		int optval = 0;
		if(setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&optval, sizeof(optval)) == SOCKET_ERROR)
		{
			printf("setsockopt(IP_MULTICAST_LOOP) failed\n");
			close(sock);
			return 0;
		}
		
		char recvbuf[BUFSIZE], sendbuf[BUFSIZE];
		
		remote.sin_family      = AF_INET;
		remote.sin_port        = htons(iPort);
		remote.sin_addr.s_addr = dwMulticastGroup;
		
		for(i = 0; i < 1; i++)
		{
			static unsigned int seq_count = 0;
			seq_count++;
			snprintf(sendbuf, BUFSIZE, "SEARCH * HDS/1.0\r\n"
										"CSeq:%u\r\n"
										"Client-ID:nvmOPxEnYfQRAeLFdsMrpBbnMDbEPiMC\r\n"
										"Accept-Type:text/HDP\r\n"
										"Content-Length:0\r\n"
										"\r\n", seq_count);
			
			if(sendto(sock, (char *)sendbuf, strlen(sendbuf), 0, (struct sockaddr *)&remote, sizeof(remote)) == SOCKET_ERROR)
			{
				printf("Search IPNC:sendto failed with: %d\n", errno);
				close(sock);
				return -1;
			}
		}
		
		socklen_t len = sizeof(struct sockaddr_in);
		
		unsigned int cc = 0;
		
		for(i = 0; i < 100; i++)
		{
			fd_set r;
			struct timeval t;
			
			t.tv_sec = 0;
			t.tv_usec = 100;
			
			FD_ZERO(&r);
			FD_SET(sock, &r);
			
			int ret = select(sock + 1, &r, NULL, NULL, &t);
			
			if(ret <= 0)
			{
				if(cc++ >= 10)
				{
					break;
				}
				continue;
			}
			
			cc = 0;
			
			if(ret > 0)
			{
				ret = recvfrom(sock, recvbuf, BUFSIZE, 0, (struct sockaddr *)&from, &len);
				if(ret < 0)
				{
					break;
				}
				if(ret == 0)
				{
					break;
				}
				
				recvbuf[ret] = 0;
				
				//第一个包收到的是自己发出去的搜索包
				//printf("hello,%s\n", recvbuf);
				//printf("hello......\n");
				
				if(strstr(recvbuf, "HDS/1.0 200 OK") && 
					strstr(recvbuf,"Client-ID:nvmOPxEnYfQRAeLFdsMrpBbnMDbEPiMC"))
				{
					//printf("hi,%s\n", recvbuf);
					
					char *ip = NULL;
					char *id = NULL;
					char *mac = NULL;
					char *name = NULL;
					char *port = NULL;
					
					char *mask = NULL;
					char *gate = NULL;
					char *fdns = NULL;
					char *sdns = NULL;
					
					//printf("获取IP地址和Device Info\n");
					
					ip = strstr(recvbuf, "IP=");
					if(ip == NULL)
					{
						continue;
					}
					id = strstr(recvbuf, "Device-ID=");
					if(id == NULL)
					{
						continue;
					}
					mac = strstr(recvbuf, "MAC=");
					if(mac == NULL)
					{
						continue;
					}
					name = strstr(recvbuf, "Device-Name=");
					if(name == NULL)
					{
						continue;
					}
					port = strstr(recvbuf, "Http-Port=");
					if(port == NULL)
					{
						continue;
					}
					
					mask = strstr(recvbuf, "MASK=");
					if(mask == NULL)
					{
						printf("MASK is null\n");
						continue;
					}
					gate = strstr(recvbuf, "Gateway=");
					if(gate == NULL)
					{
						printf("Gateway is null\n");
						continue;
					}
					fdns = strstr(recvbuf, "Fdns=");
					if(fdns == NULL)
					{
						printf("Fdns is null\n");
						//continue;
					}
					sdns = strstr(recvbuf, "Sdns=");
					if(sdns == NULL)
					{
						printf("Sdns is null\n");
						//continue;
					}
					else
					{
						//printf("Sdns is %s\n", sdns);
					}
					
					ip += strlen("IP=");
					*strstr(ip, "\r\n") = 0;
					//printf("ip : %s\n", ip);
					
					id += strlen("Device-ID=");
					*strstr(id, "\r\n") = 0;
					//printf("id : %s\n", id);
					
					mac += strlen("MAC=");
					*strstr(mac, "\r\n") = 0;
					//printf("mac : %s\n", mac);
					
					name += strlen("Device-Name=");
					*strstr(name, "\r\n") = 0;
					//printf("name : %s\n", name);
					
					port += strlen("Http-Port=");
					*strstr(port, "\r\n") = 0;
					//printf("port : %s\n", port);
					
					mask += strlen("MASK=");
					*strstr(mask, "\r\n") = 0;
					//printf("mask : %s\n", mask);
					
					gate += strlen("Gateway=");
					*strstr(gate, "\r\n") = 0;
					//printf("gateway : %s\n", gate);
					
					if(fdns != NULL)
					{
						fdns += strlen("Fdns=");
						*strstr(fdns, "\r\n") = 0;
						//printf("Fdns : %s\n", fdns);
					}
					
					if(sdns != NULL)
					{
						sdns += strlen("Sdns=");
						*strstr(sdns, "\r\n") = 0;
						//printf("Sdns : %s\n", sdns);
					}
					
					ipc_node *pNode = (ipc_node *)malloc(sizeof(ipc_node));
					if(pNode == NULL)
					{
						printf("Not enough space to save new ipc info.\n");
						if(*head)
						{
							IPC_Free(*head);
							*head = NULL;
						}
						close(sock);
						return -1;
					}
					
					memset(pNode, 0, sizeof(*pNode));
					pNode->next = NULL;
					
					count++;
					
					strncpy(pNode->ipcam.address, mac, sizeof(pNode->ipcam.address)-1);
					strcpy(pNode->ipcam.user, "admin");
					strcpy(pNode->ipcam.pwd, "admin");
					pNode->ipcam.channel_no = 0;
					pNode->ipcam.enable = 0;
					pNode->ipcam.ipc_type = IPC_TYPE_720P;
					pNode->ipcam.protocol_type = PRO_TYPE_HAIXIN;
					pNode->ipcam.stream_type = STREAM_TYPE_MAIN;
					pNode->ipcam.trans_type = TRANS_TYPE_TCP;
					pNode->ipcam.force_fps = 0;
					pNode->ipcam.frame_rate = 30;
					pNode->ipcam.dwIp = inet_addr(ip);
					pNode->ipcam.wPort = atoi(port);
					strcpy(pNode->ipcam.uuid, id);
					strcpy(pNode->ipcam.name, name);
					pNode->ipcam.net_mask = inet_addr(mask);
					pNode->ipcam.net_gateway = inet_addr(gate);
					if(fdns != NULL)
					{
						pNode->ipcam.dns1 = inet_addr(fdns);
						if(pNode->ipcam.dns1 == (unsigned int)(-1))
						{
							pNode->ipcam.dns1 = inet_addr("8.8.8.8");
						}
					}
					if(sdns != NULL)
					{
						pNode->ipcam.dns2 = inet_addr(sdns);
						if(pNode->ipcam.dns2 == (unsigned int)(-1))
						{
							pNode->ipcam.dns2 = inet_addr("8.8.8.8");
						}
					}

					//看是否有重复的
					int rtn = IPC_Find(*head, pNode);
					if(rtn == 1)
					{
						printf("ipc conflict : [%s,0x%08x,%d,%s]\n", pNode->ipcam.address, pNode->ipcam.dwIp, pNode->ipcam.wPort, pNode->ipcam.uuid);
						conflict_flag = 1;
					}
					else if(rtn == 2)
					{
						printf("ipc repeat : [%s,0x%08x,%d,%s]\n", pNode->ipcam.address, pNode->ipcam.dwIp, pNode->ipcam.wPort, pNode->ipcam.uuid);
						free(pNode);
						pNode = NULL;
						continue;
					}
					
					printf("I13-ipc%d : [%s,0x%08x,%d,%s]\n", count, pNode->ipcam.address, pNode->ipcam.dwIp, pNode->ipcam.wPort, pNode->ipcam.uuid);
					
					if(*head == NULL)
					{
						*head = pNode;
						tail = pNode;
					}
					else
					{
						tail->next = pNode;
						tail = pNode;
					}
				}
			}
		}
		
		//清空缓冲区
		for(;;)
		{
			fd_set r;
			struct timeval t;
			
			t.tv_sec = 0;
			t.tv_usec = 0;
			
			FD_ZERO(&r);
			FD_SET(sock, &r);
			
			char recvbuf[BUFSIZE];
			socklen_t len = sizeof(struct sockaddr_in);
			
			int ret = select(sock + 1, &r, NULL, NULL, &t);
			if(ret > 0)
			{
				recvfrom(sock, recvbuf, BUFSIZE, 0, (struct sockaddr *)&from, &len);
				continue;
			}
			
			break;
		}
		
		close(sock);
		
		if(check_conflict && conflict_flag)
		{
			int idx = 0;
			ipc_node *p = *head;
			while(p)
			{
				if(p->ipcam.protocol_type == PRO_TYPE_HAIXIN)
				{
					unsigned int val = inet_addr("192.168.1.151");
					val = ntohl(val);
					val = val + idx;
					val = htonl(val);
					
					ipc_neteork_para_t nw;
					nw.ip_address = val;
					nw.net_mask = p->ipcam.net_mask;
					nw.net_gateway = p->ipcam.net_gateway;
					nw.dns1 = p->ipcam.dns1;
					nw.dns2 = p->ipcam.dns2;
					if(IPC_SetNetworkParam(&p->ipcam, &nw) == 0)
					{
						p->ipcam.dwIp = val;
					}
					
					idx++;
				}
				//idx++;
				p = p->next;
			}
		}
	}
	
//Step2:
	if(protocol_type & PRO_TYPE_ONVIF)
	{
	#if 1
		//printf("ONVIF search-1......\n");
		//fflush(stdout);
		
		//ONVIF_LOCK();
		
		//printf("ONVIF search-2\n");
		//fflush(stdout);
		
		onvif_wsdiscovery onvif;
		onvif.init();
		
		//printf("ONVIF search-3\n");
		//fflush(stdout);
		
		onvif.send_once_wsdiscovery_multi();
		
		//printf("ONVIF search-4\n");
		//fflush(stdout);
		
		//ONVIF_UNLOCK();
		
		//printf("ONVIF search-5\n");
		//fflush(stdout);
		
		for(list<discovery_device *>::iterator it = onvif.receivers.begin(); it != onvif.receivers.end(); it++)
		{
			//printf("ONVIF search-6\n");
			//fflush(stdout);
			
			int len = 0;
			
			char *ip = NULL;
			char *port = NULL;
			
			char default_port[] = {"80"};
			
			char address[256];
			memset(address, 0, sizeof(address));
			strncpy(address, (*it)->get_xaddrs(len), sizeof(address)-1);
			
			ip = strstr(address, "http://");
			if(ip == NULL)
			{
				printf("error address(%s) for resolve ip\n",address);
				//if(*head)
				//{
				//	IPC_Free(*head);
				//	*head = NULL;
				//}
				//return -1;
				continue;
			}
			ip += strlen("http://");
			
			//海康最新型号的摄像机支持ipv4和ipv6
			//http://192.168.1.99/onvif/device_service http://[fe80::4619:b7ff:fe11:fe78]/onvif/device_service
			char *ptr = strstr(ip, " http://");
			if(ptr != NULL)
			{
				*ptr = 0;
			}
			
			port = strstr(ip, ":");
			if(port == NULL)
			{
				printf("error address(%s) for resolve port\n",address);
				//if(*head)
				//{
				//	IPC_Free(*head);
				//	*head = NULL;
				//}
				//return -1;
				port = default_port;
				if(strstr(ip, "/") != NULL)
				{
					*strstr(ip, "/") = '\0';
				}
			}
			else
			{
				*port = '\0';
				port += strlen(":");
				if(strstr(port, "/") != NULL)
				{
					*strstr(port, "/") = '\0';
				}
			}
			
			ipc_node *pNode = (ipc_node *)malloc(sizeof(ipc_node));
			if(pNode == NULL)
			{
				printf("Not enough space to save new ipc info.\n");
				if(*head)
				{
					IPC_Free(*head);
					*head = NULL;
				}
				return -1;
			}
			
			memset(pNode, 0, sizeof(*pNode));
			pNode->next = NULL;
			
			//strncpy(pNode->ipcam.address, (*it)->get_xaddrs(len), sizeof(pNode->ipcam.address)-1);
			char onvifaddress[256];
			memset(onvifaddress, 0, sizeof(onvifaddress));
			strncpy(onvifaddress, (*it)->get_xaddrs(len), sizeof(onvifaddress)-1);
			ptr = strstr(onvifaddress+strlen("http://"), " http://");
			if(ptr != NULL)
			{
				*ptr = 0;
			}
			strncpy(pNode->ipcam.address, onvifaddress, sizeof(pNode->ipcam.address)-1);
			strcpy(pNode->ipcam.user, "admin");
			strcpy(pNode->ipcam.pwd, "admin");
			strcpy(pNode->ipcam.name, "onvif-nvt-ipc");
			//strcpy(pNode->ipcam.uuid, "5f5a69c2-e0ae-504f-829b-000389111004");
			pNode->ipcam.channel_no = 0;
			pNode->ipcam.enable = 0;
			pNode->ipcam.ipc_type = IPC_TYPE_720P;
			pNode->ipcam.protocol_type = PRO_TYPE_ONVIF;
			pNode->ipcam.stream_type = STREAM_TYPE_MAIN;
			pNode->ipcam.trans_type = TRANS_TYPE_TCP;
			pNode->ipcam.force_fps = 0;
			pNode->ipcam.frame_rate = 30;
			pNode->ipcam.dwIp = inet_addr((*it)->get_ip(len));//inet_addr(ip);
			pNode->ipcam.wPort = atoi(port);
			strcpy(pNode->ipcam.uuid, (*it)->get_uuid(len));
			
			
			if(IPC_Find(*head, pNode))
			{
				printf("ipc conflict : [%s,0x%08x,%d,%s]\n", pNode->ipcam.address, pNode->ipcam.dwIp, pNode->ipcam.wPort, pNode->ipcam.uuid);
				free(pNode);
				pNode = NULL;
				continue;
			}
			
			count++;
			
			printf("ONVIF-ipc%d : [%s,0x%08x,%d,%s]\n", count, pNode->ipcam.address, pNode->ipcam.dwIp, pNode->ipcam.wPort, pNode->ipcam.uuid);
			
			if(*head == NULL)
			{
				*head = pNode;
				tail = pNode;
			}
			else
			{
				tail->next = pNode;
				tail = pNode;
			}
		}
	#else
		/*define container to store the device address */
		DeviceAddrsStack deviceStack;
		memset(&deviceStack, 0, sizeof(deviceStack));
		
		ONVIF_LOCK();
		
		if(!discovery(&deviceStack))
		{
			ONVIF_UNLOCK();
			
			printf("discovery %d devices\n",deviceStack.len);
			
			for(i = 0; i < deviceStack.len; ++i)
			{
				//printf("ipc%d	:	%s\n", i, deviceStack.stack[i]);
				
				char *ip = NULL;
				char *port = NULL;
				
				char address[64];
				memset(address, 0, sizeof(address));
				strncpy(address, deviceStack.stack[i], sizeof(address)-1);
				
				ip = strstr(address, "http://");
				if(ip == NULL)
				{
					printf("error address for resolve ip\n");
					if(*head)
					{
						IPC_Free(*head);
						*head = NULL;
					}
					return -1;
				}
				ip += strlen("http://");
				
				port = strstr(ip, ":");
				if(port == NULL)
				{
					printf("error address for resolve port\n");
					if(*head)
					{
						IPC_Free(*head);
						*head = NULL;
					}
					return -1;
				}
				*port = '\0';
				port += strlen(":");
				*strstr(port, "/") = 0;
				
				ipc_node *pNode = (ipc_node *)malloc(sizeof(ipc_node));
				if(pNode == NULL)
				{
					printf("Not enough space to save new ipc info.\n");
					if(*head)
					{
						IPC_Free(*head);
						*head = NULL;
					}
					return -1;
				}
				
				memset(pNode, 0, sizeof(*pNode));
				pNode->next = NULL;
				
				strncpy(pNode->ipcam.address, deviceStack.stack[i], sizeof(pNode->ipcam.address)-1);
				strcpy(pNode->ipcam.user, "admin");
				strcpy(pNode->ipcam.pwd, "admin");
				//strcpy(pNode->ipcam.name, "onvif-nvt-ipc");
				//strcpy(pNode->ipcam.uuid, "5f5a69c2-e0ae-504f-829b-000389111004");
				pNode->ipcam.channel_no = 0;
				pNode->ipcam.enable = 0;
				pNode->ipcam.ipc_type = IPC_TYPE_720P;
				pNode->ipcam.protocol_type = PRO_TYPE_ONVIF;
				pNode->ipcam.stream_type = STREAM_TYPE_MAIN;
				pNode->ipcam.trans_type = TRANS_TYPE_TCP;
				pNode->ipcam.force_fps = 0;
				pNode->ipcam.frame_rate = 30;
				pNode->ipcam.dwIp = inet_addr(ip);
				pNode->ipcam.wPort = atoi(port);
				strcpy(pNode->ipcam.uuid, deviceStack.uuid[i]);
				
				printf("STD-ipc%d : [%s,0x%08x,%d,%s]\n", i, pNode->ipcam.address, pNode->ipcam.dwIp, pNode->ipcam.wPort, pNode->ipcam.uuid);
				
				if(*head == NULL)
				{
					*head = pNode;
					tail = pNode;
				}
				else
				{
					tail->next = pNode;
					tail = pNode;
				}
			}
		}
		else
		{
			ONVIF_UNLOCK();
		}
		
		DestoryDevice(&deviceStack);
	#endif
	}
	
	if(*head == NULL)
	{
		printf("No ipc on this network.\n");
		return -1;
	}
	
	return 0;
}

//释放网络摄像机
int IPC_Free(ipc_node* head)
{
	if(head == NULL)
	{
		return 0;
	}
	
	ipc_node *p1 = head;
	
	while(p1)
	{
		ipc_node *p2 = p1->next;
		
		//printf("free ipc : [%s,0x%08x,%d]\n", p1->ipcam.address, p1->ipcam.dwIp, p1->ipcam.wPort);
		
		free(p1);
		
		p1 = p2;
	}
	
	return 0;
}

int IPC_GetNetworkParam(ipc_unit *ipcam, ipc_neteork_para_t *pnw)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(ipcam == NULL)
	{
		return -1;
	}
	
	if(pnw == NULL)
	{
		return -1;
	}
	
	if(ipcam->protocol_type == PRO_TYPE_ONVIF)
	{
		return Onvif_GetNetworkParam(ipcam, pnw);
	}
	else if(ipcam->protocol_type == PRO_TYPE_HAIXIN)
	{
		return I13_GetNetworkParam(ipcam, pnw);
	}
	
	return -1;
}

int IPC_SetNetworkParam(ipc_unit *ipcam, ipc_neteork_para_t *pnw)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(ipcam == NULL)
	{
		return -1;
	}
	
	if(pnw == NULL)
	{
		return -1;
	}
	
	if(ipcam->protocol_type == PRO_TYPE_ONVIF)
	{
		return Onvif_SetNetworkParam(ipcam, pnw);
	}
	else if(ipcam->protocol_type == PRO_TYPE_HAIXIN)
	{
		return I13_SetNetworkParam(ipcam, pnw);
	}
	
	return -1;
}

int IPC_CMD_Open(int chn)
{
	return 0;
}

int IPC_CMD_Close(int chn)
{
	return 0;
}

int IPC_CMD_SetImageParam(int chn, video_image_para_t *para)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(chn < 0 || chn >= (int)g_chn_count)
	{
		return -1;
	}
	
	if(para == NULL)
	{
		return -1;
	}
	
	pthread_mutex_lock(&g_chn_info[chn].lock);
	
	if(g_chn_info[chn].ops.SetImageParam != NULL)
	{
		g_chn_info[chn].ops.SetImageParam(chn, para);
	}
	
	pthread_mutex_unlock(&g_chn_info[chn].lock);
	
	return 0;
}

int IPC_CMD_GetImageParam(int chn, video_image_para_t *para)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(chn < 0 || chn >= (int)g_chn_count)
	{
		return -1;
	}
	
	if(para == NULL)
	{
		return -1;
	}
	
	pthread_mutex_lock(&g_chn_info[chn].lock);
	
	if(g_chn_info[chn].ops.GetImageParam != NULL)
	{
		g_chn_info[chn].ops.GetImageParam(chn, para);
	}
	
	pthread_mutex_unlock(&g_chn_info[chn].lock);
	
	return 0;
}

static unsigned int g_speed = 0;

int IPC_CMD_PtzCtrl(int chn, EMPTZCMDTYPE cmd, void* data)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(chn < 0 || chn >= (int)g_chn_count)
	{
		return -1;
	}
	
	pthread_mutex_lock(&g_chn_info[chn].lock);
	
	if(cmd == EM_PTZ_CMD_SETSPEED)
	{
		g_speed = *((unsigned int *)data);
	}
	
	if(cmd < EM_PTZ_CMD_STOP_TILEUP)
	{
		unsigned int speed = *((unsigned int *)data);
		if(speed == 0)
		{
			if(g_chn_info[chn].newcam.protocol_type != PRO_TYPE_HAIXIN)
			{
				*((unsigned int *)data) = g_speed;
			}
		}
	}
	
	if(g_chn_info[chn].ops.PtzCtrl != NULL)
	{
		g_chn_info[chn].ops.PtzCtrl(chn, cmd, data);
	}
	
	pthread_mutex_unlock(&g_chn_info[chn].lock);
	
	return 0;
}

int IPC_CMD_SetTime(int chn, time_t t, int force)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(chn < 0 || chn >= (int)g_chn_count)
	{
		return -1;
	}
	
	//pthread_mutex_lock(&g_chn_info[chn].lock);
	
	if(g_chn_info[chn].ops.SetTime != NULL)
	{
		g_chn_info[chn].ops.SetTime(chn, t, force);
	}
	
	//pthread_mutex_unlock(&g_chn_info[chn].lock);
	
	return 0;
}

int IPC_CMD_Reboot(int chn)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(chn < 0 || chn >= (int)g_chn_count)
	{
		return -1;
	}
	
	//pthread_mutex_lock(&g_chn_info[chn].lock);
	
	if(g_chn_info[chn].ops.Reboot != NULL)
	{
		g_chn_info[chn].ops.Reboot(chn);
	}
	
	//pthread_mutex_unlock(&g_chn_info[chn].lock);
	
	return 0;
}

int IPC_CMD_RequestIFrame(int chn)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(chn < 0 || chn >= (int)(g_chn_count*2))
	{
		return -1;
	}
	
	int idx = chn;
	
	if(chn >= (int)(g_chn_count))
	{
		chn -= g_chn_count;
	}
	
	//printf("chn%d IPC_CMD_RequestIFrame try lock\n",chn);
	
	//pthread_mutex_lock(&g_chn_info[chn].lock);
	
	//printf("chn%d IPC_CMD_RequestIFrame get lock\n",chn);
	
	if(g_chn_info[chn].ops.RequestIFrame != NULL)
	{
		g_chn_info[chn].ops.RequestIFrame(idx);
	}
	
	//pthread_mutex_unlock(&g_chn_info[chn].lock);
	
	//printf("chn%d IPC_CMD_RequestIFrame over\n",chn);
	
	return 0;
}

int Onvif_GetNetworkParam(ipc_unit *ipcam, ipc_neteork_para_t *pnw)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(ipcam == NULL)
	{
		return -1;
	}
	
	if(pnw == NULL)
	{
		return -1;
	}
	
	if(ipcam->net_mask)
	{
		pnw->ip_address = ipcam->dwIp;
		pnw->net_mask = ipcam->net_mask;
		pnw->net_gateway = ipcam->net_gateway;
		pnw->dns1 = ipcam->dns1;
		pnw->dns2 = ipcam->dns2;
		
		return 0;
	}
	
	ONVIF_LOCK();
	
	discovery_device *receiver_onvif = new device_ptz();
	if(NULL == receiver_onvif)
	{
		ONVIF_UNLOCK();
		return -1;
	}
	
	struct in_addr serv;
	serv.s_addr = ipcam->dwIp;
	char ipaddr[32] = {0};
	strcpy(ipaddr, inet_ntoa(serv));
	receiver_onvif->set_ip(ipaddr, strlen(ipaddr));
	
	receiver_onvif->set_xaddrs(ipcam->address, strlen(ipcam->address));
	
	((device_ptz*)receiver_onvif)->set_username_password(ipcam->user,ipcam->pwd);
	
	device_ip dev_ip;
	memset(&dev_ip,0,sizeof(dev_ip));
	((device_ptz*)receiver_onvif)->handle_mngmt_get_ip(&dev_ip);
	if(ipcam->dwIp)
	{
		pnw->ip_address = ipcam->dwIp;
	}
	else
	{
		pnw->ip_address = inet_addr(dev_ip.ip);
	}
	pnw->net_mask = (unsigned int)(-1) - ((1 << (32 - dev_ip.prefix)) - 1);
	pnw->net_mask = htonl(pnw->net_mask);
	printf("Onvif_GetNetworkParam prefix=%d,net_mask=0x%08x\n",dev_ip.prefix,pnw->net_mask);
	
	device_gateway dev_gateway;
	memset(&dev_gateway,0,sizeof(dev_gateway));
	((device_ptz*)receiver_onvif)->handle_mngmt_get_default_gateway(&dev_gateway);
	pnw->net_gateway = inet_addr(dev_gateway.gateway);
	
	device_dns dev_dns;
	memset(&dev_dns,0,sizeof(dev_dns));
	((device_ptz*)receiver_onvif)->handle_mngmt_get_dns(&dev_dns);
	pnw->dns1 = inet_addr(dev_dns.dns);
	pnw->dns2 = 0;
	
	delete receiver_onvif;
	
	ONVIF_UNLOCK();
	
	return 0;
}

int Onvif_SetNetworkParam(ipc_unit *ipcam, ipc_neteork_para_t *pnw)
{
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(ipcam == NULL)
	{
		return -1;
	}
	
	if(pnw == NULL)
	{
		return -1;
	}
	
	ONVIF_LOCK();
	
	discovery_device *receiver_onvif = new device_ptz();
	if(NULL == receiver_onvif)
	{
		ONVIF_UNLOCK();
		return -1;
	}
	
	struct in_addr serv;
	serv.s_addr = ipcam->dwIp;
	char ipaddr[32] = {0};
	strcpy(ipaddr, inet_ntoa(serv));
	receiver_onvif->set_ip(ipaddr, strlen(ipaddr));
	
	receiver_onvif->set_xaddrs(ipcam->address, strlen(ipcam->address));
	
	((device_ptz*)receiver_onvif)->set_username_password(ipcam->user,ipcam->pwd);
	
	int prefix_length = 0;
	
	struct in_addr host;
	host.s_addr = pnw->ip_address;
	strcpy(ipaddr, inet_ntoa(host));
	
	int i = 0;
	int bits = 32;//sizeof(pnw->net_mask)*8;
	unsigned int val = ntohl(pnw->net_mask);
	for(i=bits-1;i>=0;i--)
	{
		if((val>>i)&0x1)
		{
			prefix_length++;
		}
		else
		{
			break;
		}
	}
	printf("Onvif_SetNetworkParam:prefix_length=%d\n",prefix_length);
	((device_ptz*)receiver_onvif)->handle_mngmt_set_ip_prefix(ipaddr, strlen(ipaddr), prefix_length);
	
	host.s_addr = pnw->net_gateway;
	strcpy(ipaddr, inet_ntoa(host));
	((device_ptz*)receiver_onvif)->handle_mngmt_set_default_gateway(ipaddr, strlen(ipaddr));
	
	host.s_addr = pnw->dns1;
	strcpy(ipaddr, inet_ntoa(host));
	((device_ptz*)receiver_onvif)->handle_mngmt_set_dns(ipaddr, strlen(ipaddr));
	
	delete receiver_onvif;
	
	ONVIF_UNLOCK();
	
	return 0;
}

int Onvif_CMD_Open(int chn)
{
	return 0;
}

int Onvif_CMD_Close(int chn)
{
	return 0;
}


int Onvif_CMD_SetImageParam(int chn, video_image_para_t *para)
{
	int ret;
	ipc_unit ipcam;
	if(IPC_Get(chn, &ipcam))
	{
		return -1;
	}
	
	if(!IPC_GetLinkStatus(chn))
	{
		printf("Onvif_CMD_PtzCtrl: chn%d vlost!!!\n",chn);
		return -1;
	}
	
	ONVIF_LOCK();
	
	discovery_device *receiver_onvif = new device_ptz();
	if(NULL == receiver_onvif)
	{
		ONVIF_UNLOCK();
		return -1;
	}

	struct in_addr serv;
	serv.s_addr = ipcam.dwIp;
	char ipaddr[32] = {0};
	strcpy(ipaddr, inet_ntoa(serv));
	receiver_onvif->set_ip(ipaddr, strlen(ipaddr));
	
	receiver_onvif->set_xaddrs(ipcam.address, strlen(ipcam.address));
	
	((device_ptz*)receiver_onvif)->set_username_password(ipcam.user,ipcam.pwd);
	
	if(((device_ptz*)receiver_onvif)->handle_image_setting_support())//先得到一些前提条件
	{
		//那么是设置主码流还是子码流?  0 为主码流
		//由chn  来看
		ret = ((device_ptz*)receiver_onvif)->handle_image_set_brightness(0,para->brightness);
		ret = ((device_ptz*)receiver_onvif)->handle_image_set_color_saturation(0,para->saturation);
		ret = ((device_ptz*)receiver_onvif)->handle_image_set_contrast(0,para->contrast);
		//ret = ((device_ptz*)receiver_onvif)->			// not support hue
	}

	delete receiver_onvif;
	
	ONVIF_UNLOCK();
	
	return 0;
}


int Onvif_CMD_GetImageParam(int chn, video_image_para_t *para)
{
/*	int ret;
	ipc_unit ipcam;
	if(IPC_Get(chn, &ipcam))
	{
		return -1;
	}
	
	if(!IPC_GetLinkStatus(chn))
	{
		printf("Onvif_CMD_PtzCtrl: chn%d vlost!!!\n",chn);
		return -1;
	}
	
	ONVIF_LOCK();
	
	discovery_device *receiver_onvif = new device_ptz();
	if(NULL == receiver_onvif)
	{
		ONVIF_UNLOCK();
		return -1;
	}


	struct in_addr serv;
	serv.s_addr = ipcam.dwIp;
	char ipaddr[32] = {0};
	strcpy(ipaddr, inet_ntoa(serv));
	receiver_onvif->set_ip(ipaddr, strlen(ipaddr));
	
	receiver_onvif->set_xaddrs(ipcam.address, strlen(ipcam.address));
	
	((device_ptz*)receiver_onvif)->set_username_password(ipcam.user,ipcam.pwd);
	
	if(((device_ptz*)receiver_onvif)->handle_image_setting_support())//先得到一些前提条件
	{
		current_image_config* ncurrent_image_config;
		ret = ((device_ptz*)receiver_onvif)->handle_image_current_config(0,&ncurrent_image_config);	//1 or 0 ?
		if(ret >= 0)
		{

		}
	}
	
	delete receiver_onvif;
	
	ONVIF_UNLOCK();
*/
	return 0;
}



int Onvif_CMD_PtzCtrl(int chn, EMPTZCMDTYPE cmd, void* data)
{
	ipc_unit ipcam;
	if(IPC_Get(chn, &ipcam))
	{
		return -1;
	}
	
	if(!IPC_GetLinkStatus(chn))
	{
		printf("Onvif_CMD_PtzCtrl: chn%d vlost!!!\n",chn);
		return -1;
	}
	
	//printf("Onvif_CMD_PtzCtrl: chn%d start......\n",chn);
	
	ONVIF_LOCK();
	
	discovery_device *receiver_onvif = new device_ptz();
	if(NULL == receiver_onvif)
	{
		ONVIF_UNLOCK();
		return -1;
	}
	
	struct in_addr serv;
	serv.s_addr = ipcam.dwIp;
	char ipaddr[32] = {0};
	strcpy(ipaddr, inet_ntoa(serv));
	receiver_onvif->set_ip(ipaddr, strlen(ipaddr));
	
	receiver_onvif->set_xaddrs(ipcam.address, strlen(ipcam.address));
	
	((device_ptz*)receiver_onvif)->set_username_password(ipcam.user,ipcam.pwd);
	
	uint32_t stream_index = 0;
	
	if(((device_ptz*)receiver_onvif)->handle_ptz_support())
	{
		//printf("Onvif_CMD_PtzCtrl: chn%d support ptz cmd\n",chn);
		
		if(cmd == EM_PTZ_CMD_START_TILEUP)
		{
			unsigned int tiltSpeed = *((unsigned int *)data);
			tiltSpeed = tiltSpeed ? (tiltSpeed) : (LS_PTZ_SPEED_MAX/3);
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_UP,0,tiltSpeed,0,0);
		}
		else if(cmd == EM_PTZ_CMD_START_TILEDOWN)
		{
			unsigned int tiltSpeed = *((unsigned int *)data);
			tiltSpeed = tiltSpeed ? (tiltSpeed) : (LS_PTZ_SPEED_MAX/3);
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_DOWN,0,tiltSpeed,0,0);
		}
		else if(cmd == EM_PTZ_CMD_START_PANLEFT)
		{
			unsigned int panSpeed = *((unsigned int *)data);
			panSpeed = panSpeed ? (panSpeed) : (LS_PTZ_SPEED_MAX/3);
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_LEFT,panSpeed,0,0,0);
		}
		else if(cmd == EM_PTZ_CMD_START_PANRIGHT)
		{
			unsigned int panSpeed = *((unsigned int *)data);
			panSpeed = panSpeed ? (panSpeed) : (LS_PTZ_SPEED_MAX/3);
			printf("Onvif_CMD_PtzCtrl: chn%d ptz right speed=%d\n",chn,panSpeed);
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_RIGHT,panSpeed,0,0,0);
		}
		else if(cmd == EM_PTZ_CMD_START_LEFTUP)
		{
			unsigned int panSpeed = *((unsigned int *)data);
			unsigned int tiltSpeed = *((unsigned int *)data);
			panSpeed = panSpeed ? (panSpeed) : (LS_PTZ_SPEED_MAX/3);
			tiltSpeed = tiltSpeed ? (tiltSpeed) : (LS_PTZ_SPEED_MAX/3);
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_UL,panSpeed,tiltSpeed,0,0);
		}
		else if(cmd == EM_PTZ_CMD_START_LEFTDOWN)
		{
			unsigned int panSpeed = *((unsigned int *)data);
			unsigned int tiltSpeed = *((unsigned int *)data);
			panSpeed = panSpeed ? (panSpeed) : (LS_PTZ_SPEED_MAX/3);
			tiltSpeed = tiltSpeed ? (tiltSpeed) : (LS_PTZ_SPEED_MAX/3);
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_DL,panSpeed,tiltSpeed,0,0);
		}
		else if(cmd == EM_PTZ_CMD_START_RIGHTUP)
		{
			unsigned int panSpeed = *((unsigned int *)data);
			unsigned int tiltSpeed = *((unsigned int *)data);
			panSpeed = panSpeed ? (panSpeed) : (LS_PTZ_SPEED_MAX/3);
			tiltSpeed = tiltSpeed ? (tiltSpeed) : (LS_PTZ_SPEED_MAX/3);
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_UR,panSpeed,tiltSpeed,0,0);
		}
		else if(cmd == EM_PTZ_CMD_START_RIGHTDOWN)
		{
			unsigned int panSpeed = *((unsigned int *)data);
			unsigned int tiltSpeed = *((unsigned int *)data);
			panSpeed = panSpeed ? (panSpeed) : (LS_PTZ_SPEED_MAX/3);
			tiltSpeed = tiltSpeed ? (tiltSpeed) : (LS_PTZ_SPEED_MAX/3);
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_DR,panSpeed,tiltSpeed,0,0);
		}
		else if(cmd == EM_PTZ_CMD_START_ZOOMTELE)
		{
			unsigned int zoomSpeed = *((unsigned int *)data);
			zoomSpeed = zoomSpeed ? (zoomSpeed) : (1);
			printf("Onvif_CMD_PtzCtrl: chn%d zoomin speed=%d\n",chn,zoomSpeed);
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_ZOOMIN,0,0,zoomSpeed,0);
		}
		else if(cmd == EM_PTZ_CMD_START_ZOOMWIDE)
		{
			unsigned int zoomSpeed = *((unsigned int *)data);
			zoomSpeed = zoomSpeed ? (zoomSpeed) : (1);
			printf("Onvif_CMD_PtzCtrl: chn%d zoomout speed=%d\n",chn,zoomSpeed);
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_ZOOMOUT,0,0,zoomSpeed,0);
		}
		else if(cmd == EM_PTZ_CMD_START_FOCUSNEAR)
		{
			
		}
		else if(cmd == EM_PTZ_CMD_START_FOCUSFAR)
		{
			
		}
		else if(cmd == EM_PTZ_CMD_START_IRISSMALL)
		{
			
		}
		else if(cmd == EM_PTZ_CMD_START_IRISLARGE)
		{
			
		}
		else if(cmd >= EM_PTZ_CMD_STOP_TILEUP && cmd < EM_PTZ_CMD_PRESET_SET)
		{
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_RIGHT_STOP,0,0,0,0);
		}
		else if(cmd == EM_PTZ_CMD_PRESET_SET)
		{
			unsigned int preset = *((unsigned int *)data);
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_POS_SET,0,0,0,preset);
		}
		else if(cmd == EM_PTZ_CMD_PRESET_GOTO)
		{
			unsigned int preset = *((unsigned int *)data);
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_POS_CALL,0,0,0,preset);
		}
		else if(cmd == EM_PTZ_CMD_PRESET_CLEAR)
		{
			unsigned int preset = *((unsigned int *)data);
			((device_ptz*)receiver_onvif)->do_ptz(stream_index,LS_PTZ_CMD_POS_CLEAR,0,0,0,preset);
		}
		else if(cmd == EM_PTZ_CMD_LIGHT_ON)
		{
			
		}
		else if(cmd == EM_PTZ_CMD_LIGHT_OFF)
		{
			
		}
		else if(cmd == EM_PTZ_CMD_AUX_ON)
		{
			
		}
		else if(cmd == EM_PTZ_CMD_AUX_OFF)
		{
			
		}
		else if(cmd == EM_PTZ_CMD_AUTOPAN_ON)
		{
			
		}
		else if(cmd == EM_PTZ_CMD_AUTOPAN_OFF)
		{
			
		}
	}
	else
	{
		printf("Onvif_CMD_PtzCtrl: chn%d do not support ptz cmd\n",chn);
	}
	
	delete receiver_onvif;
	
	ONVIF_UNLOCK();
	
	return 0;
}

static const char* szTimeZoneInfo[] = 
{
#if 1
	"GMT-12",
	"GMT-11",
	"GMT-10",
	"GMT-09",
	"GMT-08",
	"GMT-07",
	"GMT-06",
	"GMT-05",
	"GMT-04:30",
	"GMT-04",
	"GMT-03:30",
	"GMT-03",
	"GMT-02",
	"GMT-01",
	"GMT",//"GMT+00:00",//"GMT+0",
	"GMT+01",
	"GMT+02",
	"GMT+03",
	"GMT+03:30",
	"GMT+04",
	"GMT+04:30",
	"GMT+05",
	"GMT+05:30",
	"GMT+05:45",
	"GMT+06",
	"GMT+06:30",
	"GMT+07",
	"GMT+08",//"ChinaStandardTime-8",
	"GMT+09",
	"GMT+09:30",
	"GMT+10",
	"GMT+11",
	"GMT+12",
	"GMT+13",
#else
	"GMT-12:00",
	"GMT-11:00",
	"GMT-10:00",
	"GMT-09:00",
	"GMT-08:00",
	"GMT-07:00",
	"GMT-06:00",
	"GMT-05:00",
	"GMT-04:30",
	"GMT-04:00",
	"GMT-03:30",
	"GMT-03:00",
	"GMT-02:00",
	"GMT-01:00",
	"GMT",//"GMT+00:00",//"GMT+0",
	"GMT+01:00",
	"GMT+02:00",
	"GMT+03:00",
	"GMT+03:30",
	"GMT+04:00",
	"GMT+04:30",
	"GMT+05:00",
	"GMT+05:30",
	"GMT+05:45",
	"GMT+06:00",
	"GMT+06:30",
	"GMT+07:00",
	"GMT+08:00",
	"GMT+09:00",
	"GMT+09:30",
	"GMT+10:00",
	"GMT+11:00",
	"GMT+12:00",
	"GMT+13:00",
#endif
};

static const char* GetTZInfo(int index)
{
	if(index < 0 || index >= (int)(sizeof(szTimeZoneInfo)/sizeof(szTimeZoneInfo[0])))
	{
		return "ChinaStandardTime-8";
	}
	
	return szTimeZoneInfo[index];
}

#if 0
static int TimeZoneOffset[] = 
{
	-12*3600,
	-11*3600,
	-10*3600,
	-9*3600,
	-8*3600,
	-7*3600,
	-6*3600,
	-5*3600,
	-4*3600-1800,
	-4*3600,
	-3*3600-1800,
	-3*3600,
	-2*3600,
	-1*3600,
	0,
	1*3600,
	2*3600,
	3*3600,
	3*3600+1800,
	4*3600,
	4*3600+1800,
	5*3600,
	5*3600+1800,
	5*3600+2700,
	6*3600,
	6*3600+1800,
	7*3600,
	8*3600,
	9*3600,
	9*3600+1800,
	10*3600,
	11*3600,
	12*3600,
	13*3600,
};

static int GetTZOffset(int index)
{
	if(index < 0 || index >= (int)(sizeof(TimeZoneOffset)/sizeof(TimeZoneOffset[0])))
	{
		return 0;
	}
	
	return TimeZoneOffset[index];
}
#endif

int Onvif_CMD_SetTime(int chn, time_t t, int force)
{
	ipc_unit ipcam;
	if(IPC_Get(chn, &ipcam))
	{
		return -1;
	}
	
	if(!force)
	{
		if(!ipcam.enable || !IPC_GetLinkStatus(chn))
		{
			//printf("Onvif_CMD_SetTime: chn%d vlost!!!\n",chn);
			return -1;
		}
	}
	
	ONVIF_LOCK();
	
	discovery_device *receiver_onvif = new device_ptz();
	if(NULL == receiver_onvif)
	{
		ONVIF_UNLOCK();
		return -1;
	}
	
	struct in_addr serv;
	serv.s_addr = ipcam.dwIp;
	char ipaddr[32] = {0};
	strcpy(ipaddr, inet_ntoa(serv));
	receiver_onvif->set_ip(ipaddr, strlen(ipaddr));
	
	receiver_onvif->set_xaddrs(ipcam.address, strlen(ipcam.address));
	
	((device_ptz*)receiver_onvif)->set_username_password(ipcam.user,ipcam.pwd);
	
	if(t == 0)
	{
		t = time(NULL);// + 1;//???
	}
	
	int nTimeZone = IPC_GetTimeZone();
	//t += GetTZOffset(nTimeZone);
	
	//printf("Onvif_CMD_SetTime: chn%d start set,TimeZone=%s!!!\n",chn,GetTZInfo(nTimeZone));
	
	//((device_ptz*)receiver_onvif)->handle_mngmt_set_time_zone("GMT+00:00");//海芯摄像机不对
	//((device_ptz*)receiver_onvif)->handle_mngmt_set_time_zone("GMT+0");//海芯摄像机不对
	//((device_ptz*)receiver_onvif)->handle_mngmt_set_time_zone("GMT");
	//((device_ptz*)receiver_onvif)->handle_mngmt_set_time_zone(GetTZInfo(nTimeZone));
	((device_ptz*)receiver_onvif)->handle_mngmt_set_date_time(t,GetTZInfo(nTimeZone));
	//((device_ptz*)receiver_onvif)->handle_mngmt_set_date_time(t,NULL);
	
	delete receiver_onvif;
	
	ONVIF_UNLOCK();
	
	return 0;
}

int Onvif_CMD_Reboot(int chn)
{
	ipc_unit ipcam;
	if(IPC_Get(chn, &ipcam))
	{
		return -1;
	}
	
	ONVIF_LOCK();
	
	discovery_device *receiver_onvif = new device_ptz();
	if(NULL == receiver_onvif)
	{
		ONVIF_UNLOCK();
		return -1;
	}
	
	struct in_addr serv;
	serv.s_addr = ipcam.dwIp;
	char ipaddr[32] = {0};
	strcpy(ipaddr, inet_ntoa(serv));
	receiver_onvif->set_ip(ipaddr, strlen(ipaddr));
	
	receiver_onvif->set_xaddrs(ipcam.address, strlen(ipcam.address));
	
	((device_ptz*)receiver_onvif)->set_username_password(ipcam.user,ipcam.pwd);
	
	printf("$$$$$$$$$$chn%d onvif-ipc reboot$$$$$$$$$$\n",chn);
	
	((device_ptz*)receiver_onvif)->handle_mngmt_system_reboot();
	
	delete receiver_onvif;
	
	ONVIF_UNLOCK();
	
	return 0;
}

int Onvif_CMD_RequestIFrame(int chn)
{
	return -1;
	
	if(!g_init_flag)
	{
		return -1;
	}
	
	if(chn < 0 || chn >= (int)(g_chn_count*2))
	{
		return -1;
	}
	
	if(!IPC_GetLinkStatus(chn))
	{
		//printf("Onvif_CMD_RequestIFrame: chn%d vlost!!!\n",chn);
		return -1;
	}
	
	//int idx = chn;
	
	if(chn >= (int)(g_chn_count))
	{
		chn -= g_chn_count;
	}
	
	ipc_unit ipcam;
	if(IPC_Get(chn, &ipcam))
	{
		return -1;
	}
	
	if(!ipcam.enable)
	{
		return -1;
	}
	
	ONVIF_LOCK();
	
	discovery_device *receiver_onvif = new device_ptz();
	if(NULL == receiver_onvif)
	{
		ONVIF_UNLOCK();
		return -1;
	}
	
	struct in_addr serv;
	serv.s_addr = ipcam.dwIp;
	char ipaddr[32] = {0};
	strcpy(ipaddr, inet_ntoa(serv));
	receiver_onvif->set_ip(ipaddr, strlen(ipaddr));
	
	receiver_onvif->set_xaddrs(ipcam.address, strlen(ipcam.address));
	
	((device_ptz*)receiver_onvif)->set_username_password(ipcam.user,ipcam.pwd);
	
	//((device_ptz*)receiver_onvif)->handle_mngmt_system_reboot();
	
	delete receiver_onvif;
	
	ONVIF_UNLOCK();
	
	return 0;
}

