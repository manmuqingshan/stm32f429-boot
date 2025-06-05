#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "load.h"
#include "cJSON.h"


int main(int argc, char* argv[]){
    if(argc != 3){
        printf("usage:imgpack jsonfile outfile\r\n");
        return -1;
    }

    FILE* jsonfile;
    FILE* outfile;
    FILE* infile;
    char* jsonbuffer;
    long jsonfilesize;
    cJSON* cjson = NULL;
    cJSON* cjson_head = NULL;
    cJSON* cjson_section = NULL;
    cJSON* cjson_section_n = NULL;
    cJSON* cjson_tmp = NULL;
    load_head_st head;
    load_section_info_st section_info;
    long outbuffersize = 0;
    char* outbuffer;
    int sections;
    long section_lma;
    long section_len;
    memset(&head,0,sizeof(head));
    head.magic[0]='M';
    head.magic[1]='L';
    head.magic[2]='O';
    head.magic[3]='A';
    head.magic[4]='D';

    jsonfile = fopen(argv[1], "rb");
    if(jsonfile == NULL){
        printf("open %s err %s\r\n", argv[1],strerror(errno));
        return -2;
    }else{
        printf("open %s ok\r\n", argv[1]);
    }

    outfile = fopen(argv[2], "wb+");
    if(outfile == NULL){
        printf("open %s err \r\n", argv[2]);
        fclose(jsonfile);
        return -3;
    }else{
        printf("open %s ok\r\n", argv[2]);
    }

    fseek(jsonfile, 0, SEEK_END);
    jsonfilesize = ftell(jsonfile);
    printf("jsonfilesize=%ld\r\n",jsonfilesize);
    jsonbuffer = malloc(jsonfilesize);
    fseek(jsonfile, 0, SEEK_SET);
    fread(jsonbuffer,1,jsonfilesize,jsonfile);
    fclose(jsonfile);
    printf("read jsonfile to jsonbuffer\r\n");
    printf("%s\r\n",jsonbuffer);
    cjson = cJSON_Parse(jsonbuffer);
    if(cjson == NULL){
        printf("cJSON_Parse err\r\n");
        goto fail0;
    }
    cjson_head = cJSON_GetObjectItem(cjson, "head");
    if(cjson_head == NULL){
        printf("cJSON_GetObjectItem head err\r\n");
        goto fail0;
    }
    cjson_tmp = cJSON_GetObjectItem(cjson_head, "version");
    if(cjson_tmp == NULL){
        printf("cJSON_GetObjectItem version err\r\n");
        goto fail0;
    }
    head.version = cjson_tmp->valueint;
    cjson_tmp = cJSON_GetObjectItem(cjson_head, "flag");
    if(cjson_tmp == NULL){
        printf("cJSON_GetObjectItem flag err\r\n");
        goto fail0;
    }
    head.flag = cjson_tmp->valueint;
    cjson_section = cJSON_GetObjectItem(cjson_head, "section");
    if(cjson_section == NULL){
        printf("cJSON_GetObjectItem section err\r\n");
        goto fail0;
    }
    sections = cJSON_GetArraySize(cjson_section);
    printf("sections = %d\r\n",sections);
    head.sectinos = sections;
    head.len = LOAD_HDR_LEN + sections*LOAD_SECTION_INFO_LEN;

    for(int i=0; i<sections; i++){
        /* 找到lma+len最大的位置决定outbuffer的大小 */
        cjson_section_n = cJSON_GetArrayItem(cjson_section, i);
        if(cjson_section_n == NULL){
            printf("cJSON_GetArrayItem cjson_section_%d err\r\n",i);
            goto fail0;
        }
        cjson_tmp = cJSON_GetObjectItem(cjson_section_n, "lma");
        if(cjson_tmp == NULL){
            printf("cJSON_GetObjectItem cjson_section_%d lma err\r\n",i);
            goto fail0;
        }
        long tmp;
        sscanf(cjson_tmp->valuestring, "%lx", &tmp);
        section_lma = tmp;
        cjson_tmp = cJSON_GetObjectItem(cjson_section_n, "len");
        if(cjson_tmp == NULL){
            printf("cJSON_GetObjectItem cjson_section_%d len err\r\n",i);
            goto fail0;
        }
        section_len = cjson_tmp->valueint;

        if((section_lma+section_len) > outbuffersize){
            outbuffersize = section_lma+section_len;
        }
    }
    printf("malloc outbuffer %ld\r\n",outbuffersize);
    outbuffer = malloc(outbuffersize);
    memset(outbuffer,0xFF,outbuffersize);

    for(int i=0; i<sections; i++){
        /* 找到lma+len最大的位置决定outbuffer的大小 */
        cjson_section_n = cJSON_GetArrayItem(cjson_section, i);
        if(cjson_section_n == NULL){
            printf("cJSON_GetArrayItem cjson_section_%d err\r\n",i);
            goto fail1;
        }
        cjson_tmp = cJSON_GetObjectItem(cjson_section_n, "type");
        if(cjson_tmp == NULL){
            printf("cJSON_GetObjectItem cjson_section_%d type err\r\n",i);
            goto fail1;
        }
        section_info.type = cjson_tmp->valueint;
        cjson_tmp = cJSON_GetObjectItem(cjson_section_n, "subtype");
        if(cjson_tmp == NULL){
            printf("cJSON_GetObjectItem cjson_section_%d subtype err\r\n",i);
            goto fail1;
        }
        section_info.subtype = cjson_tmp->valueint;
        cjson_tmp = cJSON_GetObjectItem(cjson_section_n, "flag");
        if(cjson_tmp == NULL){
            printf("cJSON_GetObjectItem cjson_section_%d flag err\r\n",i);
            goto fail1;
        }
        section_info.flag = cjson_tmp->valueint;
        cjson_tmp = cJSON_GetObjectItem(cjson_section_n, "vma");
        if(cjson_tmp == NULL){
            printf("cJSON_GetObjectItem cjson_section_%d vma err\r\n",i);
            goto fail1;
        }
        long tmp;
        sscanf(cjson_tmp->valuestring, "%lx", &tmp);
        section_info.vma = tmp;
        cjson_tmp = cJSON_GetObjectItem(cjson_section_n, "lma");
        if(cjson_tmp == NULL){
            printf("cJSON_GetObjectItem cjson_section_%d lma err\r\n",i);
            goto fail1;
        }
        sscanf(cjson_tmp->valuestring, "%lx", &tmp);
        section_info.lma = tmp;
        cjson_tmp = cJSON_GetObjectItem(cjson_section_n, "len");
        if(cjson_tmp == NULL){
            printf("cJSON_GetObjectItem cjson_section_%d len err\r\n",i);
            goto fail1;
        }
        section_info.len = cjson_tmp->valueint;
        
        cjson_tmp = cJSON_GetObjectItem(cjson_section_n, "file");
        section_info.check = 0;
        if(cjson_tmp->valuestring != NULL){
            infile = fopen(cjson_tmp->valuestring,"rb");
            if(infile == NULL){
                printf("open %s err %s\r\n",cjson_tmp->valuestring, strerror(errno));
                goto fail1;
            }
            fread(outbuffer+section_info.lma,1,section_info.len,infile);
            printf("read %s len %d to outbuffer of %d\r\n",cjson_tmp->valuestring, section_info.len,section_info.lma);
            fclose(infile);

            if(section_info.flag & LOAD_SECTION_FLAG_CRC32){
                /* @todo */
                //section_info.check = crc32(outbuffer+section_info.lma,outbuffer+section_info.lma);
            }
        }
        printf("type=%d,subtype=%d,flag=%d,vma=%#x,lma=%#x,len=%d\r\n",
            section_info.type,section_info.subtype,section_info.flag,section_info.vma,
            section_info.lma,section_info.len);
        load_sectioninfo2buffer(&section_info, outbuffer+LOAD_HDR_LEN+i*LOAD_SECTION_INFO_LEN);
    }

    if(head.flag & 0x01){
        /* @todo */
        //head.check = xxx;
    }else{
        head.check = 0;
    }
    load_hdr2buffer(&head, outbuffer);

    printf("write to outfile\r\n");
    fwrite(outbuffer,1,outbuffersize,outfile);

fail1:
    free(outbuffer);

fail0:
    free(jsonbuffer);
    fclose(outfile);

    return 0;
}
