/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010 by Joel Andersson, Moritz Diehl, K.U.Leuven. All rights reserved.
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef KINSOL_SOLVER_HPP
#define KINSOL_SOLVER_HPP

#include "symbolic/fx/implicit_function.hpp"
#include "symbolic/fx/linear_solver.hpp"

namespace CasADi{
  
  // Forward declaration of internal class 
  class KinsolInternal;

  /** \brief Kinsol solver class
   *
   *
   * @copydoc ImplicitFunction_doc
   * You can provide an initial guess by setting output(0).\n
   * A good initial guess may be needed to avoid errors like "The linear solver's setup function failed in an unrecoverable manner."
   *
   The constraints option expects an integer entry for each variable u:\n
    
   0 then no constraint is imposed on ui. \n
   1 then ui will be constrained to be ui >= 0.0. \n
   −1 then ui will be constrained to be ui <= 0.0. \n
   2 then ui will be constrained to be ui > 0.0. \n
   −2 then ui will be constrained to be ui < 0.0. \n

   *
   * \see ImplicitFunction for more information
   * 
   */
  class KinsolSolver : public ImplicitFunction{
  public:

    /** \brief  Default constructor */
    KinsolSolver();
  
    /** \brief  Create an KINSOL instance
     *  
     * \param f FX mapping from (n+1) inputs to 1 output
     *
     */
    explicit KinsolSolver(const FX& f, const FX& jac=FX(), const LinearSolver& linsol=LinearSolver());
  
    /** \brief  Access functions of the node */
    KinsolInternal* operator->();

    /** \brief  Const access functions of the node */
    const KinsolInternal* operator->() const;

    /// Check if the node is pointing to the right type of object
    virtual bool checkNode() const;
  
    /// Static creator function
#ifdef SWIG
    %callback("%s_cb");
#endif
    static ImplicitFunction creator(const FX& f, const FX& jac, const LinearSolver& linsol){ return KinsolSolver(f,jac,linsol);}
#ifdef SWIG
    %nocallback;
#endif

  };

} // namespace CasADi

#endif //KINSOL_SOLVER_HPP

