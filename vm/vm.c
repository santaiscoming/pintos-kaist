/* vm.c: Generic interface for virtual memory objects. */

#include "vm/vm.h"
#include "hash.h"
#include "include/threads/vaddr.h"
#include "include/vm/anon.h"
#include "include/vm/file.h"
#include "threads/malloc.h"
#include "vm/inspect.h"

/**
 * @brief 각 하위 시스템의 초기화 코드를 호출하여 가상 메모리 하위 시스템을 초기화합니다.
 * 
 * @note Initializes the virtual memory subsystem by invoking each 
 *       subsystem's intialize codes.
*/
void vm_init(void) {
  vm_anon_init();
  vm_file_init();
#ifdef EFILESYS /* For project 4 */
  pagecache_init();
#endif
  register_inspect_intr();
  /* DO NOT MODIFY UPPER LINES. */
  /* TODO: Your code goes here. */
}

/**
 * @brief page의 type을 반환한다.
 * 
 * @note Get the type of the page. This function is useful if you 
 *       want to know the type of the page after it will be initialized.
 *       This function is fully implemented now.
 * 
 *       >  페이지의 유형을 반환합니다. 이 함수는 페이지가 ⭐️초기화된 후의 유형을 알고 싶을 때 유용합니다.
 *       >  이 함수는 현재 완전히 구현되어 있습니다.
*/
enum vm_type page_get_type(struct page *page) {
  int ty = VM_TYPE(page->operations->type);
  switch (ty) {
    case VM_UNINIT:
      return VM_TYPE(page->uninit.type);
    default:
      return ty;
  }
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);
static struct page *page_lookup(struct hash *hash_table, const void *address);
static void supplemental_page_destroy(struct hash_elem *e, void *aux UNUSED);

/**
 * @brief ⭐️pending 상태인 page를 초기화 프로그램과 함께 생성한다.
 * 
 * @details init과 initializer는 다르다.
 *          init은 페이지에 대해서 실행하는 함수이고, initializer는 페이지를 초기화하는 함수이다.
 *          load_segment()에서 현재 함수를 사용하는데 init에 lazy_load_segment를
 *          init으로 전달해준다.
 *          그 후 uninit_new() 내부에서 init이 있을경우는 실행하고, 없을경우는 true를 반환한다.
 *          또한 그에대한 결과를 initializer()의 실행값과 비교한다.
 * 
 *          즉, init은 사용자가 제공하는 사용자 수준의 커스텀 초기화 함수이며
 *          initializer는 시스템 수준에서 페이지를 초기화하는 함수이다.  
 * 
 * @ref uninit_new(), load_segment()
 * 
 * @note Create the pending page object with initializer. 
 *       If you want to create a page, do not create it directly
 *       and make it through this function or `vm_alloc_page`. 
 * 
 *       >  초기화 프로그램과 함께 **pending** 상태인 페이지 객체를 만듭니다.
 *       >  페이지를 만들고 싶다면 직접 만들지 말고 v_a_p_w_i 함수(현재 함수)나
 *       >  `vm_alloc_page`를 통해 만드세요.
*/
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage,
                                    bool writable, vm_initializer *init,
                                    void *aux) {
  struct supplemental_page_table *spt = &thread_current()->spt;
  bool (*initializer)(struct page *, enum vm_type, void *);
  bool succ = false;

  ASSERT(VM_TYPE(type) != VM_UNINIT)

  /* Check wheter the upage is already occupied or not. */
  if (spt_find_page(spt, upage) == NULL) {
    /* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. 
     * 
     * >  페이지를 만들고 VM 유형에 따라 초기화 프로그램을 가져오고
     * >  uninit_new를 호출하여 "uninit"페이지 구조체를 만듭니다.
     * >  uninit_new를 호출한 후 필드를 수정해야합니다. */

    struct page *page = (struct page *)calloc(1, sizeof(struct page));
    if (!page) goto err;

    switch (VM_TYPE(type)) {
      case VM_ANON:
        initializer = anon_initializer;
        break;

      case VM_FILE:
        initializer = file_backed_initializer;
        break;

      default:
        free(page);
        goto err;
    }

    uninit_new(page, upage, init, type, aux, initializer);
    page->writable = writable;
    page->type = type;

    /* TODO: Insert the page into the spt. 
     * >  spt에 페이지를 삽입하십시오. */
    if (!spt_insert_page(spt, page)) {
      free(page);
      goto err;
    }

    succ = true;
  }

err:
  return succ;
}
/**
 * @brief spt로부터 va에 해당하는 page를 찾아 반환한다.
 *  
 * @param spt supplemental page table
 * @param va virtual address
 * 
 * @return page ? page : NULL
 * 
 * @details pg_round_down() : 주어진 주소를 페이지의 시작 주소로 내림한다.
 *          va 블럭이 0x1000 ~ 0x2000 이라고 가정하자.
 *          해당 범위 내의 모든 주소는 항상 같은 page를 가르켜야하기에
 *          pg_round_down을 사용하여 0x1000으로 내림하여 page를 찾는다.
 * 
 * @note Find VA from spt and return page. On error, return NULL.
 * 
 * TODO: Fill this function. 
*/
struct page *spt_find_page(struct supplemental_page_table *spt, void *va) {
  struct page *page;
  void *va_pg_start_ptr = (void *)pg_round_down(va);

  page = page_lookup(&spt->page_table, va_pg_start_ptr);

  return (page != NULL) ? page : NULL;
}

/**
 * @brief page를 page_table에 삽입한다.
 * 
 * @note Insert PAGE into spt with validation.
 * 
 * TODO: Fill this function.
*/
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
                     struct page *page UNUSED) {
  int succ =
      (hash_insert(&spt->page_table, &page->spt_elem) == NULL) ? true : false;

  return succ;
}

/**
 * @brief page를 spt로부터 제거한다.
 * 
 * @param spt supplemental page table
 * @param page page
*/
void spt_remove_page(struct supplemental_page_table *spt, struct page *page) {
  /*hash delete()는 삭제할 elem을 삭제하지(찾지) 못했을때 NULL을 반환하나 함수의 반환값이 void다. */
  hash_delete(&spt->page_table, &page->spt_elem);

  return;
}

/* Get the struct frame, that will be evicted. */
static struct frame *vm_get_victim(void) {
  struct frame *victim = NULL;
  /* TODO: The policy for eviction is up to you. */

  return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *vm_evict_frame(void) {
  struct frame *victim UNUSED = vm_get_victim();
  /* TODO: swap out the victim and return the evicted frame. */

  return NULL;
}

/**
 * @brief physical memory에서 page만큼의 공간을 할당하고 할당한 블럭의 ptr을 
 *        들고있는 frame을 반환한다.
 * 
 * @note palloc() and get frame. If there is no available page, evict the page
 *       and return it. This always return valid address. That is, if the user pool
 *       memory is full, this function evicts the frame to get the available memory
 *       space.
 * 
 *       >  palloc() 및 프레임을 가져옵니다. 사용 가능한 페이지가 없으면 페이지를 제거하고 반환합니다.
 *       >  이 함수는 항상 유효한 주소를 반환합니다. 즉, 사용자 풀 메모리가 가득 차면 이 함수는
 *       >  사용 가능한 메모리를 얻기 위해 프레임을 제거합니다.
 * 
 * TODO: Fill this function.
*/
static struct frame *vm_get_frame(void) {
  struct page *new_page = NULL;
  struct frame *frame = NULL;

  new_page = palloc_get_page(PAL_USER); /* GITBOOK : user pool */
  if (!new_page) PANIC("TODO !");

  frame = (struct frame *)calloc(1, sizeof(struct frame));
  frame->kva = new_page;
  frame->page = NULL;

  ASSERT(frame != NULL);
  ASSERT(frame->page == NULL);

  return frame;
}

/* Growing the stack. */
/**
 * @brief page fault가 발생한 주소를 기준으로 stack을 확장한다.
 * 
 * @param addr fault address
 * 
 * @details ⭐️MY_README.md 참고
*/
static void vm_stack_growth(void *addr UNUSED) {
  bool succ = true;
  struct supplement_page_table *spt = &thread_current()->spt;
  struct page *page = NULL;
  void *page_addr = pg_round_down(addr);

  while (spt_find_page(spt, page_addr) == NULL) {
    succ = vm_alloc_page(VM_ANON, page_addr, true);
    if (!succ) PANIC("BAAAAAM !!");

    page_addr += PGSIZE;

    if (addr >= page_addr) break;
  }
}

/* Handle the fault on write_protected page */
static bool vm_handle_wp(struct page *page UNUSED) {}

/**
 * @brief page fault시에 handling을 시도한다.
 * 
 * @param f interrupt frame
 * @param addr fault address
 * @param user bool ? user로부터 접근 : kernel로부터 접근 ⭐️page fault는 kernel에서도 발생 가능하다.
 * @param write bool ? 쓰기 권한으로 접근 : 읽기 권한으로 접근
 * @param not_present bool ? not-present(non load P.M) page 접근 : Read-only page 접근
 * 
 * @ref `page_fault()` from process.c
 * 
 * @return bool
 * 
 * TODO: Validate the fault
*/
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
                         bool user UNUSED, bool write UNUSED,
                         bool not_present UNUSED) {
  struct supplemental_page_table *spt = &thread_current()->spt;
  void *page_addr = pg_round_down(addr);
  struct page *page = NULL;

  if (user && is_kernel_vaddr(addr)) return false;

  page = spt_find_page(spt, page_addr);

  if (!page) {
    if (!not_present) return false; /*  */

    // clang-format off
    if (addr < (void *)USER_STACK && 
        addr >= (void *)(f->rsp - 8) &&
        addr >= (void *)(USER_STACK - USER_STACK_LIMIT_SIZE) ||
        addr > f->rsp) {
      // clang-format on
      vm_stack_growth(addr);
      return true;
    }

    return false;
  } else {
    /* page는 R/O인데 write 작업을 하려는 경우 */
    if (page->writable == false && write == true) return false;

    return vm_claim_page(page_addr);
  }
}

/**
 * @brief page를 해제한다.
 * 
 * @warning ❌ DO NOT MODIFY THIS FUNCTION.
*/
void vm_dealloc_page(struct page *page) {
  destroy(page);
  free(page);
}

/**
 * This function serves as the higher-level interface to the process of claiming a page. 
 * Claiming a page in this context typically means allocating a physical frame for a virtual address (VA) that has previously been reserved 
 * or setup but not yet mapped to a physical memory (i.e., lazy allocation).
 * 
 * 이 기능은 페이지를 청구하는 프로세스의 상위 인터페이스 역할을 합니다.
 * 이러한 맥락에서 페이지를 청구한다는 것은 일반적으로 이전에 예약되거나 설정되었지만 
 * 아직 물리적 메모리에 매핑되지 않은 가상 주소(VA)에 대한 물리적 프레임을
 * 할당하는 것을 의미합니다(즉, 지연 할당).
*/
/**
 * @brief VA에 해당하는 page(⭐️이미 할당되어있는)를 요구한다. 단, physical memory에 
 *        매핑되어있지 않은 page이다.
 * 
 * @details VA에 해당하는 page를 요구하는 역할만 한다. (Claiming page 의 상위 인터페이스)
 *          physical memory에 매핑하는 역할인 low-level interface는 수행하지 않는다.
 *          physical memory에 매핑하는 역할은 vm_do_claim_page()에게 위임한다.
 *          vm_do_claim_page는 물리 메모리 관리와 MMU 설정과 같은 하위 세부사항을 다룬다.
 * 
 * @note Claim the 🤔page that allocate on VA.
 *       >  VA에 할당된 페이지를 요구합니다.
*/
bool vm_claim_page(void *va UNUSED) {
  struct thread *curr_t = thread_current();
  struct page *page = NULL;

  /* TODO: Fill this function */
  page = spt_find_page(&(curr_t->spt), va);
  if (!page) return false;

  return vm_do_claim_page(page);
}

/**
 * @brief 요구한 page를 mmu를 통해 physical memory(from `vm_get_frame()`)에 매핑한다.
 * 
 * @note Claim the 🤔PAGE and set up the mmu.
 *       >  페이지를 요구하고 mmu를 통해 set up 한다.
 * 
 * @details 🤔 vm_claim_page()와 vm_do_claim_page()의 note에 ⭐️page에
 *          대한 문자 표현이 대문자와 소문자로 구분되어있는데 page는 virtual page
 *          를 의미하고 PAGE는 Frame page를 의미한다.
*/
static bool vm_do_claim_page(struct page *page) {
  struct thread *curr_t = thread_current();
  struct frame *frame = vm_get_frame();
  bool writable = page->writable;

  /* page 구조체와 frame 구조체의 연결 */
  frame->page = page;
  page->frame = frame;

  /* TODO: Insert page table entry to map page's VA to frame's PA. */
  /*  */
  if (!pml4_set_page(curr_t->pml4, page->va, frame->kva, writable)) {
    return false;
  }

  /* ---------------------------------------------------------- */

  return swap_in(page, frame->kva);
}

/**
 * @brief 새로운 supplemental page table을 초기화 한다.
 * 
 * @param spt supplemental page table
 * 
 * @note Initialize new supplemental page table
*/
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED) {
  hash_init(&spt->page_table, hash_page_hash, cmp_page_hash, NULL);
}

/**
 * @brief parent thread의 spt를 child thread로 복사한다.
 * 
 * @warning 🚨 copy 시점의 current thread는 child 이다. 
 * 
 * @see __do_fork()
 * 
 * @param dst 복사할 supplemental page table
 * @param src 복사할 supplemental page table
 * 
 * @return bool
 * 
 * @note Copy the supplemental page table from src to dst.
 *
*/
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
                                  struct supplemental_page_table *src UNUSED) {
  struct hash_iterator iterator;
  struct page *parent_page, *child_page = NULL;
  struct file_segment_info *src_aux, *dst_aux = NULL;
  enum vm_type curr_page_type;
  bool succ = false;

  hash_first(&iterator, &src->page_table);

  while (hash_next(&iterator)) {
    parent_page = hash_entry(hash_cur(&iterator), struct page, spt_elem);
    curr_page_type = VM_TYPE(parent_page->operations->type);

    switch (curr_page_type) {

      case VM_UNINIT:
        src_aux = (struct file_segment_info *)parent_page->uninit.aux;
        dst_aux = (struct file_segment_info *)calloc(
            1, sizeof(struct file_segment_info));
        if (!dst_aux) goto err;

        memcpy(dst_aux, src_aux, sizeof(struct file_segment_info));
        dst_aux->file = file_duplicate(src_aux->file);
        /* memcpy 오류시 다음과 같이 변경될 수 있음
           dst_aux->read_bytes = src_aux->read_bytes;
           dst_aux->offset = src_aux->offset; 
           dst_aux->zero_bytes = src_aux->zero_bytes; */

        if (!vm_alloc_page_with_initializer(
                page_get_type(parent_page), parent_page->va,
                parent_page->writable, parent_page->uninit.init, dst_aux)) {

          free(dst_aux);
          goto err;
        }
        break;

      case VM_ANON:
        if (!vm_alloc_page(VM_ANON | VM_MARKER_0, parent_page->va,
                           parent_page->writable)) {
          goto err;
        }
        if (!vm_claim_page(parent_page->va)) goto err;

        child_page = spt_find_page(dst, parent_page->va);
        if (!child_page) goto err;

        memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);

        break;
        /* TODO : copy-on-write 구현한다면 부모의 kva를 자식의 va가 가르키도록 설정 */
    }
  }

  succ = true;

err:

  return succ;
}

static void supplemental_page_destroy(struct hash_elem *e, void *aux UNUSED) {
  struct page *page = hash_entry(e, struct page, spt_elem);

  vm_dealloc_page(page);
}

/**
 * @brief supplemental page table의 자원을 해제(deallocate)한다.
 * 
 * @param spt 해제할 supplemental page table
 * 
 * @note Free the resource hold by the supplemental page table 
 * 
 * TODO: Destroy all the supplemental_page_table hold by thread and
 * TODO: writeback all the modified contents to the storage. 
*/
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED) {
  // destroy와 clear의 차이는 단지 buchets를 free하는지의 차이인데 안되는 이유를 찾아보라.
  // hash_destroy(&spt->page_table, supplemental_page_destroy);
  hash_clear(&spt->page_table, supplemental_page_destroy);
}

/**
 * @brief page의 va를 hash값으로 변환한다.
 * 
 * @param e hash_elem
 * @param aux auxiliary data
 * 
 * @return hash value
*/
uint64_t hash_page_hash(const struct hash_elem *e, void *aux) {
  struct page *page = hash_entry(e, struct page, spt_elem);
  uint64_t result = hash_bytes(&page->va, sizeof(page->va));

  return result;
}

/**
 * @brief page의 va를 비교한다.
 * 
 * @param x 비교할 hash_elem
 * @param y 비교할 hash_elem
 * 
 * @return bool
*/
bool cmp_page_hash(const struct hash_elem *a, const struct hash_elem *b,
                   void *aux) {
  struct page *p_a = hash_entry(a, struct page, spt_elem);
  struct page *p_b = hash_entry(b, struct page, spt_elem);

  return p_a->va < p_b->va;
}

/* Returns the page containing the given virtual address, or a null pointer if no such page exists. */
static struct page *page_lookup(struct hash *hash_table, const void *address) {
  struct page p;
  struct hash_elem *e;

  p.va = address;
  e = hash_find(hash_table, &p.spt_elem);

  return (e != NULL) ? hash_entry(e, struct page, spt_elem) : NULL;
}