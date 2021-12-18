/******************************************************************************

                  版权所有 (C), 2019-2029, SDC OS 开源软件小组所有

 ******************************************************************************
  文 件 名   : main.c
  版 本 号   : 初稿
  作    者   : jelly
  生成日期   : 2019年6月8日
  最近修改   :
  功能描述   : 主函数
  函数列表   :
                             main
                             sdc_ReadFromVideoService
  修改历史   :
  1.日    期   : 2019年6月8日
    作    者   : jelly
    修改内容   : 创建文件

******************************************************************************/

/*----------------------------------------------*
 * 包含头文件                                   *
 *----------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>


#include <inttypes.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <time.h>



#include"main.h"
#include "sdc.h"
#include "sdc_os_api.h"
#include"queue.h"

#include "sample_comm_nnie.h"
#include "label_event.h"

/*----------------------------------------------*
 * 外部变量说明                                 *
 *----------------------------------------------*/
extern int fd_video;
extern int fd_codec;
extern int fd_utils;
extern int fd_algorithm;
extern int fd_event;
extern int fd_cache;
extern unsigned short int trans_id;




/*----------------------------------------------*
 * 外部函数原型说明                             *
 *----------------------------------------------*/
extern unsigned int sleep (unsigned int seconds);


/*----------------------------------------------*
 * 内部函数原型说明                             *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 全局变量                                     *
 *----------------------------------------------*/
QUEUE_S g_ptrQueue;
#define MAX_OBJ_NUM 10

/*----------------------------------------------*
 * 模块级变量                                   *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 常量定义                                     *
 *----------------------------------------------*/

SYSTEM_MANAGE_PARAS_S g_stSystemManage;
unsigned int g_uiRecvNum = 0;
unsigned int g_uiFreeNum = 0;

unsigned int g_uiRecvPos = 0;
unsigned int g_uiFreePos = 0;


/*ssd para*/
SAMPLE_SVP_NNIE_MODEL_S s_stYoloModel = {0};
SAMPLE_SVP_NNIE_PARAM_S s_stYoloNnieParam = {0};
SAMPLE_SVP_NNIE_YOLOV3_SOFTWARE_PARAM_S s_stYolov3SoftwareParam = {0};
SAMPLE_SVP_NNIE_SSD_SOFTWARE_PARAM_S s_stSsdSoftwareParam = {0};
SAMPLE_SVP_NNIE_CFG_S   stNnieCfg_SDC = {0};

HI_BOOL BGRInProcessingFlag = HI_FALSE;
HI_CHAR BGRBuffer[304*300*3];
HI_FLOAT thresh = 0.5; 

typedef enum IVS_FRAME_TYPE_E
{
    VW_FRAME_ORIGION = 0,
    VW_FRAME_SCALE_1,
    VW_FRAME_TYPE_MAX,
} VW_IVS_FRAME_TYPE_E;

typedef struct VW_IVSFRAMEDATA
{
    char reverse0[2616];
    VW_YUV_FRAME_S astYuvframe[VW_FRAME_TYPE_MAX];
    char reverse1[272];
} VW_IVSFRAMEDATA_S;


extern int SDC_SVP_ForwardBGR(HI_CHAR *pcSrcBGR, SDC_SSD_RESULT_S *pstResult, SDC_SSD_INPUT_SIZE_S InputSize);



/*----------------------------------------------*
 * 宏定义                                       *
 *----------------------------------------------*/
#define MAX_YUV_BUF_NUM 50 

#define  APP_NAME   "safetyhat"
#define LABEL_EVENT_DATA                 ("tag.saas.sdc")
 
//excavator_dx

int SDC_LabelEventDel(int fp, unsigned int baseid, unsigned int id, char *cAppName)
{
    int nDataLen;
	int nResult;
	char *pcTemp = NULL;
	
    paas_shm_cached_event_s shm_event;
    sdc_extend_head_s shm_head = {
        .type = SDC_HEAD_SHM_CACHED_EVENT,
        .length = 8,
    };

    sdc_common_head_s head;
    head.version = SDC_VERSION;
    head.url = SDC_URL_PAAS_EVENTD_EVENT;
    head.method = SDC_METHOD_CREATE;
    head.head_length = sizeof(head) + sizeof(shm_head);
    head.content_length = sizeof(shm_event);

    struct iovec iov[3];
    iov[0].iov_base = &head;
    iov[0].iov_len = sizeof(head);
    iov[1].iov_base = &shm_head;
    iov[1].iov_len = sizeof(shm_head);
    iov[2].iov_base = &shm_event;
    iov[2].iov_len = sizeof(shm_event);

    SDC_SHM_CACHE_S shm_cache;
    memset(&shm_cache, 0, sizeof(shm_cache));

    LABEL_EVENT_DATA_S * pevent = NULL;
    nDataLen = sizeof(label);
    shm_cache.size  = sizeof(LABEL_EVENT_DATA_S) + nDataLen;
	shm_cache.ttl = 0;
    //printf("ioctl fail\n");
	
    nResult = ioctl(fd_cache, SDC_CACHE_ALLOC,&shm_cache);
    if(nResult != 0)
    {
        printf("ioctl fail\n");
        goto EVENT_FAIL;
    }
    pevent = (LABEL_EVENT_DATA_S *)shm_cache.addr_virt;
	
    pevent->data = (char *)pevent +sizeof(LABEL_EVENT_DATA_S);
    pevent->base.id = baseid;

    (void)sprintf(pevent->base.name, "%s", LABEL_EVENT_DATA);

	pevent->base.length = nDataLen;
	memset(pevent->data, 0, sizeof(nDataLen));
    pcTemp = pevent->data;

	*(uint32_t *)pcTemp = 0;//add
	pcTemp += sizeof(uint32_t);
	
	strcpy_s(pcTemp, 32, cAppName); //app name
	pcTemp += 32;
	
	*(uint64_t *)pcTemp = id;

	shm_event.addr_phy = shm_cache.addr_phy;	
	shm_event.size = shm_cache.size;	
	shm_event.cookie = shm_cache.cookie;
	nResult = writev(fp, iov, 3);
	if(nResult == -1)
    {
        printf("ioctl fail\n");
        goto EVENT_FAIL;
    }   
	munmap(pevent, sizeof(LABEL_EVENT_DATA_S) + nDataLen);

    return 0;
EVENT_FAIL:
    if(pevent)
    {
        munmap(pevent, sizeof(LABEL_EVENT_DATA_S) + nDataLen);
    }

    return -1;
	
}


int SDC_LabelEventPublish(int fp, unsigned int baseid, int iDataLen, char *cEventMsg, uint64_t pts)
{
	int nResult;
    paas_shm_cached_event_s shm_event;
    sdc_extend_head_s shm_head = {
        .type = SDC_HEAD_SHM_CACHED_EVENT,
        .length = 8,
    };

    sdc_common_head_s head;
    head.version = SDC_VERSION;
    head.url = SDC_URL_PAAS_EVENTD_EVENT;
    head.method = SDC_METHOD_CREATE;
    head.head_length = sizeof(head) + sizeof(shm_head);
    head.content_length = sizeof(shm_event);

    struct iovec iov[3];
    iov[0].iov_base = &head;
    iov[0].iov_len = sizeof(head);
    iov[1].iov_base = &shm_head;
    iov[1].iov_len = sizeof(shm_head);
    iov[2].iov_base = &shm_event;
    iov[2].iov_len = sizeof(shm_event);


    SDC_SHM_CACHE_S shm_cache;
    memset(&shm_cache, 0, sizeof(shm_cache));

    LABEL_EVENT_DATA_S * pevent = NULL;
    //nDataLen= sizeof(label) + uiPolygonNum * sizeof(polygon) + iPointNum * sizeof(point) + (uiTagNum - '0')*sizeof(tag) + iStrNum;
    shm_cache.size  = sizeof(LABEL_EVENT_DATA_S) + iDataLen;
    nResult = ioctl(fd_cache, SDC_CACHE_ALLOC,&shm_cache);
    if(nResult != 0)
    {
        printf("ioctl fail\n");
        goto EVENT_FAIL;
    }
    pevent = (LABEL_EVENT_DATA_S *)shm_cache.addr_virt;
	pevent->data = (char *)pevent + sizeof(LABEL_EVENT_DATA_S);
    pevent->base.id = baseid;
	pevent->base.src_timestamp = pts;
	pevent->base.tran_timestamp = pts + 10;/*默认填写*/
    (void)sprintf(pevent->base.name, "%s", LABEL_EVENT_DATA);
	(void)sprintf(pevent->base.publisher, "%s", "test");

	pevent->base.length = iDataLen;
	memcpy_s(pevent->data, iDataLen, cEventMsg, iDataLen);
	
    //printf("length= %d\n",iDataLen);

	shm_event.addr_phy = shm_cache.addr_phy;	
	shm_event.size = shm_cache.size;	
	shm_event.cookie = shm_cache.cookie;	
	nResult = writev(fp, iov, 3);
	if(nResult == -1)
    {
        printf("writev fail\n");
        goto EVENT_FAIL;
    }   
	munmap(pevent, sizeof(LABEL_EVENT_DATA_S) + iDataLen);

    return 0;
EVENT_FAIL:
    if(pevent)
    {
        munmap(pevent, sizeof(LABEL_EVENT_DATA_S) + iDataLen);
    }

    return -1;
	
}

/* 视频服务读现成*/
void * SDC_ReadFromVideoService(void *arg)       
{
    //struct member *temp;
    int iReadLen = 0;
    char cMsgReadBuf[2048];
    sdc_common_head_s *pstSdcMsgHead = NULL;
    //sdc_extend_head_s *extend_head  = NULL;
    char *pucSdcYuvData = NULL;
    //sdc_yuv_data_s *pstYuvData = NULL;
    int iQueueState;
    int iQueueStoreNum = 0;
    
    /* 线程pthread开始运行 */
    printf("SDC_ReadFromVideoService pthread start!\n");

    pstSdcMsgHead = (sdc_common_head_s *)cMsgReadBuf;
        
    while(1)
    {
        if (g_stSystemManage.uiSystemState != SYS_STATE_NORMAL)
        {
            usleep(10000);
            continue;
        }

        g_uiRecvPos =  1;

        //printf("*******1111111111******\n");

        iQueueStoreNum = QUE_GetQueueSize(&g_ptrQueue);
        if (iQueueStoreNum > 1)
        {              
            //printf("Recv yuv data iQueueStoreNum:%d!\n", iQueueStoreNum); 
            //usleep(100000);
            continue;
        }
                
        iReadLen = read(fd_video, (void *)cMsgReadBuf, 2048);

        //printf("*******22222222******\n");
        
        g_uiRecvPos = 2;
        
        if (iReadLen < 0)
        {
            fprintf(stdout,"read fail response:%d,url:%d,code:%d, method:%d\n",
                pstSdcMsgHead->response,pstSdcMsgHead->url,pstSdcMsgHead->code, pstSdcMsgHead->method);
            
            continue;
        }


        //fprintf(stdout,"read succeed response:%d,url:%d,code:%d, method:%d, iReadLen:%d\n",
            //pstSdcMsgHead->response,pstSdcMsgHead->url,pstSdcMsgHead->code, pstSdcMsgHead->method, iReadLen);

        switch (pstSdcMsgHead->url)
        {
            case SDC_URL_YUV_CHANNEL:
            
            //fprintf(stdout,"Read Rsp  url:%d\n ",pstSdcMsgHead->url);
            break;
            
            case SDC_URL_YUV_DATA:
 
            
            //fprintf(stdout,"Read Rsp  url:%d, response:%d, content_length:%d\n",
                //pstSdcMsgHead->url, pstSdcMsgHead->response, pstSdcMsgHead->content_length);
 
            if (pstSdcMsgHead->content_length != 0)
            {
                #if 0
                sdc_for_each_extend_head(pstSdcMsgHead, extend_head) 
                {
                    //clock_gettime(CLOCK_BOOTTIME, &time1);
                    //fprintf(stdout,"time33333333333:%lu, %lu\n", time1.tv_sec, time1.tv_nsec);
                    SDC_DisplayExtendHead(extend_head);
                }
                #endif
 
                pucSdcYuvData = &cMsgReadBuf[pstSdcMsgHead->head_length];

                g_uiRecvNum += pstSdcMsgHead->content_length/sizeof(sdc_yuv_data_s);
 
                //pstYuvData = (sdc_yuv_data_s *)pucSdcYuvData;
                //fprintf(stdout,"Recv yuv data channel:%d, pts:%ld frameNum:%d\n ", 
                    //pstYuvData->channel, pstYuvData->pts, pstSdcMsgHead->content_length/sizeof(sdc_yuv_data_s)); 
                iQueueState = QUE_PushQueue(&g_ptrQueue, pucSdcYuvData);                
                if (iQueueState != QUEUE_STATE_OK)
                {
                    printf("QUE_PushQueue State:%d!\n", iQueueState); 

                    SDC_YuvDataFree(fd_video, (sdc_yuv_data_s *)pucSdcYuvData);
                    g_uiFreeNum++;
                    usleep(5000);
                    
                }
                else
                {

                }
            }
            else
            {
                //
            }
 
            break;
            
            case SDC_URL_VENC_DATA:
            fprintf(stdout,"Read Rsp  url:%d\n ",pstSdcMsgHead->url);
            break;
            
            case SDC_URL_YUV_SNAP:
            fprintf(stdout,"Read Rsp  url:%d\n ",pstSdcMsgHead->url);
            break;
            
            case SDC_URL_RED_LIGHT_ENHANCED:
            fprintf(stdout,"Read Rsp  url:%d\n ",pstSdcMsgHead->url);
            break;
            
            default:
            {
                fprintf(stdout,"Read From Video Services ,unknow url:%d\n ",pstSdcMsgHead->url);
            }
            break;
        }
        g_uiRecvPos = 3;

    }
                                         
    return NULL;                         
}

/* YUV数据帧处理*/
void * SDC_YuvDataProc(void *arg)       
{
    sdc_yuv_data_s stSdcYuvData;
    int iQueueState;
    SDC_SSD_INPUT_SIZE_S InputSize;
	SDC_SSD_RESULT_S stResult;
    sdc_yuv_frame_s sdcRgb;
    VW_YUV_FRAME_S rgbimg; 
    int iRetCode = OK;
	UINT32 idx = 0; 
	uint64_t pts;
    struct timespec time1 = {0, 0};
    //struct timespec time2 = {0, 0};
    struct timespec time3 = {0, 0};
    unsigned int uiTimeCout = 0;

	int i = 0;
	int iLength = 0;
	int iObjectNum = 0;
	META_INFO_S astMetaInfo[10] = {0};
	char cLabelSendBuf[4096] = {0};
	char *pcTemp = NULL;
	int iTagLen = 0;
	char auTempBuf[32]={0};
        
    printf("SDC_YuvDataProc pthread start!\n"); 

     /* 加载初始化模型*/

    while(1)
    {
        if (g_stSystemManage.uiSystemState != SYS_STATE_NORMAL)
        {
            //usleep(10000);
            continue;
        }

        g_uiFreePos = 1;
        
        /*从队列里取出一帧数据进行处理*/
        iQueueState = QUE_PopQueue(&g_ptrQueue, (char *)&stSdcYuvData);
        if (iQueueState != QUEUE_STATE_OK)
        {    
            //printf("QUE_PopQueue State:%d!\n", iQueueState); 
            //usleep(500000);
            continue;
        }
        else
        {
            //printf("get frame with:%d !\n", stSdcYuvData.frame.width);
            if (stSdcYuvData.frame.width != 416)
            {
                /*使用完后释放YUV 数据*/
                SDC_YuvDataFree(fd_video, &stSdcYuvData);
                g_uiFreeNum++;
                printf("get frame with:%d != 416 !\n", stSdcYuvData.frame.width);
                continue;
            }

            clock_gettime(CLOCK_BOOTTIME, &time1);            

            iRetCode = SDC_TransYUV2RGB(fd_algorithm, &(stSdcYuvData.frame), &sdcRgb);

			pts = stSdcYuvData.pts;			
            SDC_YuvDataFree(fd_video, &stSdcYuvData);

            g_uiFreeNum++;     
            
            if (iRetCode != OK)
            {
                printf("Err in SDCIveTransYUV2RGB!\n");
                continue;
            }

            SDC_Struct2RGB(&sdcRgb, &rgbimg); 
            //clock_gettime(CLOCK_BOOTTIME, &time2);
            //uiTimeCout = (unsigned int)(time2.tv_sec - time1.tv_sec)*1000 + (unsigned int)((time2.tv_nsec - time1.tv_nsec)/1000000);
            //fprintf(stdout,"time111111:%d\n", uiTimeCout);
            
                
            InputSize.ImageWidth = 416;
            InputSize.ImageHeight = 416;
 
            stResult.numOfObject = 10;
            stResult.thresh = thresh;
            stResult.pObjInfo = (SDC_SSD_OBJECT_INFO_S *)malloc(stResult.numOfObject * sizeof(SDC_SSD_OBJECT_INFO_S));
 
            if (SDC_SVP_ForwardBGR(rgbimg.pYuvImgAddr, &stResult, InputSize) != OK)
            {
                printf("Err in SDC_SVP_ForwardBGR!\n");
                continue;
            }

            //clock_gettime(CLOCK_BOOTTIME, &time3);
            //uiTimeCout = (unsigned int)(time3.tv_sec - time2.tv_sec)*1000 + (unsigned int)((time3.tv_nsec - time2.tv_nsec)/1000000);
            //fprintf(stdout,"time2222222:%d\n", uiTimeCout);

                /*print result, this sample has 21 classes:
                             class 0:background     class 1:plane           class 2:bicycle
                             class 3:bird           class 4:boat            class 5:bottle
                             class 6:bus            class 7:car             class 8:cat
                             class 9:chair          class10:cow             class11:diningtable
                             class 12:dog           class13:horse           class14:motorbike
                             class 15:person        class16:pottedplant     class17:sheep

                             class 18:sofa          class19:train           class20:tvmonitor*/
            #if 1
			/*清除所有元数据*/
			SDC_LabelEventDel(fd_event, 0, 0, APP_NAME);
			#endif
			
			idx = 0;	
			memset_s(astMetaInfo,sizeof(META_INFO_S) * 10, 0, sizeof(META_INFO_S) * 10);
            for(i = 0; i < stResult.numOfObject; i++)
            {
                if(stResult.pObjInfo[i].confidence > thresh)
                {
	                if(stResult.pObjInfo[i].x_left < 0) stResult.pObjInfo[i].x_left = 0;
	                if(stResult.pObjInfo[i].y_top < 0) stResult.pObjInfo[i].y_top = 0;
	                if(stResult.pObjInfo[i].w < 0) stResult.pObjInfo[i].w = 0;
	                if(stResult.pObjInfo[i].h < 0) stResult.pObjInfo[i].h = 0;
		
                    #if 1
                    printf("Object[%d] class[%u] confidece[%f] {%03d, %03d, %03d, %03d, %03d, %03d}\n", \
                        i, stResult.pObjInfo[i].class, stResult.pObjInfo[i].confidence, \
                        stResult.pObjInfo[i].x_left * 1920/InputSize.ImageWidth, \
                        stResult.pObjInfo[i].y_top * 1080/InputSize.ImageHeight, \
                        stResult.pObjInfo[i].x_right * 1920/InputSize.ImageWidth, \
                        stResult.pObjInfo[i].y_bottom * 1080/InputSize.ImageHeight, \
                        stResult.pObjInfo[i].w * 1920/InputSize.ImageWidth, \
                        stResult.pObjInfo[i].h * 1080/InputSize.ImageHeight);

					//#else
					astMetaInfo[idx].uclass = stResult.pObjInfo[i].class;
					astMetaInfo[idx].usX = stResult.pObjInfo[i].x_left*10000/InputSize.ImageWidth;
					astMetaInfo[idx].usY = stResult.pObjInfo[i].y_top*10000/InputSize.ImageHeight;
					astMetaInfo[idx].usWidth = stResult.pObjInfo[i].w*10000/InputSize.ImageWidth;
					astMetaInfo[idx].usHeight = stResult.pObjInfo[i].h*10000/InputSize.ImageHeight;
					astMetaInfo[idx].confidence = stResult.pObjInfo[i].confidence;	
					#endif	
                    idx++;
					//break;
                }
            }
			
			iObjectNum = idx;
            //DisplayChannelData(pts, stResult.numOfObject);            
            memset_s(cLabelSendBuf,4096, 0, 4096);

            if (iObjectNum > 0)
        	{
				pcTemp = cLabelSendBuf;
				*(uint32_t *)pcTemp = 1;//add
				pcTemp += sizeof(uint32_t);
				
				strcpy_s(pcTemp, 32, APP_NAME); //app name
				pcTemp += 32;
				
				*(uint64_t *)pcTemp = 0;//id
				pcTemp += sizeof(uint64_t);

				*(uint16_t *)pcTemp = iObjectNum;//polygon_cnt
				pcTemp += sizeof(uint16_t);

				for (i=0; i < iObjectNum; i++)
				{
					*(int32_t *)pcTemp = 0xFF0000;//color = 345455;
					pcTemp += sizeof(int32_t);

					*(int32_t *)pcTemp = 5;//edge_width = 3;
					pcTemp += sizeof(int32_t);

					*(uint32_t *)pcTemp = 0;//pstPolygon->attr = 1;
					pcTemp += sizeof(uint32_t);

					*(int32_t *)pcTemp = 0xFF0000;//pstPolygon->bottom_color = 345455;
					pcTemp += sizeof(int32_t);

					*(int32_t *)pcTemp = 0;//pstPolygon->transparency = 128;
					pcTemp += sizeof(int32_t);

					*(int32_t *)pcTemp = 4;//pstPolygon->iPointcnt = 4;
					pcTemp += sizeof(int32_t);

					
                    /*以下是矩形框坐标*/
					*(uint32_t *)pcTemp = astMetaInfo[i].usX;//x1;
					pcTemp += sizeof(uint32_t);

					*(uint32_t *)pcTemp = astMetaInfo[i].usY;//y1;
					pcTemp += sizeof(uint32_t);

					*(uint32_t *)pcTemp = astMetaInfo[i].usX;//x2;
					pcTemp += sizeof(uint32_t);

					*(uint32_t *)pcTemp = astMetaInfo[i].usY + astMetaInfo[i].usHeight;//y2;
					pcTemp += sizeof(uint32_t);

					*(uint32_t *)pcTemp = astMetaInfo[i].usX + astMetaInfo[i].usWidth;//x3;
					pcTemp += sizeof(uint32_t);

					*(uint32_t *)pcTemp = astMetaInfo[i].usY + astMetaInfo[i].usHeight;//y3;
					pcTemp += sizeof(uint32_t);

					*(uint32_t *)pcTemp = astMetaInfo[i].usX + astMetaInfo[i].usWidth;//x4;
					pcTemp += sizeof(uint32_t);

					*(uint32_t *)pcTemp = astMetaInfo[i].usY;//y4;
					pcTemp += sizeof(uint32_t);
				}

				/*给tag_cnt 赋值，一个目标两条字串，置信度和目标*/ 
				//iObjectNum = 1;
				*pcTemp = iObjectNum * 2;//tag_cnt
				pcTemp++;
								
				for (i=0; i < iObjectNum; i++)
				{
					*(int32_t *)pcTemp = 0xFF0000;//color = 345455;
					pcTemp += sizeof(int32_t);
					
					strcpy_s(pcTemp, 32, "宋体"); //font
					pcTemp += 32;

					*(int32_t *)pcTemp = 16;//size
					pcTemp += sizeof(int32_t);

					*(uint32_t *)pcTemp = astMetaInfo[i].usX + 25;//pos-x
					pcTemp += sizeof(uint32_t);

					*(uint32_t *)pcTemp = astMetaInfo[i].usY + 25;//pos-y
					pcTemp += sizeof(uint32_t);	

					switch(astMetaInfo[i].uclass)
					{
					    case 1:
						iTagLen = 20;
						*(uint32_t *)pcTemp = iTagLen;//len
					    pcTemp += sizeof(uint32_t);				 	
								
					    strcpy_s(pcTemp, 127, "安全帽"); 
						pcTemp += iTagLen;
						
						break;
						
						case 2:
						iTagLen = 20;
						*(uint32_t *)pcTemp = iTagLen;//len
					    pcTemp += sizeof(uint32_t);				 	
								
					    strcpy_s(pcTemp, 127, "未戴安全帽"); 
						pcTemp += iTagLen;
						
						break;
						
						
						default:
						iTagLen = 20;//sizeof("未知");
						*(uint32_t *)pcTemp = iTagLen;//len
					    pcTemp += sizeof(uint32_t);				 	
								
					    strcpy_s(pcTemp, 127, "未知"); 
						pcTemp += iTagLen;
						
						//fprintf(stderr, "class:%d\r\n",stResult.pObjInfo[i].class);
						break;
					}

					*(uint32_t *)pcTemp = 0xFF0000;//color = 345455;
					pcTemp += sizeof(uint32_t);
					
					strcpy_s(pcTemp, 32, "宋体"); //font
					pcTemp += 32;

					*(uint32_t *)pcTemp = 16;//size
					pcTemp += sizeof(uint32_t);

					*(uint32_t *)pcTemp = astMetaInfo[i].usX + 25;//pos-x
					pcTemp += sizeof(uint32_t);


					*(uint32_t *)pcTemp = astMetaInfo[i].usY + 300;//pos-y
					pcTemp += sizeof(uint32_t);		
					
					/*先增加置信度内容*/

					memset_s(auTempBuf, 32,0,32);
					sprintf(auTempBuf, "置信度: %2.2f%%", (float)astMetaInfo[i].confidence*100);
					
					iTagLen = sizeof(auTempBuf);
					*(uint32_t *)pcTemp = iTagLen;//len
				    pcTemp += sizeof(uint32_t);				 	
							
				    strcpy_s(pcTemp, 32, auTempBuf); 
					pcTemp += iTagLen;		
				}

				/*超时时间*/
				*pcTemp = 1;
				pcTemp++;
				iLength = (int)(pcTemp - cLabelSendBuf);

				iRetCode = SDC_LabelEventPublish(fd_event, 0, iLength, cLabelSendBuf, pts);
				if (iRetCode != OK)
	            {
	                printf("Err in SDC_LabelEventPublish!\n");
	            }
        	}

            //printf("\nscesss!!!   stResult.numOfObject==%d   \n",stResult.numOfObject);
            free(stResult.pObjInfo);

            (void)SDC_TransYUV2RGBRelease(fd_algorithm, &sdcRgb);

        }
       
        g_uiFreePos = 5;
		
        clock_gettime(CLOCK_BOOTTIME, &time3);
        uiTimeCout = (unsigned int)(time3.tv_sec - time1.tv_sec)*1000 + (unsigned int)((time3.tv_nsec - time1.tv_nsec)/1000000);
        fprintf(stdout,"forward_time:%d\n", uiTimeCout);
 
        //fprintf(stdout,"SDC used complete YuvDataFree\n "); 
    }
   
    return NULL;                         
}                                        

                                   
/* main函数 */                             
int main(int argc,char* argv[])
{
	int nret;
	sdc_hardware_id_s stHardWareParas;
	unsigned int uiYuvChnId = 1;
	
	pthread_t tidpRecv;    
    pthread_t tidpProc;
    struct member *b; 
    char *pcModelName = "./lib/yolov3_safetyhat.wk";

    g_stSystemManage.uiSystemState = SYS_STATE_IDLE;
    g_stSystemManage.uiMaxUsedBufNum = 10;

    /*打开文件服务连接*/
	nret = SDC_ServiceCreate();
	if(nret)
    {
        fprintf(stderr, "nret:%d Err in SDC_ServiceCreate!\r\n", nret);
        return ERR;  
    }
	
    nret = QUE_CreateQueue(&g_ptrQueue, MAX_YUV_BUF_NUM);
    if(nret != OK) 
    {
        fprintf(stderr, "Err in QUE_CreateQueue!\r\n");
        return ERR;  
    }

    /*获取ID信息*/
    if (SDC_GetHardWareId(&stHardWareParas) == OK) 
    {
        fprintf(stdout,"SDC_GetHardWareId Succeed: %s\n",stHardWareParas.id);
    }
    else
    {
        fprintf(stdout,"Err in SDC_GetHardWareId\n");
        return ERR;
    }

	/*动态获取一个yuv通道，多APP执行时需要跳过占用的通道*/
	if (SDC_YuvChnAttrGetIdleYuvChn(fd_video, &uiYuvChnId) != 0)
    {
        fprintf(stdout,"Err in SDC_YuvChnAttrGetIdleYuvChn\n");
        return ERR;
    }

    if (SDC_YuvChnAttrSet(fd_video, uiYuvChnId) != OK)
    {
        fprintf(stdout,"Err in SDC_YuvChnAttrSet\n");
        return ERR;
    }


    /*Ssd Load model*/
    fprintf(stdout, "Yolo Load model!\n");
    #if 1   
    nret = SDC_LoadModel(1, pcModelName,&s_stYoloModel.stModel);//SDC_LoadModel_test
    if(nret != OK) 
    {
        fprintf(stdout, "Err in SDC_LoadModel!\r\n");
        return ERR;  
    }
    
    stNnieCfg_SDC.u32MaxInputNum = 1; //max input image num in each batch
    stNnieCfg_SDC.u32MaxRoiNum = 0;
    stNnieCfg_SDC.aenNnieCoreId[0] = SVP_NNIE_ID_0;//set NNIE core

    fprintf(stdout, "Yolo parameter initialization!\n");
    
    s_stYoloNnieParam.pstModel = &s_stYoloModel.stModel;



    nret = SAMPLE_SVP_NNIE_Yolov3_ParamInit(fd_utils, &stNnieCfg_SDC,&s_stYoloNnieParam,&s_stYolov3SoftwareParam);
    
    // printf("\nnumclass====%d\n",s_stYolov3SoftwareParam.stClassRoiNum.unShape.stWhc.u32Width);
    if(nret != OK) 
    {
        fprintf(stdout, "Error,SAMPLE_SVP_NNIE_yolov3_ParamInit failed!\n");
        return ERR;  
    }
  
    #endif    
    
	/* 为结构体变量b赋值 */                      
	b = (struct member *)malloc(sizeof(struct member));           
	b->num=1;                            
	b->name="SDC_ReadFromVideoService";  


    g_stSystemManage.uiSystemState = SYS_STATE_STARTING;
					  
    /* 创建线程处理YUV数据*/                    
	if ((pthread_create(&tidpProc, NULL, SDC_YuvDataProc, (void*)b)) == ERR)
	{                                    
		 fprintf(stderr, "create error!\n");       
		 return ERR;                        
	}

    /* 创建线程读视频服务线程 */                    
	if ((pthread_create(&tidpRecv, NULL, SDC_ReadFromVideoService, (void*)b)) == ERR)
	{                                    
		 fprintf(stderr, "create error!\n");       
		 return ERR;                        
	}  
									  
	/* 令线程pthread先运行 */                  
	sleep(2);
    g_stSystemManage.uiSystemState = SYS_STATE_NORMAL;
    
    if (SDC_YuvDataReq(fd_video, 2, uiYuvChnId, g_stSystemManage.uiMaxUsedBufNum) != OK)
    {
        fprintf(stdout,"Err in SDC_YuvDataReq\n");
        return ERR;
    }
									  
	while(1)
	{
	    //fprintf(stderr, "Main Work normal,RecvNum:%d, FreeNum:%d,RecvPos:%d, FreePos:%d\n", g_uiRecvNum, g_uiFreeNum, g_uiRecvPos, g_uiFreePos);  
        //fprintf(stderr, "FrameNum:%d\r\n",QUE_GetQueueSize(&g_ptrQueue));
        sleep(5);  
		//SDC_LabelEventDel(fd_event, 2, 0, APP_NAME);
	}
    
	return 0;
} 

 
