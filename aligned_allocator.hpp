/**
 * This aligned allocator has been shamelessly stolen from:
 *     https://gist.github.com/donny-dont/1471329
 *
 * We take absolutely no credit for its splendor, only for recognizing
 * said splendor ;)
 */
#ifdef _WIN32
#include <malloc.h>
#endif
#include <cstdint>
#include <vector>
#include <iostream>
//
//////////////////////// These needed to be added to compile on its own:
#include <cstddef>    // ptrdiff_t
////////////////////////
//
/**
 * Allocator for aligned data.
 *
 * Modified from the Mallocator from Stephan T. Lavavej.
 * <http://blogs.msdn.com/b/vcblog/archive/2008/08/28/the-mallocator.aspx>
 */
template <typename T, std::size_t Alignment>
class aligned_allocator
{
    public:
 
        // The following will be the same for virtually all allocators.
        typedef T * pointer;
        typedef const T * const_pointer;
        typedef T& reference;
        typedef const T& const_reference;
        typedef T value_type;
        typedef std::size_t size_type;
        typedef ptrdiff_t difference_type;
 
        T * address(T& r) const;
        const T * address(const T& s) const; 
        std::size_t max_size() const;
 
        // The following must be the same for all allocators.
        template <typename U>
        struct rebind
        {
            typedef aligned_allocator<U, Alignment> other;
        } ;
 
        bool operator!=(const aligned_allocator& other) const
        {
            return !(*this == other);
        }
 
        void construct(T * const p, const T& t) const; 
        void destroy(T * const p) const;
 
        // Returns true if and only if storage allocated from *this
        // can be deallocated from other, and vice versa.
        // Always returns true for stateless allocators.
        bool operator==(const aligned_allocator& other) const
        {
            return true;
        }
 
 
        // Default constructor, copy constructor, rebinding constructor, and destructor.
        // Empty for stateless allocators.
        aligned_allocator() { }
 
        aligned_allocator(const aligned_allocator&) { }
 
        template <typename U> aligned_allocator(const aligned_allocator<U, Alignment>&) { }
 
        ~aligned_allocator() { }
 
 
        // The following will be different for each allocator.
        T * allocate(const std::size_t n) const;
        void deallocate(T * const p, const std::size_t n) const; 
 
        // The following will be the same for all allocators that ignore hints.
        template <typename U>
        T * allocate(const std::size_t n, const U * /* const hint */) const
        {
            return allocate(n);
        }
 
 
        // Allocators are not required to be assignable, so
        // all allocators should have a private unimplemented
        // assignment operator. Note that this will trigger the
        // off-by-default (enabled under /Wall) warning C4626
        // "assignment operator could not be generated because a
        // base class assignment operator is inaccessible" within
        // the STL headers, but that warning is useless.
    private:
        aligned_allocator& operator=(const aligned_allocator&);
};
