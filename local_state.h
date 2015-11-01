
#ifndef LocalState_H
#define LocalState_H

#ifdef __INTEL_COMPILER
    #define DEF_ALIGN(x) __declspec(align((x)))
    #define USE_ALIGN(var, align) __assume_aligned((var), (align));
#else // GCC
    #define DEF_ALIGN(x) __attribute__ ((aligned((x))))
    #define USE_ALIGN(var, align) ((void)0) /* __builtin_assume_align is unreliabale... */
#endif
#ifdef _PARALLEL_DEVICE
    #pragma offload_attribute(push,target(mic))
#else
    #include "aligned_allocator.h"
#endif

// Class for encapsulating per-thread local state
template <class Physics>
class LocalState {

typedef typename Physics::real real;
typedef typename Physics::vec  vec;

public:
    // the node side
    LocalState(int nx, int ny)
        : nx(nx), ny(ny),
          size(nx*ny),
          serial(
              (size) +// u_ // 0
              (size) +// v_ // 1
              (size) +// f_ // 2
              (size) +// g_ // 3
              (size) +// ux_// 4
              (size) +// uy_// 5
              (size) +// fx_// 6
              (size)  // gy_// 7
          ){
        vec *start = serial.data();
        u_         = start + (0*size);
        v_         = start + (1*size);
        f_         = start + (2*size);
        g_         = start + (3*size);
        ux_        = start + (4*size);
        uy_        = start + (5*size);
        fx_        = start + (6*size);
        gy_        = start + (7*size);
    }

    // the device side
    void init(int nx, int ny, int size, vec *data) {
        serial.assign(data, data+size);
        vec *start = serial.data();
        u_         = start + (0*size);
        v_         = start + (1*size);
        f_         = start + (2*size);
        g_         = start + (3*size);
        ux_        = start + (4*size);
        uy_        = start + (5*size);
        fx_        = start + (6*size);
        gy_        = start + (7*size);
    }

    // Array accessor functions
    inline vec& u(int ix, int iy)  { return *(u_+offset(ix,iy));  }
    inline vec& v(int ix, int iy)  { return *(v_+offset(ix,iy));  }
    inline vec& f(int ix, int iy)  { return *(f_+offset(ix,iy));  }
    inline vec& g(int ix, int iy)  { return *(g_+offset(ix,iy));  }
    inline vec& ux(int ix, int iy) { return *(ux_+offset(ix,iy)); }
    inline vec& uy(int ix, int iy) { return *(uy_+offset(ix,iy)); }
    inline vec& fx(int ix, int iy) { return *(fx_+offset(ix,iy)); }
    inline vec& gy(int ix, int iy) { return *(gy_+offset(ix,iy)); }

    // Miscellaneous accessors
    inline int get_nx() { return nx; }
    inline int get_ny() { return ny; }


    #ifdef _PARALLEL_DEVICE
        typedef std::vector<vec> aligned_vector; // :'(
    #else
        typedef DEF_ALIGN(Physics::BYTE_ALIGN) std::vector<vec, aligned_allocator<vec, Physics::BYTE_ALIGN>> aligned_vector;
    #endif
    // danger: don't touch these if you don't know what you are doing...
    int nx, ny, size;
    aligned_vector serial;

private:
    // Helper to calculate 1D offset from 2D coordinates
    inline int offset(int ix, int iy) const { return iy*nx+ix; }



    vec *u_;  // Solution values
    vec *v_;  // Solution values at next step
    vec *f_;  // Fluxes in x
    vec *g_;  // Fluxes in y
    vec *ux_; // x differences of u
    vec *uy_; // y differences of u
    vec *fx_; // x differences of f
    vec *gy_; // y differences of g
};

#ifdef _PARALLEL_DEVICE
    #pragma offload_attribute(pop)
#endif

#endif // LocalState_H