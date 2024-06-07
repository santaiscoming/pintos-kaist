#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "hash.h"
#include "threads/palloc.h"

enum vm_type {
  /* page not initialized 
		 -> (aka. unused(free) page)*/
  VM_UNINIT = 0,
  /* page not related to the file, 
		 -> (aka. anonymous page) */
  VM_ANON = 1,
  /* page that realated to the file
		 -> (aka. file_backed page) 즉, 파일에 연결된 page를 의미한다.*/
  VM_FILE = 2,
  /* page that hold the page cache, for project 4 */
  VM_PAGE_CACHE = 3,

  /* Bit flags to store state */

  /* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
  VM_MARKER_0 = (1 << 3),
  VM_MARKER_1 = (1 << 4),

  /* DO NOT EXCEED THIS VALUE. */
  VM_MARKER_END = (1 << 31),
};

#include "vm/anon.h"
#include "vm/file.h"
#include "vm/uninit.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* ----------------- added for PROJECT.3-3 ----------------- */

/* 1MB */
#define USER_STACK_LIMIT_SIZE (1 << 20)

/* --------------------------------------------------------- */

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page {
  const struct page_operations *operations;
  void *va;            /* Address in terms of user space */
  struct frame *frame; /* Back reference for frame */

  /* Your implementation */
  /* ----------------- added for PROJECT.3-1 ----------------- */

  struct hash_elem spt_elem;
  bool writable;
  enum vm_type type;

  /* --------------------------------------------------------- */

  /** Per-type data are binded into the union.
	 * Each function automatically detects the current union
   * 
   * >  각 유형의 데이터가 union에 바인딩됩니다.
   * >  각 함수는 자동으로 현재 union을 감지합니다.
  */
  union { /* 3개의 구조체중 하나만 선택한다. */
    struct uninit_page uninit;
    struct anon_page anon;
    struct file_page file;
#ifdef EFILESYS
    struct page_cache page_cache;
#endif
  };
};

/* The representation of "frame" */
struct frame {
  void *kva; /* Address in terms of kernel space */
  struct page *page;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. 
 * 
 * >  페이지 작업들을 위한 함수 테이블입니다.
 * >  이것은 C에서 "interface"를 구현하는 한 가지 방법입니다.
 * >  "method"의 테이블을 구조체의 멤버에 넣고,
 * >  필요할 때마다 호출하십시오. */
struct page_operations {
  bool (*swap_in)(struct page *, void *);
  bool (*swap_out)(struct page *);
  void (*destroy)(struct page *);
  enum vm_type type;
};

/* struct page_operations에 등록하고 사용할 함수 */
#define swap_in(page, v) (page)->operations->swap_in((page), v)
#define swap_out(page) (page)->operations->swap_out(page)
#define destroy(page) \
  if ((page)->operations->destroy) (page)->operations->destroy(page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this.
 * 
 * >  현재 프로세스의 메모리 공간을 나타내는 구조체입니다.
 * >  이 구조체에 대해 특정 설계를 따르도록 강요하고 싶지 않습니다.
 * >  이 구조체에 대한 모든 설계는 당신에게 달려 있습니다. */
struct supplemental_page_table {
  /* ----------------- added for PROJECT.3-1 ----------------- */

  struct hash page_table;

  /* --------------------------------------------------------- */
};

#include "threads/thread.h"
void supplemental_page_table_init(struct supplemental_page_table *spt);
bool supplemental_page_table_copy(struct supplemental_page_table *dst,
                                  struct supplemental_page_table *src);
void supplemental_page_table_kill(struct supplemental_page_table *spt);
struct page *spt_find_page(struct supplemental_page_table *spt, void *va);
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page);
void spt_remove_page(struct supplemental_page_table *spt, struct page *page);

void vm_init(void);
bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user,
                         bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
  vm_alloc_page_with_initializer((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage,
                                    bool writable, vm_initializer *init,
                                    void *aux);
void vm_dealloc_page(struct page *page);
bool vm_claim_page(void *va);
enum vm_type page_get_type(struct page *page);

/* ----------------- added for PROJECT.3-1 ----------------- */

uint64_t hash_page_hash(const struct hash_elem *e, void *aux);
bool cmp_page_hash(const struct hash_elem *x, const struct hash_elem *y,
                   void *aux);

/* --------------------------------------------------------- */

#endif /* VM_VM_H */
