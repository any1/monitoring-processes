#pragma once

#define MEMORDER __ATOMIC_RELAXED

#define atomic_load(src) __atomic_load_n((src), MEMORDER)
#define atomic_store(dst, value) __atomic_store_n((dst), (value), MEMORDER)

#define atomic_cas(dst, expected, desired) \
        __atomic_compare_exchange_n((dst), (expected), (desired), 0, \
                                    MEMORDER, MEMORDER)

#define atomic_cas_weak(dst, expected, desired) \
        __atomic_compare_exchange_n((dst), (expected), (desired), 1, \
                                    MEMORDER, MEMORDER)

#define atomic_swap(dst, value) __atomic_exchange_n((dst), (value), MEMORDER)

#define atomic_add_fetch(dst, value) \
        __atomic_add_fetch((dst), (value), MEMORDER)

#define atomic_sub_fetch(dst, value) \
        __atomic_sub_fetch((dst), (value), MEMORDER)

#define atomic_and_fetch(dst, value) \
        __atomic_and_fetch((dst), (value), MEMORDER)

#define atomic_xor_fetch(dst, value) \
        __atomic_xor_fetch((dst), (value), MEMORDER)

#define atomic_or_fetch(dst, value) \
        __atomic_or_fetch((dst), (value), MEMORDER)

#define atomic_nand_fetch(dst, value) \
        __atomic_nand_fetch((dst), (value), MEMORDER)


#define atomic_fetch_add(dst, value) \
        __atomic_fetch_add((dst), (value), MEMORDER)

#define atomic_fetch_sub(dst, value) \
        __atomic_fetch_sub((dst), (value), MEMORDER)

#define atomic_fetch_and(dst, value) \
        __atomic_fetch_and((dst), (value), MEMORDER)

#define atomic_fetch_xor(dst, value) \
        __atomic_fetch_xor((dst), (value), MEMORDER)

#define atomic_fetch_or(dst, value) \
        __atomic_fetch_or((dst), (value), MEMORDER)

#define atomic_fetch_nand(dst, value) \
        __atomic_fetch_nand((dst), (value), MEMORDER)
