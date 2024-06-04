/** uninit.c: Implementation of uninitialized page.
 * 
 * >  초기화되지 않은 페이지의 구현입니다.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * 
 * >  모든 페이지는 uninit 페이지로 태어납니다. 첫 번째 페이지 오류가 발생하면,
 * >  핸들러 체인이 uninit_initialize (page->operations.swap_in)를 호출합니다.
 * >  uninit_initialize function은 페이지 객체를 초기화하여 특정 페이지 객체 (anon,
 * >  file, page_cache)로 변환하고, vm_alloc_page_with_initializer 함수에서
 * >  전달된 초기화 콜백을 call합니다. */

#include "vm/uninit.h"
#include "vm/vm.h"

static bool uninit_initialize(struct page *page, void *kva);
static void uninit_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
    .swap_in = uninit_initialize,
    .swap_out = NULL,
    .destroy = uninit_destroy,
    .type = VM_UNINIT,
};

/**
 * @brief 전달받은 page를 uninit page로 초기화합니다.
 * 
 * @param page 페이지 객체
 * @param va 페이지의 가상 주소
 * @param init 초기화 프로그램
 * @param type 페이지의 타입
 * @param aux 초기화 프로그램에 전달할 인자
 * 
 * @warning DO NOT MODIFY this function
*/
void uninit_new(struct page *page, void *va, vm_initializer *init,
                enum vm_type type, void *aux,
                bool (*initializer)(struct page *, enum vm_type, void *)) {
  ASSERT(page != NULL);

  *page = (struct page){.operations = &uninit_ops,
                        .va = va,
                        .frame = NULL, /* no frame for now */
                        .uninit = (struct uninit_page){
                            .init = init,
                            .type = type,
                            .aux = aux,
                            .page_initializer = initializer,
                        }};
}

/**
 * @brief uninit page를 초기화합니다.
 * 
 * @note Initalize the page on first fault
 *       >  첫 번째 오류에서 페이지를 초기화합니다.
*/
static bool uninit_initialize(struct page *page, void *kva) {
  struct uninit_page *uninit = &page->uninit;

  /* Fetch first, page_initialize may overwrite the values */
  vm_initializer *init = uninit->init;
  void *aux = uninit->aux;

  /* TODO: You may need to fix this function. */
  return uninit->page_initializer(page, uninit->type, kva) &&
         (init ? init(page, aux) : true);
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void uninit_destroy(struct page *page) {
  struct uninit_page *uninit UNUSED = &page->uninit;
  /* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
}
