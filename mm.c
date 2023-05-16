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
#define GET(p)              (*(unsigned int*)(p)) // p가 참조하는 워드를 읽어서 리턴
#define PUT(p, val)         (*(unsigned int*)(p) = (val)) // val값을 p가 가리키는 워드에 저장

// Header와 Footer에서 블록의 사이즈와 할당여부 읽어옴
#define GET_SIZE(p)         (GET(p) & ~7) // 10진수 7은 2진수로 0111, ~7은 1000, &연산 하면 마지막 할당여부 확인하는 3비트 0으로 만들어줌 -> SIZE
#define GET_ALLOC(p)        (GET(p) & 1) // 주소 p에 있는 할당 비트를 리턴

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)            ((char*)(bp) - WSIZE) // 헤더를 가리키는 포인터를 리턴
#define FTRP(bp)            ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 풋터를 가리키는 포인터를 리턴

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)       ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE))) // 다음 블록의 블록 포인터 가리킴 (payload 첫 바이트, header 아님)
#define PREV_BLKP(bp)       ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) // 이전 블록의 블록 포인터 가리킴 (payload 첫 바이트, header 아님)



/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
void* heap_listp;

 
// mm_init - initialize the malloc package.
int mm_init(void)
{
    // mem_max_addr 넘어서면 -1 return
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1)
        return -1;
    
    // 최초의 prologue header/footer와 epilogue header 임시생성(extend_heap 통해 맨 끝으로 보내줌)
    PUT(heap_listp, 0);                             // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    // Prologue header, header 4Bytes + footer 4Bytes = 8Bytes
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    // Proloque footer, header 4Bytes + footer 4Bytes = 8Bytes
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        // Epilogue header
    heap_listp += (2*WSIZE); // header 바로 다음 위치로

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    // epilogue 헤더를 맨 끝으로 보내줌
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}


// 1. 힙이 초기화 될 때, CHUNKSIZE 바이트로 확장하여 초기 가용 블록 만들기
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

    //Initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(size, 0));           // 가용블록 header
    PUT(FTRP(bp), PACK(size, 0));           // 가용블록 footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // epliloque 헤더 초기가용블록 맨 뒤로 

    // 앞 뒤로 합칠 수 있는 영역있으면 합쳐서 return
    return coalesce(bp);
}


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



 // mm_malloc - Allocate a block by incrementing the brk pointer.
 //             Always allocate a block whose size is a multiple of the alignment.
 // 
void *mm_malloc(size_t size)
{
    size_t asize;           /* Adjusted block size */
    size_t extendsize;      /* Amount to extend heap if no fit */
    char *bp;

    // size 0 별도 처리 필요 없는 요청 처리 
    if (size == 0)
        return NULL;
    
    /* Adjust block size to include overhead and alignment reqs. */
    // 요청사이즈가 2워드(8Bytes)보다 작으면 최소 사이즈 4워드(16Bytes)로 블록 크기 지정 
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

    // find_fit을 못 했을 때, 메모리 추가 할당 (2^12 과 요청받은 메모리중 큰걸로)
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
    coalesce(bp);
}


// 
static void *find_fit(size_t asize)
{
    // First-fit search
    void *bp;

    // 다음 블록으로 넘어가면서, 할당되어있지않고 가용가능한 크기가 asize보다 크면 bp 반환
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL; // fit한게 없는 경우
}


// 
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


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr));
    //copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














