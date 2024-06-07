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

// clang-format off
#include "vm/vm.h"
#include "vm/uninit.h"
// clang-format on

static bool uninit_initialize(struct page *page, void *kva);
static void uninit_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
    /* ⭐️ first page fault일때 swap_in을 `vm_do_claim_page()`
       에서 호출하는대 이때의 `swap_in()`은  `uninit_initialize`이다. */
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
 * @ref vm_alloc_page_with_initializer()
 * 
 * @warning DO NOT MODIFY this function
 * 
 * @details vm_alloc_page_with_initializer()에서 호출한다는것은 load_segment()에서
 *          해당 file을 조각조각 읽은 후 해당 file을 uninit page로 초기화한다.
 *          즉, 해당 file의 type이 ANON_TYPE일지라도 uninit page로 초기화 후에
 *          ⭐️ type과 해당 type에 맞는 initializer를 설정한다.
 *          즉, uninit struct 내부는 추후에 lazy load 될 시점에 현재 페이지가 어떤
 *          page(ex. anon)으로 변환되어야 하는지에 대한 정보를 가지고 있다.
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
 * PAGE will be freed by the caller. 
 * 
 * >  uninit_page에 의해 보유된 리소스를 해제합니다. 대부분의 페이지가 다른 페이지
 * >  객체로 변환되지만, 프로세스가 종료될 때 실행 중에 참조되지 않는 uninit 페이지가
 * >  있을 수 있습니다. page는 호출자에 의해 해제됩니다. */
static void uninit_destroy(struct page *page) {
  struct uninit_page *uninit UNUSED = &page->uninit;
  struct file_segment_info *aux = uninit->aux;

  /* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
  if (uninit->aux != NULL) free(aux);
  // TODO : file_duplicate 했을시에 file_close()
}
