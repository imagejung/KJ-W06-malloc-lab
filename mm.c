#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "imagejung",
    /* First member's full name */
    "youngsang jung",
    /* First member's email address */
    "ysjung9312@gmail.com",
    "",
    ""
};


// 기본 상수 정의
#define WSIZE       4       // Double word size 상수 (4Bytes)
#define DSIZE       8       // Double word size 상수 (8Bytes)
#define CHUNKSIZE   (1<<12) // 초기 가용 블록과 힙 확장을 위한 기본 크기, 1000000000000 (2^12)

// 큰 값 비교 매크로
#define MAX(x,y)            ((x) > (y) ? (x) : (y))

// 메모리 블록은 더블워드이므로 size 값은 8(Bytes)의 배수. size의 끝 3비트는 000
// 끝 3비트 000에 할당여부 000 or 001 로 표현 (1이 할당됨을 표현)
// (size) | (alloc) -> "or" 비트연산 수행, 해당값을 Header와 Footer에 넣음
#define PACK(size, alloc)   ((size) | (alloc))

// 값 읽기, 저장하기
#define GET(p)              (*(unsigned int*)(p))         // p가 참조하는 워드를 읽어서 리턴
#define PUT(p, val)         (*(unsigned int*)(p) = (val)) // val값을 p가 가리키는 워드에 저장

// Header와 Footer에서 블록의 사이즈와 할당여부 읽어옴
#define GET_SIZE(p)         (GET(p) & ~0x7) // 자연수7은 2진수로 0111, ~7은 1000, &연산 하면 마지막 할당여부 확인하는 3비트 0으로 만들어줌 -> SIZE
#define GET_ALLOC(p)        (GET(p) & 0x1)  // 주소 p에 있는 맨 끝 할당 비트(0 or 1)를 리턴

// bp가 Payload 맨 앞자리를 가리키고 있는 상태에서, Header와 Footer 워드 출력하는 매크로
// (char*) 형 포인터 변수로 bp 사용. +1 -1 등을 1Byte를 의미하도록 하기 위해서 
#define HDRP(bp)            ((char*)(bp) - WSIZE) // 헤더를 가리키는 포인터를 리턴
#define FTRP(bp)            ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 풋터를 가리키는 포인터를 리턴

// bp가 payload 맨 앞자리를 가리키고 있는 상태에서, 다음/이전 블록의 Payload 맨 앞자리 가리킴
#define NEXT_BLKP(bp)       ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE))) // 다음 블록의 블록 포인터 가리킴 (payload 첫 바이트, header 아님)
#define PREV_BLKP(bp)       ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) // 이전 블록의 블록 포인터 가리킴 (payload 첫 바이트, header 아님)

// .h에 없는 함수들 먼저 선언 
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
void* heap_listp;

 
// mm_init함수. Prologue Header/Footer와 Epilogue Header 생성
int mm_init(void)
{
    // mem_max_addr 넘어서면 -1 return
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1)
        return -1;
    
    // 최초의 prologue header/footer와 epilogue header 생성(epilogue header는 extend_heap 통해 맨 끝으로 보내줌)
    PUT(heap_listp, 0);                             // Alignment padding (총 4워드 사이즈 만들기 위해)
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    // Prologue header, header 4Bytes + footer 4Bytes = 8Bytes (Prologue 가 8Bytes)
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    // Proloque footer, header 4Bytes + footer 4Bytes = 8Bytes
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        // Epilogue Header
    heap_listp += (2*WSIZE); // Prologue Header 바로 다음 위치로 (Payload 맨앞 가리킴)

    // Epilogue Header를 CHUNKSIZE 맨 끝으로 보내줌
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) // mem_max_addr 넘어서면 -1 return (err)
        return -1;
    return 0;
}

// extend_heap 함수
// 1. 힙이 초기화 될 때, 가용블록 CHUNKSIZE 크기로 확장하여(Epilogue Header 맨 끝으로 보내주기) 초기 가용 블록 만들기
// 2. find_fit으로 크기에 맞는 가용가능한 블록 못 찾았을 때 추가적인 힙 공간 요청
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    
    // 홀수면 word size +1 
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    
    // mem_max_addr 넘어서면 NULL return
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // Initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(size, 0));           // 가용블록 header
    PUT(FTRP(bp), PACK(size, 0));           // 가용블록 footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // epliloque 헤더 초기가용블록 맨 뒤로 

    // 앞 뒤로 합칠 수 있는 영역있으면 합쳐서 return
    return coalesce(bp);
}

// 다음/이전 블록의 할당여부 확인하여 합쳐주기
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case1, 앞뒤가 다 할당 되어 있음
    if (prev_alloc && next_alloc){
        return bp;
    }

    // case2, 앞에는 할당되어 있음, 뒤에는 가용가능 영역
    else if (prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // case3, 앞에는 가용가능, 뒤에는 할당되어 있음
    else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    // case4, 앞 / 뒤 모두 가용가능 영역
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}



// 메모리 할당 함수
void *mm_malloc(size_t size)
{
    size_t asize;           // Adjusted Block 사이즈
    size_t extendsize;      // 메모리 할당할 공간 없을 경우 확장한 Heap 사이즈
    char *bp;

    // size 0, 별도 처리 필요 없는 요청 처리 
    if (size == 0)
        return NULL;
    
    // 요청사이즈가(메모리가 들어갈 사이즈) 2워드(8Bytes)보다 작으면 최소 사이즈 4워드(16Bytes)로 블록 크기 지정 
    if (size <= DSIZE)
        asize = 2*DSIZE;
    // 더블워드(8의 배수로)의 크기로 블록 지정하기 위해서 
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    // asize의 블록크기로 할당할 수 있는 위치 찾기
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize); // 블록을 찾으면 요청한 블록을 배치하고 남는부분 분할
        return bp;
    } 

    // find_fit을 못 했을 때, 메모리 추가 할당 ('2^12' 과 '요청받은 메모리'중 큰걸로)
    // 2^12보다 작으면 place로 가용영역 만들어 줌
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}


// 블록사이즈 만큼 header와 footer에 할당확인 영역 0으로
void mm_free(void *bp)
{   
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp); // 다음/이전 할당안된 영역있으면 합치기
}


// First-Fit search
static void *find_fit(size_t asize)
{
    void *bp;

    // 다음 블록으로 넘어가면서, 크기가 0보다 큰 정상적인 부분에 검색
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        // 할당되어있지않고, 가용가능한 크기가 asize보다 크면 bp 반환
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL; // fit한게 없는 경우
}


// 메모리 할당하고 남는 부분 분할
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    // 필요부분 배치하고 남는부분이 16Bytes(4워드) 보다 크면 가용가능 영역으로 다시 할당
    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    // 남는 부분 16Bytes(4워드)보다 작으면 그냥 분할x
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}


// 
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}