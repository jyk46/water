/**
 * This aligned allocator has been shamelessly stolen from:
 *     https://gist.github.com/donny-dont/1471329
 *
 * We take absolutely no credit for its splendor, only for recognizing
 * said splendor ;)
 */
#include "aligned_allocator.hpp"
//
//////////////////////// These needed to be added to compile on its own:
#include <stdexcept>  // std::length_error
#include <xmmintrin.h>// _mm_malloc
////////////////////////
//
template <typename T, std::size_t Alignment>
T * aligned_allocator<T, Alignment>::address(T& r) const
{
    return &r;
}

template <typename T, std::size_t Alignment>
const T * aligned_allocator<T, Alignment>::address(const T& s) const
{
    return &s;
}

template <typename T, std::size_t Alignment>
std::size_t aligned_allocator<T, Alignment>::max_size() const
{
    // The following has been carefully written to be independent of
    // the definition of size_t and to avoid signed/unsigned warnings.
    return (static_cast<std::size_t>(0) - static_cast<std::size_t>(1)) / sizeof(T);
}

template <typename T, std::size_t Alignment>
void aligned_allocator<T, Alignment>::construct(T * const p, const T& t) const
{
    void * const pv = static_cast<void *>(p);

    new (pv) T(t);
}

template <typename T, std::size_t Alignment>
void aligned_allocator<T, Alignment>::destroy(T * const p) const
{
    p->~T();
}


// The following will be different for each allocator.
template <typename T, std::size_t Alignment>
T * aligned_allocator<T, Alignment>::allocate(const std::size_t n) const
{
    // The return value of allocate(0) is unspecified.
    // Mallocator returns NULL in order to avoid depending
    // on malloc(0)'s implementation-defined behavior
    // (the implementation can define malloc(0) to return NULL,
    // in which case the bad_alloc check below would fire).
    // All allocators can return NULL in this case.
    if (n == 0) {
        return NULL;
    }

    // All allocators should contain an integer overflow check.
    // The Standardization Committee recommends that std::length_error
    // be thrown in the case of integer overflow.
    if (n > max_size())
    {
        throw std::length_error("aligned_allocator<T>::allocate() - Integer overflow.");
    }

    // Mallocator wraps malloc().
    void * const pv = _mm_malloc(n * sizeof(T), Alignment);

    // Allocators should throw std::bad_alloc in the case of memory allocation failure.
    if (pv == NULL)
    {
        throw std::bad_alloc();
    }

    return static_cast<T *>(pv);
}

template <typename T, std::size_t Alignment>
void aligned_allocator<T, Alignment>::deallocate(T * const p, const std::size_t n) const
{
    _mm_free(p);
}

/**
 * In order to use this within the rest of the framework, we have removed this main method but
 * left it here for completeness.
 */
int main()
{
    typedef std::vector<__m128, aligned_allocator<__m128, sizeof(__m128)> > aligned_vector;
    aligned_vector lhs;
    aligned_vector rhs;

    float a = 1.0f;
    float b = 2.0f;
    float c = 3.0f;
    float d = 4.0f;

    float e = 5.0f;
    float f = 6.0f;
    float g = 7.0f;
    float h = 8.0f;

    for (std::size_t i = 0; i < 1000; ++i)
    {
        lhs.push_back(_mm_set_ps(a, b, c, d));
        rhs.push_back(_mm_set_ps(e, f, g, h));

        a += 1.0f; b += 1.0f; c += 1.0f; d += 1.0f;
        e += 1.0f; f += 1.0f; g += 1.0f; h += 1.0f;
    }

    __m128 mul = _mm_mul_ps(lhs[10], rhs[10]);
}
