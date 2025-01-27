/* =====================================================================================
TChem version 2.0
Copyright (2020) NTESS
https://github.com/sandialabs/TChem

Copyright 2020 National Technology & Engineering Solutions of Sandia, LLC (NTESS).
Under the terms of Contract DE-NA0003525 with NTESS, the U.S. Government retains
certain rights in this software.

This file is part of TChem. TChem is open source software: you can redistribute it
and/or modify it under the terms of BSD 2-Clause License
(https://opensource.org/licenses/BSD-2-Clause). A copy of the licese is also
provided under the main directory

Questions? Contact Cosmin Safta at <csafta@sandia.gov>, or
           Kyungjoo Kim at <kyukim@sandia.gov>, or
           Oscar Diaz-Ibarra at <odiazib@sandia.gov>

Sandia National Laboratories, Livermore, CA, USA
===================================================================================== */
#ifndef __TCHEM_IMPL_RATEOFPROGRESS_SURFACE_IND_HPP__
#define __TCHEM_IMPL_RATEOFPROGRESS_SURFACE_IND_HPP__
// Compute rate of progess Forward and reverse
// Use this function when compute rate of progess from another application
// example csp
#include "TChem_Impl_Gk.hpp"
#include "TChem_Impl_KForwardReverseSurface.hpp"
#include "TChem_Impl_MolarConcentrations.hpp"
#include "TChem_Impl_RateOfProgressSurface.hpp"
#include "TChem_Util.hpp"

namespace TChem {
namespace Impl {
template<typename ValueType, typename DeviceType>
struct RateOfProgressSurfaceInd
{

  using value_type = ValueType;
  using device_type = DeviceType;
  using scalar_type = typename ats<value_type>::scalar_type;
  using real_type = scalar_type;

  using real_type_1d_view_type = Tines::value_type_1d_view<real_type,device_type>;
  using value_type_1d_view_type = Tines::value_type_1d_view<value_type,device_type>;
  /// sacado is value type
  using ordinary_type_1d_view_type = Tines::value_type_1d_view<ordinal_type,device_type>;

  using kinetic_surf_model_type = KineticSurfModelConstData<device_type>;
  using kinetic_model_type= KineticModelConstData<device_type>;

  template<typename MemberType>
  KOKKOS_INLINE_FUNCTION static void team_invoke_detail(
    const MemberType& member,
    /// input
    const value_type& t,
    const value_type& p,
    const value_type& density,
    const value_type_1d_view_type& Yk, /// (kmcd.nSpec)
    const value_type_1d_view_type& zSurf,
    /// output
    const value_type_1d_view_type& ropFor,
    const value_type_1d_view_type& ropRev,
    /// workspace
    // gas species
    const value_type_1d_view_type& gk,
    const value_type_1d_view_type& hks,
    const value_type_1d_view_type& cpks,
    // surface species
    const value_type_1d_view_type& Surf_gk,
    const value_type_1d_view_type& Surf_hks,
    const value_type_1d_view_type& Surf_cpks,

    const value_type_1d_view_type& concX,
    const value_type_1d_view_type& concXSurf,
    // const value_type_1d_view_type &concM,
    const value_type_1d_view_type& kfor,
    const value_type_1d_view_type& krev,

    // const real_type_1d_view_type &Crnd,
    const ordinary_type_1d_view_type& iter,
    /// const input from kinetic model
    const kinetic_model_type& kmcd,
    /// const input from surface kinetic model
    const kinetic_surf_model_type& kmcdSurf)
  {
    ///const real_type zero(0); ///not used
    const real_type one(1);
    const real_type ten(10);

    using MolarConcentrations = MolarConcentrations<value_type,device_type>;
    using GkSurfGas = GkFcnSurfGas<value_type,device_type>;
    using KForwardReverseSurface = KForwardReverseSurface<value_type,device_type>;
    using RateOfProgressSurface = RateOfProgressSurface<value_type,device_type>;

    // is Xc  mass fraction or concetration ?

    /* 1. compute molar concentrations from mass fraction (moles/cm3) */
    MolarConcentrations::team_invoke(member,
                                     t,
                                     p,
                                     density,
                                     Yk, // need to be mass fraction
                                     concX,
                                     kmcd);

    /// 2. initialize and transform molar concentrations (kmol/m3) to
    /// (moles/cm3)
    member.team_barrier();
    {
      const real_type one_e_minus_three(1e-3);
      Kokkos::parallel_for(
        Tines::RangeFactory<value_type>::TeamVectorRange(member, kmcd.nSpec),
        [&](const ordinal_type& i) { concX(i) *= one_e_minus_three; });
    }

    /* 2. surface molar concentrations (moles/cm2) */
    /* FR: note there is one type of site, and site occupancy \sigma_k =1.0 */
    {
      Kokkos::parallel_for(Tines::RangeFactory<value_type>::TeamVectorRange(member, kmcdSurf.nSpec),
                           [&](const ordinal_type& i) {
                             concXSurf(i) =
                               zSurf(i) * kmcdSurf.sitedensity / one;
                           });
    }

    /*3. compute (-ln(T)+dS/R-dH/RT) for each species */
    /* We are computing (dS-dH)/R for each species
    gk is Gibbs free energy */

    GkSurfGas ::team_invoke(member,
                            t, /// input
                            gk,
                            hks,  /// output
                            cpks, /// workspace
                            kmcd);
    // member.team_barrier();

    // surfaces
    GkSurfGas ::team_invoke(member,
                            t, /// input
                            Surf_gk,
                            Surf_hks,  /// output
                            Surf_cpks, /// workspace
                            kmcdSurf);

    member.team_barrier();

    /* compute forward and reverse rate constants */
    KForwardReverseSurface ::team_invoke(member,
                                         t,
                                         p,
                                         gk,
                                         Surf_gk, /// input
                                         kfor,
                                         krev, /// output
                                         kmcd,    // gas info
                                         kmcdSurf // surface info
    );
    member.team_barrier();

    // /// compute rate-of-progress
    RateOfProgressSurface::team_invoke(member,
                                       t,
                                       kfor,
                                       krev,
                                       concX,
                                       concXSurf, /// input
                                       ropFor,
                                       ropRev, /// output
                                       iter,   /// workspace for iterators
                                       kmcdSurf);

    member.team_barrier();

    /* transform from mole/(cm2.s) to kmol/(m2.s) */
    /* check this unit convection*/

    Kokkos::parallel_for(Tines::RangeFactory<value_type>::TeamVectorRange(member, kmcdSurf.nReac),
                         [&](const ordinal_type& i) {
                           ropFor(i) *= ten;
                           ropRev(i) *= ten;
                         });
  }

  template<typename MemberType,
           typename WorkViewType>

  KOKKOS_FORCEINLINE_FUNCTION static void team_invoke(
    const MemberType& member,
    /// input
    const real_type& t,
    const real_type& p,
    const real_type& density,
    const real_type_1d_view_type& Yk,    /// (kmcd.nSpec)
    const real_type_1d_view_type& zSurf, //(kmcdSurf.nSpec)

    /// output
    const real_type_1d_view_type& ropFor, /// (kmcd.nSpec)
    const real_type_1d_view_type& ropRev,
    /// workspace
    const WorkViewType& work,
    /// const input from kinetic model
    const kinetic_model_type& kmcd,
    /// const input from surface kinetic model
    const kinetic_surf_model_type& kmcdSurf)
  {
    ///const real_type zero(0); /// not used

    ///
    /// workspace needed gk, hks, kfor, krev
    ///
    auto w = (real_type*)work.data();

    // gas species thermal properties // uses a different model for that gas
    // phase
    auto gk = real_type_1d_view_type(w, kmcd.nSpec);
    w += kmcd.nSpec;
    auto hks = real_type_1d_view_type(w, kmcd.nSpec);
    w += kmcd.nSpec;
    auto cpks = real_type_1d_view_type(w, kmcd.nSpec);
    w += kmcd.nSpec;
    auto concX = real_type_1d_view_type(w, kmcd.nSpec);
    w += kmcd.nSpec;

    auto kfor = real_type_1d_view_type(w, kmcdSurf.nReac);
    w += kmcdSurf.nReac;
    auto krev = real_type_1d_view_type(w, kmcdSurf.nReac);
    w += kmcdSurf.nReac;

    // surface species thermal properties
    auto Surf_gk = real_type_1d_view_type(w, kmcdSurf.nSpec);
    w += kmcdSurf.nSpec;
    auto Surf_hks = real_type_1d_view_type(w, kmcdSurf.nSpec);
    w += kmcdSurf.nSpec;
    auto Surf_cpks = real_type_1d_view_type(w, kmcdSurf.nSpec);
    w += kmcdSurf.nSpec;
    auto concXSurf = real_type_1d_view_type(w, kmcdSurf.nSpec);
    w += kmcdSurf.nSpec;


    auto iter = ordinary_type_1d_view_type((ordinal_type*)w, kmcdSurf.nReac * 2);

    w += kmcdSurf.nReac * 2;

    team_invoke_detail(member,
                       t,
                       p,
                       density,
                       Yk,
                       zSurf,
                       ropFor,
                       ropRev,
                       gk,
                       hks,
                       cpks,
                       Surf_gk,
                       Surf_hks,
                       Surf_cpks,
                       concX,
                       concXSurf,
                       kfor,
                       krev,
                       iter,
                       kmcd,
                       kmcdSurf);
  }
};

} // namespace Impl
} // namespace TChem

#endif
