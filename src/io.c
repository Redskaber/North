// memory
#include <sys/mman.h>      
// file
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
// SIMD
#include <immintrin.h>
#include <x86intrin.h>
// base
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#include "io.h"



/*
 * 内存页分配函数
 * @param size: 内存页大小
 * @return: 内存页指针
*/
static void* buffer_alloc(size_t size) {
    // 对齐分配: 4KB对齐
    void *ptr = aligned_alloc(ALIGNMENT, size);
    if (!ptr) {
        fprintf(stderr, "[ERROR] buffer_alloc: Failed to allocate %zu bytes\n", size);
        return NULL;
    }
#if PLATFORM_LINUX
    // 内核内存页优化: 通过madvise进行内存页优化
    if (madvise(ptr, size, MADV_SEQUENTIAL | MADV_WILLNEED) == -1) {
        perror("[WARNING] buffer_alloc: madvise failed");
    }
#endif
    return ptr;
}

/*
 * 输入缓冲区初始化函数
 * @param input: 输入缓冲区指针
 * @param filename: 文件名
 */
void input_init(InputBuffer *input, const char *filename) {
    struct stat st;
    memset(input, 0, sizeof(InputBuffer));
    // file open mode
    int open_mode = O_RDONLY;
#if PLATFORM_LINUX
    open_mode |= O_DIRECT;
#endif

    // file open
    if((input->fd = open(filename, open_mode)) == -1) {
        perror("[ERROR] input_init: open failed");
        exit(EXIT_FAILURE);
    }

    // get file state
    if(fstat(input->fd, &st) == -1) {
        perror("[ERROR] input_init: fstat failed");
        close(input->fd);
        exit(EXIT_FAILURE);
    }
    input->file_size = st.st_size;

    // memory map file
    input->mapped_addr = mmap(NULL, input->file_size, PROT_READ,
                    MAP_PRIVATE | (PLATFORM_LINUX ? MAP_POPULATE : 0), input->fd, 0);
    if(input->mapped_addr == MAP_FAILED) {
        perror("[ERROR] input_init: mmap failed");
        close(input->fd);
        exit(EXIT_FAILURE);
    }

    // memory visit optimization
    if(madvise(input->mapped_addr, input->file_size, MADV_SEQUENTIAL) == -1) {
        perror("[WARNING] input_init: madvise failed");
    }

    // alloc buffer
    if(!(input->buf[0] = (char*)buffer_alloc(BUFFER_SIZE))||
        !(input->buf[1] = (char*)buffer_alloc(BUFFER_SIZE))) {
        fprintf(stderr, "[FATAL] input_init: Buffer allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // init input buffer
    size_t first_load = input->file_size > BUFFER_SIZE ? BUFFER_SIZE : input->file_size;
    memcpy(input->buf[0], input->mapped_addr, first_load);
    input->front_idx = first_load;
    input->file_offset = first_load;
    input->active_buf = 0;
}

/*
 * AVX2版本
 * @param buf: 输入缓冲区指针
 * @return: 处理结果结构体
 */
#if defined(__AVX2__)
__attribute__((target("avx2")))
ProcessResult process_buffer_avx2(const char *buf) {
    ProcessResult res = {0};
    res.positions = malloc(MAX_POSITIONS * sizeof(uint32_t));
    if (!res.positions) {
        fprintf(stderr, "[ERROR] process_buffer_avx2: Memory allocation failed\n");
        return res;
    }

    const __m256i whitespace = _mm256_set1_epi8(' ');
    for (size_t i = 0; i < BUFFER_SIZE && res.pos_count < MAX_POSITIONS; i += 32) {
        // 加载32字节数据块
        __m256i chunk = _mm256_loadu_si256((const __m256i*)(buf + i));
        // 比较空格字符
        __m256i cmp = _mm256_cmpeq_epi8(chunk, whitespace);
        // 生成掩码（分为两个128位部分）
        unsigned mask_lo = _mm256_movemask_epi8(cmp);
        unsigned mask_hi = _mm256_movemask_epi8(_mm256_permute4x64_epi64(cmp, 0x4E));
        unsigned mask = mask_lo | (mask_hi << 16);
        
        res.space_count += _mm_popcnt_u32(mask);
        
        // 处理掩码中的每一位
        while (mask && res.pos_count < MAX_POSITIONS) {
            uint32_t pos = __builtin_ctz(mask);
            res.positions[res.pos_count++] = i + pos;
            mask ^= (1U << pos);
        }
    }
    return res;
}
#elif defined(__SSE4_2__)
__attribute__((target("sse4.2")))
ProcessResult process_buffer_sse(const char *buf) {
    ProcessResult res = {0};
    res.positions = malloc(MAX_POSITIONS * sizeof(uint32_t));
    if (!res.positions) {
        fprintf(stderr, "[ERROR] process_buffer_sse: Memory allocation failed\n");
        return res;
    }

    const __m128i whitespace = _mm_set1_epi8(' ');
    for (size_t i = 0; i < BUFFER_SIZE && res.pos_count < MAX_POSITIONS; i += 16) {
        __m128i chunk = _mm_loadu_si128((const __m128i*)(buf + i));
        __m128i cmp = _mm_cmpeq_epi8(chunk, whitespace);
        unsigned mask = _mm_movemask_epi8(cmp);
        
        res.space_count += _mm_popcnt_u32(mask);
        
        while (mask && res.pos_count < MAX_POSITIONS) {
            uint32_t pos = __builtin_ctz(mask);
            res.positions[res.pos_count++] = i + pos;
            mask ^= (1U << pos);
        }
    }
    return res;
}
#endif

/*
 * 纯C版本
 * @param buf: 输入缓冲区指针
 * @return: 处理结果结构体
*/
ProcessResult process_buffer_scalar(const char *buf) {
    ProcessResult res = {0};
    res.positions = (uint32_t*)malloc(MAX_POSITIONS * sizeof(uint32_t));
    if (!res.positions) {
        fprintf(stderr, "[ERROR] process_buffer_scalar: Memory allocation failed\n");
        return res;
    }

    for (size_t i = 0; i < BUFFER_SIZE && res.pos_count < MAX_POSITIONS; ++i) {
        if (buf[i] == ' ') {
            res.space_count++;
            res.positions[res.pos_count++] = i;
        }
    }
    return res;
}

/*
 * 处理缓冲区函数
 * @param buf: 输入缓冲区指针
 * @return: 处理结果结构体
*/
ProcessResult process_buffer(const char *buf) {
#if defined(__AVX2__)
    return process_buffer_avx2(buf);
#elif defined(__SSE4_2__)
    return process_buffer_sse(buf);
#else
    return process_buffer_scalar(buf);
#endif
}

/*
 * 下一个字符函数
 * @param sys: 输入缓冲区指针
 * @return: 下一个字符
*/
int next_char(InputBuffer *input) {
    if (input->back_idx >= input->front_idx) {
        size_t remaining = input->file_size - input->file_offset;
        if (remaining == 0) return EOF;

        size_t load_size = remaining > BUFFER_SIZE ? BUFFER_SIZE : remaining;
        int next_buf = input->active_buf ^ 1;
        
        memcpy(input->buf[next_buf], 
            (char*)input->mapped_addr + input->file_offset, 
            load_size);
        
        input->file_offset += load_size;
        input->front_idx = load_size;
        input->active_buf = next_buf;
        input->back_idx = 0;
    }
    return (unsigned char)input->buf[input->active_buf][input->back_idx++];
}

/*
 * 输入缓冲区清理函数
 * @param input: 输入缓冲区指针
 * @return: void
*/
void input_cleanup(InputBuffer *input) {
    if (input->mapped_addr) {
        munmap(input->mapped_addr, input->file_size);
        input->mapped_addr = NULL;
    }
    if (input->buf[0]) {
        free(input->buf[0]);
        input->buf[0] = NULL;
    }
    if (input->buf[1]) {
        free(input->buf[1]);
        input->buf[1] = NULL;
    }
    if (input->fd != -1) {
        close(input->fd);
        input->fd = -1;
    }
}

