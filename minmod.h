#ifndef MINMOD_H
#define MINMOD_H

#ifdef _PARALLEL_DEVICE
    #pragma offload_attribute(push,target(mic))
#endif

#include <cmath>

//ldoc on
/**
 * # MinMod limiter
 * 
 * Numerical methods for solving nonlinear wave equations are
 * complicated by the fact that even with smooth initial data, a
 * nonlinear wave can develop discontinuities (shocks) in finite time.
 * 
 * This makes for interesting analysis, since a "strong" solution
 * that satisfies the differential equation no longer makes sense at
 * a shock -- instead, we have to come up with some mathematically
 * and physically reasonable definition of a "weak" solution that
 * satisfies the PDE away from the shock and satisfies some other
 * condition (an entropy condition) at the shock.
 * 
 * The presence of shocks also makes for interesting *numerical*
 * analysis, because we need to be careful about employing numerical
 * differentiation formulas that sample a discontinuous function at
 * points on different sides of a shock.  Using such formulas naively
 * usually causes the numerical method to become unstable.  A better
 * method -- better even in the absence of shocks! -- is to consider
 * multiple numerical differentiation formulas and use the highest
 * order one that "looks reasonable" in the sense that it doesn't
 * predict wildly larger slopes than the others.  Because these
 * combined formulas *limit* the wild behavior of derivative estimates
 * across a shock, we call them *limiters*.  With an appropriate limiter,
 * we can construct methods that have high-order accuracy away from shocks
 * and are at least first-order accurate close to a shock.  These are
 * sometimes called *high-resolution* methods.
 * 
 * The MinMod (minimum modulus) limiter is one example of a limiter.
 * The MinMod limiter estimates the slope through points $f_-, f_0, f_+$
 * (with the step $h$ scaled to 1) by
 * $$
 *   f' = \operatorname{minmod}((f_+-f_-)/2, \theta(f_+-f_0), \theta(f_0-f_-))
 * $$
 * where the minmod function returns the argument with smallest absolute
 * value if all arguments have the same sign, and zero otherwise.
 * Common choices of $\theta$ are $\theta = 1.0$ and $\theta = 2.0$.
 * 
 * The minmod limiter *looks* like it should be expensive to computer,
 * since superficially it seems to require a number of branches.
 * We do something a little tricky, getting rid of the condition
 * on the sign of the arguments using the `copysign` instruction.
 * If the compiler does the "right" thing with `max` and `min`
 * for floating point arguments (translating them to branch-free
 * intrinsic operations), this implementation should be relatively fast.
 * 
 * There are many other potential choices of limiters as well.  We'll
 * stick with this one for the code, but you should feel free to
 * experiment with others if you know what you're doing and think it
 * will improve performance or accuracy.
 */

template <class real>
struct MinMod {
    static constexpr real theta = 2.0f;

    // Branch-free computation of minmod of two numbers
    #pragma omp declare simd
    static inline real xmin2s(real s, real a, real b) {
        real sa = copysignf(s, a);
        real sb = copysignf(s, b);
        real abs_a = fabsf(a);
        real abs_b = fabsf(b);
        real min_abs = (abs_a < abs_b ? abs_a : abs_b);
        return (sa+sb) * min_abs;
    }

    // Limited combined slope estimate
    #pragma omp declare simd
    static inline real limdiff(real um, real u0, real up) {
        real du1 = u0-um;         // Difference to left
        real du2 = up-u0;         // Difference to right
        real duc = up-um;         // Centered difference
        return xmin2s( 0.25f, xmin2s(theta, du1, du2), duc );
    }


};

#ifdef _PARALLEL_DEVICE
    #pragma offload_attribute(pop)
#endif
//ldoc off
#endif /* MINMOD_H */
