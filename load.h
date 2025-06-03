#ifndef LOAD_H
#define LOAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define LOAD_MEM_ADDR 0
#define LOAD_HDR_LEN  16
#define LOAD_HDR_INFO_MAX_LEN  512

/**
 * \enum load_section_type_e
 * SECTION类型
*/
typedef enum
{
    LOAD_SECTION_TYPE_DTB,    /**< 设备树段     */
    LOAD_SECTION_TYPE_KERNEL, /**< 内核镜像段   */
    LOAD_SECTION_TYPE_ROOTFS, /**< 根文件系统段 */
    LOAD_SECTION_TYPE_USER,   /**< 保留段,可以包括多个段,用户段内自行去区分   */
    LOAD_SECTION_TYPE_RVD,    /**< 保留段       */
} load_section_type_e;

/**
 * \enum load_section_flag_e
 * SECTION标志类型
*/
typedef enum
{
    LOAD_SECTION_FLAG_NONE = 0,  /**< 无标志 */
    LOAD_SECTION_FLAG_CRC16 = 1, /**< CRC16校验 */
    LOAD_SECTION_FLAG_CRC32 = 2, /**< CRC32校验 */

    LOAD_SECTION_FLAG_NEEDLOAD = 16, /**< 表示是否需要加载 */
} load_section_flag_e;

/**
 * \struct load_section_info_st
 * SECTION信息
*/
typedef struct
{
    load_section_type_e type; /**< \ref load_section_type_e */
    uint8_t subtype;          /**< 用户定义的段时进一步区分   */
    uint8_t flag;             /**< 标志信息用于表示是否有校验，加密等标志 一个类型对应一位 \ref load_section_flag_e */
    uint32_t vma;             /**< 加载的目的地址,运行地址,即在SDRAM中的地址    */
    uint32_t lma;             /**< 加载的源地址,加载地址,即在存储中的地址       */
    uint32_t len;             /**< 段的有效数据长度,字节数        */
    uint32_t check;           /**< 段的有效数据对应的校验信       */
} load_section_info_e;

/**
 * \struct load_head_st
 * HEAD信息  这里不采用packed方式, HEAD结构体和Buffer最好使用手动转换方式,不要使用直接类型强制转换
*/
typedef struct
{
    uint8_t  magic[5];        /**< "MLOAD"   */    
    uint8_t  version;         /**< 版本信息   */
    uint8_t  flag;            /**< 标志用于表示HEAD是否有校验信息 */
    uint8_t  sectinos;        /**< 后面的section_info个数        */
    uint32_t len;             /**< 用于表示HEAD的长度,包括整个区域 */
    uint32_t check;           /**< 用于表示HEAD的校验信息,如果flag的bit0为1 */
    /* 后面就是sectinos个load_section_info_e */
    load_section_info_e section_info[0];
}load_head_st;

typedef uint32_t (*load_mem_write_pf)(uint8_t* buffer, uint32_t addr, uint32_t len);
typedef uint32_t (*load_mem_read_pf)(uint8_t* buffer, uint32_t addr, uint32_t len);

/**
 * \fn load_mem_itf_set
 * 设置LOAD的存储读写接口
 * \param[in] write_itf 写接口
 * \param[in] read_itf 读接口
 */
void load_mem_itf_set(load_mem_write_pf write_itf, load_mem_write_pf read_itf);

void load_buffer2sectioninfo(uint8_t* buffer, load_section_info_e* section_info);
void load_sectioninfo2buffer(load_section_info_e* section_info, uint8_t* buffer);
void load_buffer2hdr(uint8_t* buffer, load_head_st* hdr);
void load_hdr2buffer(load_head_st* hdr, uint8_t* buffer);
int load_one_section(load_section_info_e* section_info);
int load_load_sections(void);
void load_boot(void);

#ifdef __cplusplus
}
#endif

#endif