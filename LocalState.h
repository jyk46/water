#ifndef LocalState_H
#define LocalState_H

/* The following allows for minimal SIMD vectorization using GCC,
 * but at the very least allows local compilation before sending
 * to the cluster.
 */
#ifdef __INTEL_COMPILER
    #define DEF_ALIGN(x) __declspec(align((x)))
    #define USE_ALIGN(var, align) __assume_aligned((var), (align));
#else // GCC
    #define DEF_ALIGN(x) __attribute__ ((aligned((x))))
    #define USE_ALIGN(var, align) ((void)0) /* __builtin_assume_align is unreliabale... */
#endif

#include <vector>
#include <xmmintrin.h>// _mm_malloc

// Class for encapsulating per-thread local state
template <class Physics>
class LocalState {

typedef typename Physics::real real;
public:
    LocalState(int nx, int ny)
        : nx(nx), ny(ny) {
    
        u_  = (real *)_mm_malloc(sizeof(real) * nx * ny * Physics::vec_size, Physics::BYTE_ALIGN);
        f_  = (real *)_mm_malloc(sizeof(real) * nx * ny * Physics::vec_size, Physics::BYTE_ALIGN);
        g_  = (real *)_mm_malloc(sizeof(real) * nx * ny * Physics::vec_size, Physics::BYTE_ALIGN);
        ux_ = (real *)_mm_malloc(sizeof(real) * nx * ny * Physics::vec_size, Physics::BYTE_ALIGN);
        uy_ = (real *)_mm_malloc(sizeof(real) * nx * ny * Physics::vec_size, Physics::BYTE_ALIGN);
        fx_ = (real *)_mm_malloc(sizeof(real) * nx * ny * Physics::vec_size, Physics::BYTE_ALIGN);
        gy_ = (real *)_mm_malloc(sizeof(real) * nx * ny * Physics::vec_size, Physics::BYTE_ALIGN);
        v_  = (real *)_mm_malloc(sizeof(real) * nx * ny * Physics::vec_size, Physics::BYTE_ALIGN);
    }

    ~LocalState() {
        // honestly...this is just crazy
        // since these are only causing
        // prblems at the end, i'm hoping
        // this won't affect the cluster
        // that much
        // _mm_free(u_ );
        // _mm_free(f_ );
        // _mm_free(g_ );
        // _mm_free(ux_);
        // _mm_free(uy_);
        // _mm_free(fx_);
        // _mm_free(gy_);
        // _mm_free(v_ );
    }

    // Array accessor functions
    inline real* u(int ix, int iy)    { return &u_[offset(ix,iy)];  }
    inline real* v(int ix, int iy)    { return &v_[offset(ix,iy)];  }
    inline real* f(int ix, int iy)    { return &f_[offset(ix,iy)];  }
    inline real* g(int ix, int iy)    { return &g_[offset(ix,iy)];  }

    inline real* ux(int ix, int iy)   { return &ux_[offset(ix,iy)]; }
    inline real* uy(int ix, int iy)   { return &uy_[offset(ix,iy)]; }
    inline real* fx(int ix, int iy)   { return &fx_[offset(ix,iy)]; }
    inline real* gy(int ix, int iy)   { return &gy_[offset(ix,iy)]; }

    // Miscellaneous accessors
    inline int get_nx() { return nx; }
    inline int get_ny() { return ny; }

private:
    // Helper to calculate 1D offset from 2D coordinates
    inline int offset(int ix, int iy) const { return (iy*nx+ix)*Physics::vec_size; }

    const int nx, ny;

    DEF_ALIGN(Physics::BYTE_ALIGN) real *u_; // Solution values
    DEF_ALIGN(Physics::BYTE_ALIGN) real *f_; // Fluxes in x
    DEF_ALIGN(Physics::BYTE_ALIGN) real *g_; // Fluxes in y
    DEF_ALIGN(Physics::BYTE_ALIGN) real *ux_;// x differences of u
    DEF_ALIGN(Physics::BYTE_ALIGN) real *uy_;// y differences of u
    DEF_ALIGN(Physics::BYTE_ALIGN) real *fx_;// x differences of f
    DEF_ALIGN(Physics::BYTE_ALIGN) real *gy_;// y differences of g
    DEF_ALIGN(Physics::BYTE_ALIGN) real *v_; // Solution values at next step
};

#endif // LocalState_H