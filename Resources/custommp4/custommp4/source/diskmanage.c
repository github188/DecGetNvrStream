#include "diskmanage.h"
#include "partitionindex.h"
#include "hddcmd.h"
#include "common.h"
#include <string.h>

#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#include <direct.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/hdreg.h>
#include <scsi/scsi.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#endif

static u8 disk_full_policy = DISK_FULL_COVER;

volatile static u8 g_HotPlugFlag = 0;

int set_disk_hotplug_flag(u8 flag)
{
	g_HotPlugFlag = flag;
	printf("set_disk_hotplug_flag:%d\n",flag);
	return 0;
}

//const static int index_phy2log[MAX_HDD_NUM+2] = {4, 10, 9, 8, 7, 5, 3, 6, 2, 1}; //MaoRong
//const static int index_phy2log[MAX_HDD_NUM+2] = {8, 6, 7, 9, 10, 3, 1, 2, 4, 5}; //YueTian 10 sata
//3个sata总线，前两个HUB 1扩5，
const static int index_phy2log[11] = {7, 5, 6, 8, 0, 3, 1, 2, 4, 0, 9}; //YueTian  8 sata



BOOL GetDiskSN(int fd, char *sn, int nLimit)
{
	struct hd_driveid id;
	if(ioctl(fd, HDIO_GET_IDENTITY, &id) < 0)
	{
		perror("HDIO_GET_IDENTITY");
		return FALSE;
	}
	
	strncpy(sn, (char *)id.serial_no, nLimit);
	//printf("Disk Number = %s\n", id.serial_no);
	return TRUE;
}

int GetDiskPhysicalIndex(int fd)
{
	int Bus = -1;
	if(ioctl(fd, SCSI_IOCTL_GET_BUS_NUMBER, &Bus) < 0)
	{
		perror("SCSI_IOCTL_GET_BUS_NUMBER failed");
		return -1;
	}
	//printf("###Bus:0x%08x\n",Bus);
	
	struct sg_id
	{
		long	l1;	/* target(scsi_device_id) | lun << 8 | channel << 16 | host_no << 24 */
		long	l2;	/* Unique id */
	}sg_id;
	
	if(ioctl(fd, SCSI_IOCTL_GET_IDLUN, &sg_id) < 0)
	{
		perror("SCSI_IOCTL_GET_IDLUN failed");
		return -1;
	}
	//printf("###id:(0x%08lx,0x%08lx)\n",sg_id.l1,sg_id.l2);
	
	int phy_idx = (Bus * 5) + ((sg_id.l1 >> 16) & 0xff);
	//printf("disk physical index = %d\n", phy_idx);
	
	return phy_idx;
}

int GetDiskLogicIndex(int phy_idx)
{
	//int logic_idx = phy_idx;
	//return logic_idx;
	return index_phy2log[phy_idx];
}

BOOL is_sata_disk(int fd)
{
	char host[64];
	memset(host, 0, sizeof(host));
	
	*(int*)host = 63;
	if(ioctl(fd, SCSI_IOCTL_PROBE_HOST, &host) < 0)
	{
		perror("SCSI_IOCTL_PROBE_HOST failed");
		return FALSE;
	}
	
	//hdd:host=sata_sil hi3515-ahci-device ahci_platform
	//usb:host=SCSI emulation for USB Mass Storage devices
	host[63] = '\0';
	//printf("SCSI_IOCTL_PROBE_HOST-1:%s\n", host);
	if((strstr(host, "ahci")) || (strstr(host, "sata_sil")) || (strstr(host, "hi3515-ahci-device")) || (strstr(host, "atp862x_His")))
	{
		return TRUE;
	}
	
	return FALSE;
}

BOOL is_usb_disk(int fd)
{
	char host[64];
	memset(host, 0, sizeof(host));
	
	*(int*)host = 63;
	if(ioctl(fd, SCSI_IOCTL_PROBE_HOST, &host) < 0)
	{
		perror("SCSI_IOCTL_PROBE_HOST failed");
		return FALSE;
	}
	
	//hdd:host=sata_sil hi3515-ahci-device ahci_platform
	//usb:host=SCSI emulation for USB Mass Storage devices
	host[63] = '\0';
	//printf("SCSI_IOCTL_PROBE_HOST-2:%s\n", host);
	if(strstr(host, "USB") || strstr(host, "usb"))
	{
		return TRUE;
	}
	
	return FALSE;
}

int init_disk_manager(disk_manager *hdd_manager)
{
	int ret = -1;
	int fd = -1;
	
	DiskInfo dinfo;
	int nDiskFound = 0;
	
	char diskname[64];
	char devname[64];
	char mountname[64];
	
	memset(hdd_manager,0,sizeof(disk_manager));
	
	int k = 0;
	for(k = 0; k < (MAX_HDD_NUM + 2); k++)
	{
		sprintf(diskname,"/dev/sd%c",'a'+k);
		ret = ifly_diskinfo(diskname,&dinfo);
		if(ret == 0)
		{
			fd = open(diskname, O_RDONLY);
			if(fd == -1)
			{
				printf("init_disk_manager %s is not exist\n",diskname);
				return -1;
			}
			
			printf("检测到磁盘:%s,cap:%dM\n",diskname,(int)(dinfo.capability/(1024*1024)));
			
			u8 storage_type = 's';
			
			int ret = is_sata_disk(fd);
			if(!ret)
			{
				ret = is_usb_disk(fd);
				if(ret)
				{
					storage_type = 'u';
				}
			}
			
			int phy_idx = 0;
			
			char DiskSN[64];
			memset(DiskSN, 0, sizeof(DiskSN));
			
			if(storage_type == 's')
			{
				phy_idx = GetDiskPhysicalIndex(fd);
				GetDiskSN(fd, DiskSN, sizeof(DiskSN)-1);
			}
			
			close(fd);
			
			if(!ret)//既不是SATA盘，也不是U盘或移动硬盘
			{
				return -1;
			}
			
			if(storage_type == 'u')//U盘不是用来录像的
			{
				continue;
			}
			
			//sleep(2);//这里有必要加延时吗???
			
			hdd_manager->hinfo[nDiskFound].is_disk_exist = 1;
			hdd_manager->hinfo[nDiskFound].total = 0;
			hdd_manager->hinfo[nDiskFound].free = 0;
			
			hdd_manager->hinfo[nDiskFound].storage_type = storage_type;
			hdd_manager->hinfo[nDiskFound].disk_physical_idx = phy_idx;
			hdd_manager->hinfo[nDiskFound].disk_logic_idx = GetDiskLogicIndex(phy_idx);
			hdd_manager->hinfo[nDiskFound].disk_system_idx = k;
			strcpy(hdd_manager->hinfo[nDiskFound].disk_name, diskname);
			strcpy(hdd_manager->hinfo[nDiskFound].disk_sn, DiskSN);
			
			int i = 0;
			for(i = 0; i < MAX_PARTITION_NUM; i++)
			{
				sprintf(devname,"%s%d",diskname,i+1);
				fd = open(devname,O_RDONLY);
				if(fd != -1)
				{
					close(fd);
					
					hdd_manager->hinfo[nDiskFound].is_partition_exist[i] = 1;
					sprintf(mountname,"rec/%c%d",'a'+nDiskFound,i+1);//newdisk???
					
					int rtn = mkdir(mountname,1);
					if(rtn < 0)
					{
						printf("create dir [%s] failed,err:(%d,%s)\n",mountname,errno,strerror(errno));
					}
					else
					{
						//printf("create dir [%s] success\n",mountname);
					}
					
					//printf("before mount_user_0:%s...\n",devname);
					mount_user(devname, mountname);
					printf("after mount_user_0:%s...\n",devname);
					
					init_partition_index(&hdd_manager->hinfo[nDiskFound].ptn_index[i],mountname);
					//printf("after init_partition_index_0:%s...\n",mountname);
					
					hdd_manager->hinfo[nDiskFound].total += (long)(get_partition_total_space(&hdd_manager->hinfo[nDiskFound].ptn_index[i])/1000);
					hdd_manager->hinfo[nDiskFound].free += (long)(get_partition_free_space(&hdd_manager->hinfo[nDiskFound].ptn_index[i])/1000);
				}
				else
				{
					//sleep(2);//这里有必要加延时吗???
					
					printf("partition:%s open faild\n",devname);
					
					break;
				}
			}
			
			nDiskFound++;
			if(nDiskFound == MAX_HDD_NUM)
			{
				break;
			}
		}
	}
	
	printf("init_disk_manager: nDiskFound = %d\n",nDiskFound);
	
	return 1;
}

int get_disk_info(disk_manager *hdd_manager,int disk_index)
{
	int i;
	//printf("get_disk_info\n");
	dbgprint("get_disk_info:i addr:0x%08x,disk_index addr:0x%08x,hdd_manager addr:0x%08x\n",(int)&i,(int)&disk_index,(int)&hdd_manager);
	HddInfo *phinfo = &hdd_manager->hinfo[disk_index];
	if(phinfo->is_disk_exist)
	{
		phinfo->total = 0;
		phinfo->free = 0;
		for(i=0;i<MAX_PARTITION_NUM;i++)
		{
			//printf("%s yg 1\n", __func__);
			phinfo->total += (long)(get_partition_total_space(&phinfo->ptn_index[i])/1000);
			//printf("%s yg 2\n", __func__);
			phinfo->free += (long)(get_partition_free_space(&phinfo->ptn_index[i])/1000);
			//printf("%s disk%d:total=%ld,free=%ld\n",__func__,disk_index,phinfo->total,phinfo->free);
			dbgprint("disk%d:total=%ld,free=%ld\n",disk_index,phinfo->total,phinfo->free);
		}
	}
	return 1;
}

partition_index* get_rec_path(disk_manager *hdd_manager,char *pPath,u32 *open_offset,int chn,u8 *pdisk_system_idx)
{
	int i,j;
	int file_no,sect_offset;
	HddInfo *phinfo;
	u32 min_end_time = (u32)(-1);
	u32 end_time;
	int cover_disk = -1;
	int cover_ptn = -1;
	
	for(i=0;i<MAX_HDD_NUM;i++)
	{
		phinfo = &hdd_manager->hinfo[i];
		if(phinfo->is_disk_exist)
		{
			for(j=0;j<MAX_PARTITION_NUM;j++)
			{
				if(phinfo->is_partition_exist[j])
				{
					dbgprint("before:get_chn_next_segment\n");
					if(get_chn_next_segment(&phinfo->ptn_index[j],chn,&file_no,&sect_offset))
					{
						dbgprint("after1:get_chn_next_segment\n");
						dbgprint("disk partition=sd%c%d,file_no=%d,sect_offset=%d,chn=%d\n",'a'+i,j+1,file_no,sect_offset,chn);
						//printf("disk partition=sd%c%d,file_no=%d,sect_offset=%d,chn=%d\n",'a'+i,j+1,file_no,sect_offset,chn);
						//printf("%s:%u--%s\n", __func__, phinfo->disk_system_idx, phinfo->disk_name);
						sprintf(pPath,"rec/%c%d/fly%05d.ifv",'a'+i,j+1,file_no);
						*open_offset = sect_offset;//新文件在文件容器中的开始位置
						dbgprint("***no cover:new record file name:%s,open_offset=%d,chn=%d***\n",pPath,*open_offset,chn);
						
						*pdisk_system_idx = phinfo->disk_system_idx+1;//sda--1 sdb--2
						
						return &phinfo->ptn_index[j];
					}
					dbgprint("after2:get_chn_next_segment\n");
				}
			}
		}
	}
	if(disk_full_policy == DISK_FULL_COVER)
	{
		for(i=0;i<MAX_HDD_NUM;i++)
		{
			phinfo = &hdd_manager->hinfo[i];
			if(phinfo->is_disk_exist)
			{
				for(j=0;j<MAX_PARTITION_NUM;j++)
				{
					if(phinfo->is_partition_exist[j])
					{
						if(get_first_full_file_end_time(&phinfo->ptn_index[j],&end_time))
						{
							if(end_time < min_end_time)
							{
								min_end_time = end_time;
								cover_disk = i;
								cover_ptn = j;
								dbgprint("******************* get_first_full_file_end_time ok.\n");
							}
						}
					}
				}
			}
		}
	}
	if(cover_disk != -1)
	{
		phinfo = &hdd_manager->hinfo[cover_disk];
		if(get_chn_next_segment_force(&phinfo->ptn_index[cover_ptn],chn,&file_no,&sect_offset))
		{
			dbgprint("disk partition=sd%c%d,file_no=%d,sect_offset=%d,chn=%d\n",'a'+cover_disk,cover_ptn+1,file_no,sect_offset,chn);
				sprintf(pPath,"rec/%c%d/fly%05d.ifv",'a'+cover_disk,cover_ptn+1,file_no);
			*open_offset = sect_offset;
			dbgprint("***cover:new record file name:%s,open_offset=%d,chn=%d***\n",pPath,*open_offset,chn);
			//printf("***cover:new record file name:%s,open_offset=%d,chn=%d***\n", pPath,*open_offset,chn);
			//解决所有硬盘录满后所有硬盘的状态显示空闲，但实际上依然在录像
			*pdisk_system_idx = phinfo->disk_system_idx+1;//sda--1 sdb--2
			
			return &phinfo->ptn_index[cover_ptn];
		}
	}
	return NULL;
}

int set_policy_when_disk_full(u8 policy)
{
	if(policy >= DISK_FULL_COVER) policy = DISK_FULL_COVER;
	disk_full_policy = policy;
	return 1;
}

void run(recfileinfo_t* pData,int left,int right)
{
	int i,j;
	u32 middle;
	recfileinfo_t iTemp;
	i = left;
	j = right;
	middle = pData[(left+right)/2].start_time;//pData[(left+right)/2]; //求中间值
	do{
		while((pData[i].start_time > middle) && (i < right))//从左扫描小于中值的数
			i++;
		while((pData[j].start_time < middle) && (j > left))//从右扫描小于中值的数
			j--;
		if(i<=j)//找到了一对值
		{
			//交换
			iTemp = pData[j];
			pData[j] = pData[i];
			pData[i] = iTemp;
			i++;
			j--;
		}
	}while(i<=j);//如果两边扫描的下标交错，就停止（完成一次）
	//当左边部分有值(left<j)，递归左半边
	if(left<j)
		run(pData,left,j);
	//当右边部分有值(right>i)，递归右半边
	if(right>i)
		run(pData,i,right);
}

void QuickSort(recfileinfo_t * pData,int count)
{
	run(pData,0,count-1);
}

int sort_file_with_start_time(recfileinfo_t *fileinfo_buf,int nums)
{
	//printf("sort 1\n");
	QuickSort(fileinfo_buf,nums);
	//printf("sort 2\n");
	return 1;
	
	int i,j;
	recfileinfo_t tmp;
	for(i=0;i<nums-1;i++)
	{
		for(j=0;j<nums-1-i;j++)
		{
			//if(fileinfo_buf[j].start_time > fileinfo_buf[j+1].start_time)
			if(fileinfo_buf[j].start_time < fileinfo_buf[j+1].start_time)//wrchen 081223
			{
				tmp = fileinfo_buf[j];
				fileinfo_buf[j] = fileinfo_buf[j+1];
				fileinfo_buf[j+1] = tmp;
			}
		}
	}
}

//yaogang
void snaprun(recsnapinfo_t* pData,int left,int right)
{
	int i,j;
	u32 middle;
	recsnapinfo_t iTemp;
	i = left;
	j = right;
	middle = pData[(left+right)/2].start_time;//pData[(left+right)/2]; //求中间值
	do{
		while((pData[i].start_time > middle) && (i < right))//从左扫描大于中值的数
			i++;
		while((pData[j].start_time < middle) && (j > left))//从右扫描小于中值的数
			j--;
		if(i<=j)//找到了一对值
		{
			//交换
			iTemp = pData[j];
			pData[j] = pData[i];
			pData[i] = iTemp;
			i++;
			j--;
		}
	}while(i<=j);//如果两边扫描的下标交错，就停止（完成一次）
	//当左边部分有值(left<j)，递归左半边
	if(left<j)
		snaprun(pData,left,j);
	//当右边部分有值(right>i)，递归右半边
	if(right>i)
		snaprun(pData,i,right);
}

void SnapQuickSort(recsnapinfo_t * pData,int count)
{
	snaprun(pData,0,count-1);
}

int sort_snap_with_start_time(recsnapinfo_t *fileinfo_buf,int nums)
{
	//printf("sort 1\n");
	SnapQuickSort(fileinfo_buf,nums);
	//printf("sort 2\n");
	return 1;
	/*
	int i,j;
	recsnapinfo_t tmp;
	for(i=0;i<nums-1;i++)
	{
		for(j=0;j<nums-1-i;j++)
		{
			//if(fileinfo_buf[j].start_time > fileinfo_buf[j+1].start_time)
			if(fileinfo_buf[j].start_time > fileinfo_buf[j+1].start_time)//wrchen 081223
			{
				tmp = fileinfo_buf[j];
				fileinfo_buf[j] = fileinfo_buf[j+1];
				fileinfo_buf[j+1] = tmp;
			}
		}
	}
	*/
}


#ifndef WIN32
#include <sys/time.h>
#endif

//#define PRINT_SEARCH_TIME

//csp modify 20140822
//extern int tl_power_atx_check();

int search_all_rec_file(disk_manager *hdd_manager,search_param_t *search,recfileinfo_t *fileinfo_buf,int max_nums)
{
	u8 i,j;
	HddInfo *phinfo;
	int search_count = 0;
	int ret = 0;
	
	//csp modify 20140822
	if(search->end_time < search->start_time)
	{
		return 0;
	}
	//csp modify 20140822
	/*
	unsigned char cur_atx_flag = tl_power_atx_check();
	if(cur_atx_flag == 0)//电池供电
	{
		return 0;
	}
	*/
	//csp modify 20140822
	if(g_HotPlugFlag)
	{
		return 0;
	}
	
	#ifdef PRINT_SEARCH_TIME
	struct timeval start,end;
	long span;
	#endif
	
	for(i=0;i<MAX_HDD_NUM;i++)
	{
		phinfo = &hdd_manager->hinfo[i];
		
		if(phinfo->is_disk_exist)
		{
			for(j=0;j<MAX_PARTITION_NUM;j++)
			{
				if(phinfo->is_partition_exist[j])
				{
					//printf("start search chn is %d\n",search->channel_no);
					ret = search_rec_file(&phinfo->ptn_index[j], search, fileinfo_buf + search_count, max_nums-search_count, i, j);
					
					if(ret < 0)
					{						
						#ifdef PRINT_SEARCH_TIME
						gettimeofday(&start,NULL);
						#endif
						
						sort_file_with_start_time(fileinfo_buf,max_nums);//wrchen 090520
						
						#ifdef PRINT_SEARCH_TIME
						gettimeofday(&end,NULL);
						span = (end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);
						printf("sort 1.0 files,%d,span:%ld\n",max_nums,span);
						#endif
						
						return -1;
					}
					else if((max_nums-search_count) == ret)
					{						
						search_count += ret;
						sort_file_with_start_time(fileinfo_buf,search_count);//wrchen 090520
						return search_count;
					}
					search_count += ret;
				}
			}
		}
	}
		
	if(search_count)
	{
		#ifdef PRINT_SEARCH_TIME
		gettimeofday(&start,NULL);
		#endif
		
		sort_file_with_start_time(fileinfo_buf,search_count);
		
		#ifdef PRINT_SEARCH_TIME
		gettimeofday(&end,NULL);
		span = (end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);
		printf("sort 1.1 %d files,span:%ld\n",search_count,span);
		#endif
	}
	
	return search_count;
}

int get_rec_file_mp4_name(recfileinfo_t *fileinfo,char *filename,u32 *open_offset)
{
	*open_offset = fileinfo->offset;
	sprintf(filename,"rec/%c%d/fly%05d.mp4",'a'+fileinfo->disk_no,fileinfo->ptn_no+1,fileinfo->file_no);
	return 1;
}

int get_rec_file_name(recfileinfo_t *fileinfo,char *filename,u32 *open_offset)
{
	*open_offset = fileinfo->offset;
	sprintf(filename,"rec/%c%d/fly%05d.ifv",'a'+fileinfo->disk_no,fileinfo->ptn_no+1,fileinfo->file_no);
	return 1;
}

int get_snap_file_name(recsnapinfo_t *fileinfo,char *filename,u32 *open_offset)
{
	*open_offset = fileinfo->offset;
	sprintf(filename,"rec/%c%d/pic%05d.ifv",'a'+fileinfo->disk_no,fileinfo->ptn_no+1,fileinfo->file_no);
	return 1;
}


//yaogang modify 20150105 for snap
partition_index * get_partition(disk_manager *hdd_manager, u8 nDiskNo, u8 nPtnNo)
{
	HddInfo *phinfo = NULL;
	partition_index *index = NULL;

	if (hdd_manager)
	{
		phinfo = &hdd_manager->hinfo[nDiskNo];
		if(phinfo->is_disk_exist)
		{
			index = &phinfo->ptn_index[nPtnNo];
			if (index->valid == 0)
			{
				index = NULL;
			}
		}
	}
	return index;
}

partition_index * get_pic_rec_partition(disk_manager *hdd_manager)
{
	HddInfo *phinfo;
	partition_index *index = NULL;
	int i, j;

	partition_index *earliest_index = NULL;
	time_t tmin;
	
	for(i=0;i<MAX_HDD_NUM;i++)
	{
		phinfo = &hdd_manager->hinfo[i];
		if(phinfo->is_disk_exist)
		{
			for(j=0;j<MAX_PARTITION_NUM;j++)
			{
				if(phinfo->is_partition_exist[j])//phinfo->ptn_index[j]
				{
					index = &phinfo->ptn_index[j];
					if (index->valid)
					{
						lock_partition_index(index);
						//在一次循环中顺便找到结束时间最早的分区，以便后面使用
						if (NULL == earliest_index)
						{
							earliest_index = index;
							tmin = index->pic_header.end_sec;
						}
						else
						{
							if (tmin > index->pic_header.end_sec)
							{
								earliest_index = index;
								tmin = index->pic_header.end_sec;
							}
						}
						//该分区还有未用完的文件容器
						if (index->pic_header.file_cur_no < index->pic_header.file_nums)
						{
							printf("find partition: %s%d, file_cur_no: %d\n", phinfo->disk_name, j, index->pic_header.file_cur_no);
							unlock_partition_index(index);
							return index;
						}
						unlock_partition_index(index);
					}
				}
			}
		}
	}
	//所有分区的文件容器都用完了
	//回卷，找到结束时间最早的分区
	//从该分区第1 个文件容器开始覆盖
	//并且此时该分区的其他文件容器也无效了
	printf("all of partition full, Rollback\n");	
	
	if (NULL == earliest_index)
	{
		printf("Rollback failed, system disk not exist\n");
	}
	else
	{
		init_partition_pic_index(earliest_index);
	}
	return earliest_index;
}

int search_all_rec_snap(disk_manager *hdd_manager,search_param_t *search,recsnapinfo_t *snapinfo_buf,int max_nums)
{
	u8 i,j;
	HddInfo *phinfo;
	int search_count = 0;
	int ret = 0;
	
	//csp modify 20140822
	if(search->end_time < search->start_time)
	{
		return 0;
	}
	//csp modify 20140822
	/*
	unsigned char cur_atx_flag = tl_power_atx_check();
	if(cur_atx_flag == 0)//电池供电
	{
		return 0;
	}
	*/
	//csp modify 20140822
	if(g_HotPlugFlag)
	{
		return 0;
	}
	
	#ifdef PRINT_SEARCH_TIME
	struct timeval start,end;
	long span;
	#endif
	
	for(i=0;i<MAX_HDD_NUM;i++)
	{
		phinfo = &hdd_manager->hinfo[i];
		
		if(phinfo->is_disk_exist)
		{
			for(j=0;j<MAX_PARTITION_NUM;j++)
			{
				if(phinfo->is_partition_exist[j])
				{
					//printf("start search chn is %d\n",search->channel_no);
					ret = search_rec_snap(&phinfo->ptn_index[j], search, snapinfo_buf + search_count, max_nums-search_count, i, j);
					
					if(ret < 0)
					{						
						#ifdef PRINT_SEARCH_TIME
						gettimeofday(&start,NULL);
						#endif
						
						sort_snap_with_start_time(snapinfo_buf,max_nums);//wrchen 090520
						
						#ifdef PRINT_SEARCH_TIME
						gettimeofday(&end,NULL);
						span = (end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);
						printf("sort 1.0 files,%d,span:%ld\n",max_nums,span);
						#endif
						
						return -1;
					}
					else if((max_nums-search_count) == ret)
					{						
						search_count += ret;
						sort_snap_with_start_time(snapinfo_buf,search_count);//wrchen 090520
						return search_count;
					}
					search_count += ret;
				}
			}
		}
	}
		
	if(search_count)
	{
		#ifdef PRINT_SEARCH_TIME
		gettimeofday(&start,NULL);
		#endif
		
		sort_snap_with_start_time(snapinfo_buf,search_count);
		
		#ifdef PRINT_SEARCH_TIME
		gettimeofday(&end,NULL);
		span = (end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);
		printf("sort 1.1 %d files,span:%ld\n",search_count,span);
		#endif
	}
	
	return search_count;
}

//预录
//第一个硬盘的第一个分区的126/127文件容器做预录
partition_index * get_pre_rec_partition(disk_manager *hdd_manager)
{
	HddInfo *phinfo;
	partition_index *index = NULL;
	int i, j;
	
	for(i=0;i<MAX_HDD_NUM;i++)
	{
		phinfo = &hdd_manager->hinfo[i];
		if(phinfo->is_disk_exist)
		{
			for(j=0;j<MAX_PARTITION_NUM;j++)
			{
				if(phinfo->is_partition_exist[j])//phinfo->ptn_index[j]
				{
					index = &phinfo->ptn_index[j];
					if (index->valid)
					{
						return index;
					}
				}
			}
		}
	}

	return NULL;
}

