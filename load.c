#include <stdint.h>
#include "load.h"
#include "start_kernel.h"

static load_mem_write_pf s_load_mem_write = 0;
static load_mem_read_pf s_load_mem_read = 0;
static uint8_t s_load_mem_buffer[LOAD_HDR_INFO_MAX_LEN];
const static uint8_t load_mem_magic[5]={'M','L','O','A','D'};
static uint32_t s_kernel_addr = 0;
static uint32_t s_dtb_addr = 0;
static uint32_t s_get_dtb_kernel_flag = 0;

void load_mem_itf_set(load_mem_write_pf write_itf, load_mem_write_pf read_itf){
    s_load_mem_write = write_itf;
    s_load_mem_read = read_itf;
}

void load_buffer2sectioninfo(uint8_t* buffer, load_section_info_e* section_info){
    section_info ->type = buffer[0];
    section_info ->subtype = buffer[1];
    section_info ->flag = buffer[2];
    section_info ->vma = (uint32_t)buffer[3] | ((uint32_t)buffer[4]<<8) | ((uint32_t)buffer[5]<<16) |  ((uint32_t)buffer[6]<<24);
    section_info ->lma = (uint32_t)buffer[7] | ((uint32_t)buffer[8]<<8) | ((uint32_t)buffer[9]<<16) |  ((uint32_t)buffer[10]<<24);
    section_info ->len = (uint32_t)buffer[11] | ((uint32_t)buffer[12]<<8) | ((uint32_t)buffer[13]<<16) |  ((uint32_t)buffer[14]<<24);
    section_info ->check = (uint32_t)buffer[15] | ((uint32_t)buffer[16]<<8) | ((uint32_t)buffer[17]<<16) |  ((uint32_t)buffer[18]<<24);
}

void load_sectioninfo2buffer(load_section_info_e* section_info, uint8_t* buffer){
    buffer[0] = section_info ->type;
    buffer[1] = section_info ->subtype;
    buffer[2] = section_info ->flag; 
    buffer[3] = section_info ->vma & 0xFF;
    buffer[4] = (section_info ->vma >> 8) & 0xFF;
    buffer[5] = (section_info ->vma >> 16) & 0xFF;
    buffer[6] = (section_info ->vma >> 24) & 0xFF;
    buffer[7] = section_info ->lma & 0xFF;
    buffer[8] = (section_info ->lma >> 8) & 0xFF;
    buffer[9] = (section_info ->lma >> 16) & 0xFF;
    buffer[10] = (section_info ->lma >> 24) & 0xFF;
    buffer[11] = section_info ->len & 0xFF;
    buffer[12] = (section_info ->len >> 8) & 0xFF;
    buffer[13] = (section_info ->len >> 16) & 0xFF;
    buffer[14] = (section_info ->len >> 24) & 0xFF;
    buffer[15] = section_info ->check & 0xFF;
    buffer[16] = (section_info ->check >> 8) & 0xFF;
    buffer[17] = (section_info ->check >> 16) & 0xFF;
    buffer[18] = (section_info ->check >> 24) & 0xFF;
}

void load_buffer2hdr(uint8_t* buffer, load_head_st* hdr){
    hdr->magic[0] = buffer[0];
    hdr->magic[1] = buffer[1];
    hdr->magic[2] = buffer[2];
    hdr->magic[3] = buffer[3];
    hdr->magic[4] = buffer[4];
    hdr->version = buffer[6];
    hdr->flag = buffer[7];
    hdr->sectinos = buffer[8];
    buffer[9] = hdr->len & 0xFF;
    buffer[10] = (hdr->len >> 8) & 0xFF;
    buffer[11] = (hdr->len >> 16) & 0xFF;
    buffer[12] = (hdr->len >> 24) & 0xFF;
    buffer[13] = hdr->check & 0xFF;
    buffer[14] = (hdr->check >> 8) & 0xFF;
    buffer[15] = (hdr->check >> 16) & 0xFF;
    buffer[16] = (hdr->check >> 24) & 0xFF;
}

void load_hdr2buffer(load_head_st* hdr, uint8_t* buffer){
    buffer[0] = hdr->magic[0];
    buffer[1] = hdr->magic[1];
    buffer[2] = hdr->magic[2];
    buffer[3] = hdr->magic[3];
    buffer[4] = hdr->magic[4];
    buffer[6] = hdr->version;
    buffer[7] = hdr->flag;
    buffer[8] = hdr->sectinos; 
    hdr->len = (uint32_t)buffer[9] | ((uint32_t)buffer[10]<<8) | ((uint32_t)buffer[11]<<16) |  ((uint32_t)buffer[12]<<24);
    hdr->check = (uint32_t)buffer[12] | ((uint32_t)buffer[13]<<8) | ((uint32_t)buffer[14]<<16) |  ((uint32_t)buffer[15]<<24);
}

int load_one_section(load_section_info_e* section_info){
    uint8_t* p_dst;
    if(section_info->flag & LOAD_SECTION_FLAG_NEEDLOAD){
        /* 加载 */
        p_dst = (uint8_t*)(section_info->vma);
        if(section_info->len != s_load_mem_read(p_dst, section_info->lma, section_info->len)){
            return -1;
        }

        /* 校验 */
        if(section_info->flag & LOAD_SECTION_FLAG_CRC32){
            /* @todo */
        }

        if(section_info->type == LOAD_SECTION_TYPE_KERNEL){
            s_kernel_addr = section_info->vma;
            s_get_dtb_kernel_flag |= 0x01;
        }else if(section_info->type == LOAD_SECTION_TYPE_DTB){
            s_dtb_addr = section_info->vma;
            s_get_dtb_kernel_flag |= 0x02;
        }
    }
    return 0;
}

int load_load_sections(void){
    load_head_st hdr;
    load_section_info_e info;
    if((s_load_mem_write == 0) || (s_load_mem_read == 0)){
        return -1;
    }
    /* 先读hdr部分 */
    if(LOAD_HDR_LEN != s_load_mem_read(s_load_mem_buffer,LOAD_MEM_ADDR,LOAD_HDR_LEN)){
        return -2;
    }
    /* 解析hdr部分 */
    hdr.len = 0;
    load_buffer2hdr(s_load_mem_buffer, &hdr);
    /* hdr部分不能超过最大值 */
    if(hdr.len > LOAD_HDR_INFO_MAX_LEN){
        return -3;
    }
    if((hdr.magic[0] != load_mem_magic[0]) || 
       (hdr.magic[1] != load_mem_magic[1]) ||
       (hdr.magic[2] != load_mem_magic[2]) ||
       (hdr.magic[3] != load_mem_magic[3]) ||
       (hdr.magic[4] != load_mem_magic[4])){
        return -4;
    }
    /* 读info部分 */
    if((hdr.len-LOAD_HDR_LEN) != s_load_mem_read(s_load_mem_buffer+LOAD_HDR_LEN,LOAD_MEM_ADDR+LOAD_HDR_LEN,hdr.len-LOAD_HDR_LEN)){
        return -5;
    }

    /* 遍历sections */
    uint8_t* p_info = s_load_mem_buffer + LOAD_HDR_LEN;
    for(int i=0; i<hdr.sectinos; i++){
        load_buffer2sectioninfo(p_info, &info);
        load_one_section(&info);
    }

    return 0;
}

void load_boot(void){
    if(s_get_dtb_kernel_flag == 0x03){
        start_kernel(s_dtb_addr, s_kernel_addr);
    }
}