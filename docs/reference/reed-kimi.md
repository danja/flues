In a waveguide wood-wind model the reed is not represented by a separate “impedance block” in the usual electrical-analog sense.

Instead its mechanical behaviour is collapsed into a signal-dependent, memory-less reflection coefficient that terminates the bore waveguide.  The steps are:

1.  Neglect the reed mass (valid above ≈ 1 kHz).

    The reed then behaves as a spring-controlled valve whose opening height h depends only on the instantaneous pressure difference

    Δp = P_mouth − P_bore(0,t).

2.  Static flow curve

    Measure or compute the stationary volume-velocity curve

    u = f(Δp)  (usually a cubic-like function with an inflection point).

    In the wave-domain this is recast as

    u = u⁺ − u⁻ , P_bore = Z₀(u⁺ + u⁻)  (Z₀ = ρc/S).

3.  Reflection coefficient

    Solve the two equations for the outgoing wave:

    u⁻ = u⁺ − f(Δp)  with Δp = P_mouth − Z₀(u⁺ + u⁻).

    The result is a nonlinear map  

    
    u⁻ = R(u⁺ ; P_mouth) .

    R is the reflection function of the reed; it is implemented as a 1-D lookup table or a short polynomial whose single input is the incoming pressure wave u⁺ (or equivalently Δp).  The table is recomputed when the player changes embouchure (reed stiffness, offset, damping).

4.  Junction view

    Seen from the bore, the reed therefore presents an impedance  

    
    Z_r(Δp) = Δp / u(Δp)

    but in the waveguide this is never explicitly needed; the reflection coefficient  

    
    ρ(Δp) = (Z_r − Z₀)/(Z_r + Z₀)

    is stored directly in the lookup table, so the reed junction is a one-port, nonlinear scatterer that closes the delay line.

5.  Including upstream impedance

    If one wishes to keep the reed mass or to add the player’s oral tract, the reed is replaced by a lumped impedance Z_r in series with the upstream impedance Z_u and the bore impedance Z_d.  The scattering equations are then solved at a 3-port junction:

    
    P = (Z_u⁻¹P_u + Z_d⁻¹P_d + Z_r⁻¹P_r)/(Z_u⁻¹ + Z_d⁻¹ + Z_r⁻¹)

    and the reflected waves are computed with the standard reflectance formulae .  In real-time implementations the reed mass is usually folded into a second-order filter that is merged with the oral-tract resonator, but the visible part that terminates the bore waveguide is still the single nonlinear lookup table described in step 3.

Hence, in the minimal waveguide clarinet the reed is modelled as a pressure-controlled, instantaneous reflection coefficient—a one-sample, nonlinear scattering termination—rather than as an explicit impedance network.