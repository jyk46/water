#ifndef CENTRAL2D_H
#define CENTRAL2D_H

#include <cstdio>
#include <cmath>
#include <cassert>
#include <vector>

#include "aligned_allocator.h"

//ldoc on
/**
 * # Jiang-Tadmor central difference scheme
 * 
 * [Jiang and Tadmor][jt] proposed a high-resolution finite difference
 * scheme for solving hyperbolic PDE systems in two space dimensions.
 * The method is particularly attractive because, unlike many other
 * methods in this space, it does not require that we write any
 * solvers for problems with special initial data (so-called Riemann
 * problems), nor even that we compute Jacobians of the flux
 * functions.
 * 
 * While this code is based loosely on the Fortran code at the end of
 * Jiang and Tadmor's paper, we've written the current code to be
 * physics-agnostic (rather than hardwiring it to the shallow water
 * equations -- or the Euler equations in the Jiang-Tadmor paper).
 * If you're interested in the Euler equations, feel free to add your
 * own physics class to support them!
 * 
 * [jt]: http://www.cscamm.umd.edu/tadmor/pub/central-schemes/Jiang-Tadmor.SISSC-98.pdf
 * 
 * ## Staggered grids
 * 
 * The Jiang-Tadmor scheme works by alternating between a main grid
 * and a staggered grid offset by half a step in each direction.
 * Understanding this is important, particularly if you want to apply
 * a domain decomposition method and batch time steps between
 * synchronization barriers in your parallel code!
 * 
 * In even-numbered steps, the entry `u(i,j)` in the array of solution
 * values represents the average value of a cell centered at a point
 * $(x_i,y_j)$.  At the following odd-numbered step, the same entry
 * represents values for a cell centered at $(x_i + \Delta x/2, y_j +
 * \Delta y/2)$.  However, whenever we run a simulation, we always take
 * an even number of steps, so that outside the solver we can just think
 * about values on the main grid.  If `uold` and `unew` represent the
 * information at two successive *even* time steps (i.e. they represent
 * data on the same grid), then `unew(i,j)` depends indirectly on
 * `u(p,q)` for $i-3 \leq p \leq i+3$ and $j-3 \leq q \leq j+3$.
 * 
 * We currently manage this implicitly: the arrays at even time steps
 * represent cell values on the main grid, and arrays at odd steps
 * represent cell values on the staggered grid.  Our main `run` 
 * function always takes an even number of time steps to ensure we end
 * up on the primary grid.
 * 
 * ## Interface
 * 
 * We want a clean separation between the physics, the solver,
 * and the auxiliary limiter methods used by the solver.  At the same
 * time, we don't want to pay the overhead (mostly in terms of lost
 * optimization opportunities) for calling across an abstraction
 * barrier in the inner loops of our solver.  We can get around this
 * in C++ by providing the solver with *template arguments*, resolved
 * at compile time, that describe separate classes to implement the
 * physics and the limiter.
 *
 * The `Central2D` solver class takes two template arguments:
 * `Physics` and `Limiter`.  For `Physics`, we expect the name of a class
 * that defines:
 * 
 *  - A type for numerical data (`real`)
 *  - A type for solution and flux vectors in each cell (`vec`)
 *  - A flux computation function (`flux(vec& F, vec& G, const vec& U)`)
 *  - A wave speed computation function 
 *    (`wave_speed(real& cx, real& cy, const vec& U)`).
 * 
 * The `Limiter` argument is a type with a static function `limdiff`
 * with the signature
 * 
 *         limdiff(fm, f0, fp)
 * 
 * The semantics are that `fm`, `f0`, and `fp` are three successive
 * grid points in some direction, and the function returns an approximate
 * (scaled) derivative value from these points.
 * 
 * The solver keeps arrays for the solution, flux values, derivatives
 * of the solution and the fluxes, and the solution at the next time
 * point.  We use the C++ `vector` class to manage storage for these
 * arrays; but since we want to think of them as 2D arrays, we also
 * provide convenience functions to access them with multiple indices
 * (though we maintain C-style 0-based indexing).  The internal arrays
 * are padded with ghost cells; the ghost cell in the lower left corner
 * of the domain has index (0,0).
 */

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
template <class Physics, class Limiter>
class Central2D {
public:
    typedef typename Physics::real real;
    typedef typename Physics::vec  vec;

    Central2D(real w, real h,      // Domain width / height
              int nx, int ny,      // Number of cells in x/y (without ghosts)
              real cfl = 0.45f) :  // Max allowed CFL number
        nx(nx), ny(ny),
        nx_all(nx + 2*nghost),
        ny_all(ny + 2*nghost),
        dx(w/nx), dy(h/ny),
        cfl(cfl), 
        u_ (nx_all * ny_all),
        f_ (nx_all * ny_all),
        g_ (nx_all * ny_all),
        ux_(nx_all * ny_all),
        uy_(nx_all * ny_all),
        fx_(nx_all * ny_all),
        gy_(nx_all * ny_all),
        v_ (nx_all * ny_all) {}

    // Advance from time 0 to time tfinal
    void run(real tfinal);

    // Call f(Uxy, x, y) at each cell center to set initial conditions
    template <typename F>
    void init(F f);

    // Diagnostics
    void solution_check();

    // Array size accessors
    int xsize() const { return nx; }
    int ysize() const { return ny; }
    
    // Read / write elements of simulation state
    inline vec&       operator()(int i, int j) {
        return u_[offset(i+nghost,j+nghost)];
    }
    
    inline const vec& operator()(int i, int j) const {
        return u_[offset(i+nghost,j+nghost)];
    }
    
private:
    static constexpr int nghost = 3;   // Number of ghost cells

    const int nx, ny;          // Number of (non-ghost) cells in x/y
    const int nx_all, ny_all;  // Total cells in x/y (including ghost)
    const real dx, dy;         // Cell size in x/y
    const real cfl;            // Allowed CFL number

    typedef DEF_ALIGN(Physics::BYTE_ALIGN) std::vector<vec, aligned_allocator<vec, Physics::BYTE_ALIGN>> aligned_vector;

    aligned_vector u_;            // Solution values
    aligned_vector f_;            // Fluxes in x
    aligned_vector g_;            // Fluxes in y
    aligned_vector ux_;           // x differences of u
    aligned_vector uy_;           // y differences of u
    aligned_vector fx_;           // x differences of f
    aligned_vector gy_;           // y differences of g
    aligned_vector v_;            // Solution values at next step

    // Array accessor functions

    inline int offset(int ix, int iy) const { return iy*nx_all+ix; }

    inline vec& u(int ix, int iy)    { return u_[offset(ix,iy)];  }
    inline vec& v(int ix, int iy)    { return v_[offset(ix,iy)];  }
    inline vec& f(int ix, int iy)    { return f_[offset(ix,iy)];  }
    inline vec& g(int ix, int iy)    { return g_[offset(ix,iy)];  }

    inline vec& ux(int ix, int iy)   { return ux_[offset(ix,iy)]; }
    inline vec& uy(int ix, int iy)   { return uy_[offset(ix,iy)]; }
    inline vec& fx(int ix, int iy)   { return fx_[offset(ix,iy)]; }
    inline vec& gy(int ix, int iy)   { return gy_[offset(ix,iy)]; }

    // Wrapped accessor (periodic BC)
    inline int ioffset(int ix, int iy) {
        return offset( (ix+nx-nghost) % nx + nghost,
                       (iy+ny-nghost) % ny + nghost );
    }

    inline vec& uwrap(int ix, int iy)  { return u_[ioffset(ix,iy)]; }

    // Apply limiter to all components in a vector
    #pragma omp declare simd
    static inline void limdiff(real *du, const real *um, const real *u0, const real *up) {
        USE_ALIGN(du, Physics::VEC_ALIGN);
        USE_ALIGN(um, Physics::VEC_ALIGN);
        USE_ALIGN(u0, Physics::VEC_ALIGN);
        USE_ALIGN(up, Physics::VEC_ALIGN);

        #pragma ivdep
        for (int m = 0; m < Physics::vec_size; ++m)
            du[m] = Limiter::limdiff(um[m], u0[m], up[m]);
    }

    // Stages of the main algorithm
    void apply_periodic();
    void compute_fg_speeds(real& cx, real& cy);
    void limited_derivs();
    void compute_step(int io, real dt);

};


/**
 * ## Initialization
 * 
 * Before starting the simulation, we need to be able to set the
 * initial conditions.  The `init` function does exactly this by
 * running a callback function at the center of each cell in order
 * to initialize the cell $U$ value.  For the purposes of this function,
 * cell $(i,j)$ is the subdomain 
 * $[i \Delta x, (i+1) \Delta x] \times [j \Delta y, (j+1) \Delta y]$.
 */

template <class Physics, class Limiter>
template <typename F>
void Central2D<Physics, Limiter>::init(F f)
{
    for (int iy = 0; iy < ny; ++iy)
        for (int ix = 0; ix < nx; ++ix)
            f(u(nghost+ix,nghost+iy), (ix+0.5f)*dx, (iy+0.5f)*dy);
}

/**
 * ## Time stepper implementation
 * 
 * ### Boundary conditions
 * 
 * In finite volume methods, boundary conditions are typically applied by
 * setting appropriate values in ghost cells.  For our framework, we will
 * apply periodic boundary conditions; that is, waves that exit one side
 * of the domain will enter from the other side.
 * 
 * We apply the conditions by assuming that the cells with coordinates
 * `nghost <= ix <= nx+nghost` and `nghost <= iy <= ny+nghost` are
 * "canonical", and setting the values for all other cells `(ix,iy)`
 * to the corresponding canonical values `(ix+p*nx,iy+q*ny)` for some
 * integers `p` and `q`.
 */

template <class Physics, class Limiter>
void Central2D<Physics, Limiter>::apply_periodic()
{
    // Copy data between right and left boundaries
    for (int iy = 0; iy < ny_all; ++iy) {
        for (int ix = 0; ix < nghost; ++ix) {
            real *u_xy        = u(ix, iy).data();              USE_ALIGN(u_xy,        Physics::VEC_ALIGN);
            real *uwrap_xy    = uwrap(ix, iy).data();          USE_ALIGN(uwrap_xy,    Physics::VEC_ALIGN);
            real *u_ghost     = u(nx+nghost+ix,iy).data();     USE_ALIGN(u_ghost,     Physics::VEC_ALIGN);
            real *uwrap_ghost = uwrap(nx+nghost+ix,iy).data(); USE_ALIGN(uwrap_ghost, Physics::VEC_ALIGN);

            #pragma unroll
            for(int m = 0; m < Physics::vec_size; ++m) {
                u_xy[m]    = uwrap_xy[m];
                u_ghost[m] = uwrap_ghost[m];
            }
        }
    }

    // Copy data between top and bottom boundaries
    for (int ix = 0; ix < nx_all; ++ix) {
        for (int iy = 0; iy < nghost; ++iy) {
            real *u_xy        = u(ix, iy).data();              USE_ALIGN(u_xy,        Physics::VEC_ALIGN);
            real *uwrap_xy    = uwrap(ix, iy).data();          USE_ALIGN(uwrap_xy,    Physics::VEC_ALIGN);
            real *u_ghost     = u(ix,ny+nghost+iy).data();     USE_ALIGN(u_ghost,     Physics::VEC_ALIGN);
            real *uwrap_ghost = uwrap(ix,ny+nghost+iy).data(); USE_ALIGN(uwrap_ghost, Physics::VEC_ALIGN);

            #pragma unroll
            for(int m = 0; m < Physics::vec_size; ++m) {
                u_xy[m]    = uwrap_xy[m];
                u_ghost[m] = uwrap_ghost[m];
            }
        }
    }
}


/**
 * ### Initial flux and speed computations
 * 
 * At the start of each time step, we need the flux values at
 * cell centers (to advance the numerical method) and a bound
 * on the wave speeds in the $x$ and $y$ directions (so that
 * we can choose a time step that respects the specified upper
 * bound on the CFL number).
 */

template <class Physics, class Limiter>
void Central2D<Physics, Limiter>::compute_fg_speeds(real& cx_, real& cy_)
{
    using namespace std;
    real cx = 1.0e-15;
    real cy = 1.0e-15;
    for (int iy = 0; iy < ny_all; ++iy) {
        #pragma ivdep
        for (int ix = 0; ix < nx_all; ++ix) {
            real cell_cx, cell_cy;

            // gather necessary data
            real *f_xy = f(ix, iy).data(); USE_ALIGN(f_xy, Physics::VEC_ALIGN);
            real *g_xy = g(ix, iy).data(); USE_ALIGN(g_xy, Physics::VEC_ALIGN);
            real *u_xy = u(ix, iy).data(); USE_ALIGN(u_xy, Physics::VEC_ALIGN);

            Physics::flux(f_xy, g_xy, u_xy);
            Physics::wave_speed(cell_cx, cell_cy, u_xy);
            cx = max(cx, cell_cx);
            cy = max(cy, cell_cy);
        }
    }
    cx_ = cx;
    cy_ = cy;
}

/**
 * ### Derivatives with limiters
 * 
 * In order to advance the time step, we also need to estimate
 * derivatives of the fluxes and the solution values at each cell.
 * In order to maintain stability, we apply a limiter here.
 */

template <class Physics, class Limiter>
void Central2D<Physics, Limiter>::limited_derivs()
{
    for (int iy = 1; iy < ny_all-1; ++iy) {
        for (int ix = 1; ix < nx_all-1; ++ix) {
            //
            // x derivs
            //
            real *ux_x0_y0 = ux(ix, iy).data();  USE_ALIGN(ux_x0_y0, Physics::VEC_ALIGN);
            real *u_xM1_y0 = u(ix-1, iy).data(); USE_ALIGN(u_xM1_y0, Physics::VEC_ALIGN);
            real *u_x0_y0  = u(ix, iy).data();   USE_ALIGN(u_x0_y0,  Physics::VEC_ALIGN);
            real *u_xP1_y0 = u(ix+1, iy).data(); USE_ALIGN(u_xP1_y0, Physics::VEC_ALIGN);

            real *fx_x0_y0 = fx(ix, iy).data();  USE_ALIGN(fx_x0_y0, Physics::VEC_ALIGN);
            real *f_xM1_y0 = f(ix-1, iy).data(); USE_ALIGN(f_xM1_y0, Physics::VEC_ALIGN);
            real *f_x0_y0  = f(ix, iy).data();   USE_ALIGN(f_x0_y0,  Physics::VEC_ALIGN);
            real *f_xP1_y0 = f(ix+1, iy).data(); USE_ALIGN(f_xP1_y0, Physics::VEC_ALIGN);

            //
            // y derivs
            //
            real *uy_x0_y0 = uy(ix, iy).data();  USE_ALIGN(uy_x0_y0, Physics::VEC_ALIGN);
            real *u_x0_yM1 = u(ix, iy-1).data(); USE_ALIGN(u_x0_yM1, Physics::VEC_ALIGN);
            real *u_x0_yP1 = u(ix, iy+1).data(); USE_ALIGN(u_x0_yP1, Physics::VEC_ALIGN);

            real *gy_x0_y0 = gy(ix, iy).data();  USE_ALIGN(gy_x0_y0, Physics::VEC_ALIGN);
            real *g_x0_yM1 = g(ix, iy-1).data(); USE_ALIGN(g_x0_yM1, Physics::VEC_ALIGN);
            real *g_x0_y0  = g(ix, iy).data();   USE_ALIGN(g_x0_y0,  Physics::VEC_ALIGN);
            real *g_x0_yP1 = g(ix, iy+1).data(); USE_ALIGN(g_x0_yP1, Physics::VEC_ALIGN);

            limdiff( ux_x0_y0, u_xM1_y0, u_x0_y0, u_xP1_y0 );
            limdiff( fx_x0_y0, f_xM1_y0, f_x0_y0, f_xP1_y0 );
            limdiff( uy_x0_y0, u_x0_yM1, u_x0_y0, u_x0_yP1 );
            limdiff( gy_x0_y0, g_x0_yM1, g_x0_y0, g_x0_yP1 );
        }
    }
}


/**
 * ### Advancing a time step
 * 
 * Take one step of the numerical scheme.  This consists of two pieces:
 * a first-order corrector computed at a half time step, which is used
 * to obtain new $F$ and $G$ values; and a corrector step that computes
 * the solution at the full step.  For full details, we refer to the
 * [Jiang and Tadmor paper][jt].
 * 
 * The `compute_step` function takes two arguments: the `io` flag
 * which is the time step modulo 2 (0 if even, 1 if odd); and the `dt`
 * flag, which actually determines the time step length.  We need
 * to know the even-vs-odd distinction because the Jiang-Tadmor
 * scheme alternates between a primary grid (on even steps) and a
 * staggered grid (on odd steps).  This means that the data at $(i,j)$
 * in an even step and the data at $(i,j)$ in an odd step represent
 * values at different locations in space, offset by half a space step
 * in each direction.  Every other step, we shift things back by one
 * mesh cell in each direction, essentially resetting to the primary
 * indexing scheme.
 */

template <class Physics, class Limiter>
void Central2D<Physics, Limiter>::compute_step(int io, real dt)
{
    real dtcdx2 = 0.5f * dt / dx;
    real dtcdy2 = 0.5f * dt / dy;

    real uh_copy[] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Predictor (flux values of f and g at half step)
    for (int iy = 1; iy < ny_all-1; ++iy) {
        #pragma simd
        for (int ix = 1; ix < nx_all-1; ++ix) {
            // gather the necessary information
            real *uh    = u(ix,iy).data();   USE_ALIGN(uh,    Physics::VEC_ALIGN);
            real *fx_xy = fx(ix, iy).data(); USE_ALIGN(fx_xy, Physics::VEC_ALIGN);
            real *gy_xy = gy(ix, iy).data(); USE_ALIGN(gy_xy, Physics::VEC_ALIGN);
            real *f_xy  = f(ix, iy).data();  USE_ALIGN(f_xy,  Physics::VEC_ALIGN);
            real *g_xy  = g(ix, iy).data();  USE_ALIGN(g_xy,  Physics::VEC_ALIGN);

            // be careful not to modify u!!!            
            #pragma unroll
            for(int m = 0; m < Physics::vec_size; ++m) uh_copy[m] = uh[m];
            
            #pragma unroll
            for (int m = 0; m < Physics::vec_size; ++m) {
                uh_copy[m] -= dtcdx2 * fx_xy[m];
                uh_copy[m] -= dtcdy2 * gy_xy[m];
            }
            Physics::flux(f_xy, g_xy, uh_copy);
        }
    }

    // Corrector (finish the step)
    for (int iy = nghost-io; iy < ny+nghost-io; ++iy) {
        for (int ix = nghost-io; ix < nx+nghost-io; ++ix) {
            /* Nomenclature:
             *     u_x0_y0 <- u(ix  , iy  )
             *     u_x1_y0 <- u(ix+1, iy  )
             *     u_x0_y1 <- u(ix  , iy+1)
             *     u_x1_y1 <- u(ix+1, iy+1)
             */
            // The final result
            real *v_ix_iy = v(ix, iy).data();       USE_ALIGN(v_ix_iy,  Physics::VEC_ALIGN );

            // grab u
            real *u_x1_y0 = u(ix+1, iy  ).data();   USE_ALIGN(u_x1_y0,  Physics::VEC_ALIGN );
            real *u_x0_y0 = u(ix  , iy  ).data();   USE_ALIGN(u_x0_y0,  Physics::VEC_ALIGN );
            real *u_x0_y1 = u(ix  , iy+1).data();   USE_ALIGN(u_x0_y1,  Physics::VEC_ALIGN );
            real *u_x1_y1 = u(ix+1, iy+1).data();   USE_ALIGN(u_x1_y1,  Physics::VEC_ALIGN );

            // grab ux
            real *ux_x0_y0 = ux(ix  , iy  ).data(); USE_ALIGN(ux_x0_y0, Physics::VEC_ALIGN );
            real *ux_x1_y0 = ux(ix+1, iy  ).data(); USE_ALIGN(ux_x1_y0, Physics::VEC_ALIGN );
            real *ux_x0_y1 = ux(ix  , iy+1).data(); USE_ALIGN(ux_x0_y1, Physics::VEC_ALIGN );
            real *ux_x1_y1 = ux(ix+1, iy+1).data(); USE_ALIGN(ux_x1_y1, Physics::VEC_ALIGN );

            // grab uy
            real *uy_x0_y0 = uy(ix  , iy  ).data(); USE_ALIGN(uy_x0_y0, Physics::VEC_ALIGN );
            real *uy_x1_y0 = uy(ix+1, iy  ).data(); USE_ALIGN(uy_x1_y0, Physics::VEC_ALIGN );
            real *uy_x0_y1 = uy(ix  , iy+1).data(); USE_ALIGN(uy_x0_y1, Physics::VEC_ALIGN );
            real *uy_x1_y1 = uy(ix+1, iy+1).data(); USE_ALIGN(uy_x1_y1, Physics::VEC_ALIGN );

            // grab f
            real *f_x0_y0 = f(ix  , iy  ).data();   USE_ALIGN(f_x0_y0,  Physics::VEC_ALIGN );
            real *f_x1_y0 = f(ix+1, iy  ).data();   USE_ALIGN(f_x1_y0,  Physics::VEC_ALIGN );
            real *f_x0_y1 = f(ix  , iy+1).data();   USE_ALIGN(f_x0_y1,  Physics::VEC_ALIGN );
            real *f_x1_y1 = f(ix+1, iy+1).data();   USE_ALIGN(f_x1_y1,  Physics::VEC_ALIGN );

            // grab g
            real *g_x0_y0 = g(ix  , iy  ).data();   USE_ALIGN(g_x0_y0,  Physics::VEC_ALIGN );
            real *g_x1_y0 = g(ix+1, iy  ).data();   USE_ALIGN(g_x1_y0,  Physics::VEC_ALIGN );
            real *g_x0_y1 = g(ix  , iy+1).data();   USE_ALIGN(g_x0_y1,  Physics::VEC_ALIGN );
            real *g_x1_y1 = g(ix+1, iy+1).data();   USE_ALIGN(g_x1_y1,  Physics::VEC_ALIGN );

            #pragma simd
            for(int m = 0; m < Physics::vec_size; ++m) {
                v_ix_iy[m] =
                    0.2500f * ( u_x0_y0[m]  + u_x1_y0[m]    +
                                u_x0_y1[m]  + u_x1_y1[m]  ) -
                    0.0625f * ( ux_x1_y0[m] - ux_x0_y0[m]   +
                                ux_x1_y1[m] - ux_x0_y1[m]   +
                                uy_x0_y1[m] - uy_x0_y0[m]   +
                                uy_x1_y1[m] - uy_x1_y0[m] ) -
                    dtcdx2  * ( f_x1_y0[m]  - f_x0_y0[m]    +
                                f_x1_y1[m]  - f_x0_y1[m]  ) -
                    dtcdy2  * ( g_x0_y1[m]  - g_x0_y0[m]    +
                                g_x1_y1[m]  - g_x1_y0[m]  );                    
            }
        }
    }

    // Copy from v storage back to main grid
    for (int j = nghost; j < ny+nghost; ++j){
        for (int i = nghost; i < nx+nghost; ++i){
            real *u_ij = u(i, j).data();
            real *v_ij_io = v(i-io, j-io).data();

            #pragma unroll
            for(int m = 0; m < Physics::vec_size; ++m) u_ij[m] = v_ij_io[m];
        }
    }
}


/**
 * ### Advance time
 * 
 * The `run` method advances from time 0 (initial conditions) to time
 * `tfinal`.  Note that `run` can be called repeatedly; for example,
 * we might want to advance for a period of time, write out a picture,
 * advance more, and write another picture.  In this sense, `tfinal`
 * should be interpreted as an offset from the time represented by
 * the simulator at the start of the call, rather than as an absolute time.
 * 
 * We always take an even number of steps so that the solution
 * at the end lives on the main grid instead of the staggered grid. 
 */

template <class Physics, class Limiter>
void Central2D<Physics, Limiter>::run(real tfinal)
{
    bool done = false;
    real t = 0;
    while (!done) {
        real dt;
        for (int io = 0; io < 2; ++io) {
            real cx, cy;
            apply_periodic();
            compute_fg_speeds(cx, cy);
            limited_derivs();
            if (io == 0) {
                dt = cfl / std::max(cx/dx, cy/dy);
                if (t + 2*dt >= tfinal) {
                    dt = (tfinal-t)/2;
                    done = true;
                }
            }
            compute_step(io, dt);
            t += dt;
        }
    }
}

/**
 * ### Diagnostics
 * 
 * The numerical method is supposed to preserve (up to rounding
 * errors) the total volume of water in the domain and the total
 * momentum.  Ideally, we should also not see negative water heights,
 * since that will cause the system of equations to blow up.  For
 * debugging convenience, we'll plan to periodically print diagnostic
 * information about these conserved quantities (and about the range
 * of water heights).
 */

template <class Physics, class Limiter>
void Central2D<Physics, Limiter>::solution_check()
{
    using namespace std;
    real h_sum = 0, hu_sum = 0, hv_sum = 0;
    real hmin = u(nghost,nghost)[0];
    real hmax = hmin;
    for (int j = nghost; j < ny+nghost; ++j)
        for (int i = nghost; i < nx+nghost; ++i) {
            vec& uij = u(i,j);
            real h = uij[0];
            h_sum += h;
            hu_sum += uij[1];
            hv_sum += uij[2];
            hmax = max(h, hmax);
            hmin = min(h, hmin);
            assert( h > 0) ;
        }
    real cell_area = dx*dy;
    h_sum *= cell_area;
    hu_sum *= cell_area;
    hv_sum *= cell_area;
    printf("-\n  Volume: %g\n  Momentum: (%g, %g)\n  Range: [%g, %g]\n",
           h_sum, hu_sum, hv_sum, hmin, hmax);
}

//ldoc off
#endif /* CENTRAL2D_H*/
