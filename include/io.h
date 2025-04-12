/**
 * @file io.h
 * @author redskaber (redskaber@foxmail.com)
 * @brief 
 * @version 0.1
 * @date 2025-04-09
 * 
 * @copyright Copyright (c) 2025
 * 
 * @details this file is used language io mod.
 *  used double cache pipe input system impl in the future.
 */
#pragma once

#ifndef __NORTH_IO_H__
#define __NORTH_IO_H__
#include "common.h"

#define __USE_MISC 1   

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif


#if defined(__linux__) && defined(__GLIBC__)
#define PLATFORM_LINUX 1
#define __USE_GNU  1    
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif  
#ifndef O_DIRECT
# define O_DIRECT	 040000
#endif
#else
#define PLATFORM_LINUX 0
#pragma message("Non-Linux platform, O_DIRECT disabled")
#endif

#define BUFFER_SIZE     (2 << 20)   // 2MB
#define ALIGNMENT       4096
#define MAX_POSITIONS   BUFFER_SIZE  // 最大位置记录数


// 处理结果结构体
typedef struct {
    size_t space_count;     // 空格总数
    uint32_t* positions;    // 空格位置数组
    size_t pos_count;       // 有效位置数量
} ProcessResult;



typedef struct InputBuffer {
    int fd;                     // file descriptor
    size_t file_size;           // file size
    size_t file_offset;         // file offset
    volatile size_t front_idx;  // front index
    volatile size_t back_idx;   // back index
    void* mapped_addr;          // mapped address
    char* buf[2];               // buffer
    int active_buf;             // active buffer
} InputBuffer;

void input_init(InputBuffer *input, const char *filename);
ProcessResult process_buffer(const char *buf);
int next_char(InputBuffer *input);
void input_cleanup(InputBuffer *input);

#endif  // __NORTH_IO_H__
