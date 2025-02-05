#pragma once

/* This codes assumes that POOL_TAG have already been defined if building in kernel-mode. */

#ifdef _KERNEL_MODE
void* operator new (size_t size) {
    // allocate in non-paged pool (will always reside in RAM)
    return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, POOL_TAG);
}

void* operator new[] (size_t size) {
    // allocate in non-paged pool (will always reside in RAM)
    return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, POOL_TAG);
}

void operator delete (void *ptr, size_t /*size*/) noexcept {
    return ExFreePool(ptr);
}

void operator delete[] (void* ptr) noexcept {
    return ExFreePool(ptr);
}

#endif // _KERNEL_MODE
