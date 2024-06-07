#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include "vm/vm.h"

struct page;
enum vm_type;

typedef bool vm_initializer(struct page *, void *aux);

/**
 * @brief anon, file_backed로 초기화 되기 이전의 page 구조체
 * 
 * @property init 페이지가 초기화 이후 해당 page에서 실행하는 함수
 * @property ⭐️type uninit page에서 추후에 어떤 {type} page로 변환될지
 * @property aux init func에 전달할 인자
 * 
 * @note Uninitlialized page. The type for implementing the "Lazy loading"
 * 			 >  초기화되지 않은 페이지입니다. ⭐️"Lazy loading"을 구현하기 위한 타입입니다.
*/
struct uninit_page {
  /* Initiate the contets of the page */
  vm_initializer *init;
  enum vm_type type;
  void *aux;
  /* Initiate the struct page and maps the pa to the va */
  bool (*page_initializer)(struct page *, enum vm_type, void *kva);
};

void uninit_new(struct page *page, void *va, vm_initializer *init,
                enum vm_type type, void *aux,
                bool (*initializer)(struct page *, enum vm_type, void *kva));
#endif
