#include <stdint.h>
#include "load.h"

void load_buffer2sectioninfo(uint8_t* buffer, load_section_info_st* section_info){
    section_info ->type = buffer[0];
    section_info ->subtype = buffer[1];
    section_info ->flag = buffer[2];
    section_info ->vma = (uint32_t)buffer[3] | ((uint32_t)buffer[4]<<8) | ((uint32_t)buffer[5]<<16) |  ((uint32_t)buffer[6]<<24);
    section_info ->lma = (uint32_t)buffer[7] | ((uint32_t)buffer[8]<<8) | ((uint32_t)buffer[9]<<16) |  ((uint32_t)buffer[10]<<24);
    section_info ->len = (uint32_t)buffer[11] | ((uint32_t)buffer[12]<<8) | ((uint32_t)buffer[13]<<16) |  ((uint32_t)buffer[14]<<24);
    section_info ->check = (uint32_t)buffer[15] | ((uint32_t)buffer[16]<<8) | ((uint32_t)buffer[17]<<16) |  ((uint32_t)buffer[18]<<24);
}

void load_sectioninfo2buffer(load_section_info_st* section_info, uint8_t* buffer){
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
    hdr->version = buffer[5];
    hdr->flag = buffer[6];
    hdr->sectinos = buffer[7];
    hdr->len = (uint32_t)buffer[8] | ((uint32_t)buffer[9]<<8) | ((uint32_t)buffer[10]<<16) |  ((uint32_t)buffer[11]<<24);
    hdr->check = (uint32_t)buffer[12] | ((uint32_t)buffer[13]<<8) | ((uint32_t)buffer[14]<<16) |  ((uint32_t)buffer[15]<<24);
}

void load_hdr2buffer(load_head_st* hdr, uint8_t* buffer){
    buffer[0] = hdr->magic[0];
    buffer[1] = hdr->magic[1];
    buffer[2] = hdr->magic[2];
    buffer[3] = hdr->magic[3];
    buffer[4] = hdr->magic[4];
    buffer[5] = hdr->version;
    buffer[6] = hdr->flag;
    buffer[7] = hdr->sectinos; 
    buffer[8] = hdr->len & 0xFF;
    buffer[9] = (hdr->len >> 8) & 0xFF;
    buffer[10] = (hdr->len >> 16) & 0xFF;
    buffer[11] = (hdr->len >> 24) & 0xFF;
    buffer[12] = hdr->check & 0xFF;
    buffer[13] = (hdr->check >> 8) & 0xFF;
    buffer[14] = (hdr->check >> 16) & 0xFF;
    buffer[15] = (hdr->check >> 24) & 0xFF;
}
