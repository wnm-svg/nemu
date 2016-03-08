#include "common.h"
#include <stdlib.h>
#include "burst.h"
uint32_t dram_read(hwaddr_t, size_t);
void dram_write(hwaddr_t, size_t, uint32_t);
void ddr3_read(hwaddr_t, void *);
/*
cache block存储空间的大小为64B
cache存储空间的大小为64KB
8-way set associative
标志位只需要valid bit即可
替换算法采用随机方式
write through
not write allocate*/
/* Memory accessing interfaces */
struct Cache
{
	bool valid;
	int tag;
	uint8_t data[64];
}cache[1024];


uint32_t cache_read(hwaddr_t addr, size_t len) 
{
	uint32_t g = (addr>>6) & 0x7f; //group number
	uint32_t offset = addr & 0x3f; // inside addr
	uint8_t temp[4];
	int i;
	bool v = false;
	memset (temp,0,sizeof (temp));
	for (i = g * 8 ; i < (g+1) * 8 ;i ++)
	{
		if (cache[i].tag == (addr >> 13)&& cache[i].valid)
			{
				v = true;
				break;
			}
	}
	if (!v)
	{
		for (i = g * 8 ; i < (g+1)*8 ;i ++)
		{
			if (!cache[i].valid)break;
		}
		if (i == (g + 1) * 8)//ramdom
		{
			srand (0);
			i = g * 8 + rand()%8;
		}
		cache[i].valid = true;
		cache[i].tag = addr>>13;
		int j;
		for (j = 0;j < 8;j ++)
		ddr3_read(addr + j * BURST_LEN , cache[i].data + j * BURST_LEN);
		if (addr > 0x100010)
		for (j = 0;j < 64;j ++)
			Log ("addr 0x%x : 0x%x",addr + j,cache[i].data[j]);
		
	}
	Log ("offset = 0x%x",offset);
	if (offset + len >=64 ) 
	{
		Log ("out of block!");
		memcpy(temp,cache[i].data + offset, 64 - offset);
		memcpy(temp + 64 - offset,cache[i + 1].data, len - (64 - offset));
	}
	else{
		memcpy(temp,cache[i].data + offset,len);
	}
	Log ("temp %x %x %x %x",temp[0],temp[1],temp[2],temp[3]);
	int zero = 0;
	return unalign_rw(temp + zero, 4);
}

void cache_write(hwaddr_t addr, size_t len,uint32_t data) {
	uint32_t g = (addr>>6) & 0x7f; //group number
	uint32_t offset = addr & 0x3f; // inside addr
	int i;
	for (i = g * 8 ; i < (g+1)*8 ;i ++)
	{
		if (cache[i].tag == (addr >> 13)&& cache[i].valid)
			{
				memcpy (cache[i].data + offset , &data , len);
			}
	}
}
uint32_t hwaddr_read(hwaddr_t addr, size_t len) {
	Log ("read 0x%x len %d",addr,(int)len);
	uint32_t tmp = cache_read (addr,len) & (~0u >> ((4 - len) << 3)); 
	uint32_t ground = dram_read(addr, len) & (~0u >> ((4 - len) << 3));
	Log ("cache = 0x%x , dram = 0x%x , len = %d\n",tmp,ground,(int)len);
	Assert (tmp == ground,"cache = 0x%x , dram = 0x%x , len = %d\n",tmp,ground,(int)len);
	return dram_read(addr, len) & (~0u >> ((4 - len) << 3));
}

void hwaddr_write(hwaddr_t addr, size_t len, uint32_t data) {
	Log ("write 0x%x = %x",addr,data);
	cache_write(addr, len, data);
	dram_write(addr, len, data);
}

uint32_t lnaddr_read(lnaddr_t addr, size_t len) {
	return hwaddr_read(addr, len);
}

void lnaddr_write(lnaddr_t addr, size_t len, uint32_t data) {
	hwaddr_write(addr, len, data);
}

uint32_t swaddr_read(swaddr_t addr, size_t len) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif
	return lnaddr_read(addr, len);
}

void swaddr_write(swaddr_t addr, size_t len, uint32_t data) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif
	lnaddr_write(addr, len, data);
}

