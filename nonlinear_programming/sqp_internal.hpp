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

#ifndef SQP_INTERNAL_HPP
#define SQP_INTERNAL_HPP

#include "sqp_method.hpp"
#include "symbolic/fx/nlp_solver_internal.hpp"
#include "symbolic/fx/qp_solver.hpp"
#include <deque>

namespace CasADi{
    
class SQPInternal : public NLPSolverInternal{

public:
  explicit SQPInternal(const FX& nlp);
  virtual ~SQPInternal();
  virtual SQPInternal* clone() const{ return new SQPInternal(*this);}
  
  virtual void init();
  virtual void evaluate(int nfdir, int nadir);
  
  /// QP solver for the subproblems
  QPSolver qp_solver_;

  /// Exact Hessian?
  bool exact_hessian_;

  /// maximum number of sqp iterations
  int max_iter_; 

  /// Memory size of L-BFGS method
  int lbfgs_memory_;
  /// Tolerance of primal infeasibility
  double tol_pr_;
  /// Tolerance of dual infeasibility
  double tol_du_;

  // Merit function parameters
  double nu_;
  double muR_;
  double muLS;
  double Merit_,Merit_cand_, Merit_mu_,Merit_mu_cand_;
  

  // Optimality measure and adjustment parameters
  double tau_;
  double phiWeight_;
  double yMax_;
  double phiComb_;
  double phiMaxO_, phiMaxV_;
  
  

  /// Linesearch parameters
  //@{
  double sigma_;
  double c1_;
  double beta_;
  int max_iter_ls_;
  int merit_memsize_;
  double sigmaMax_;
  double dvMax_;
  double alphaMin_;
  //@}

  /// Hessian regularization
  double reg_;
  double eps_active_;
  double muH_;

  /// Access QPSolver
  const QPSolver getQPSolver() const { return qp_solver_;}
  
  /// Lagrange multipliers of the NLP
  std::vector<double> mu_, mu_x_, mu_e_, pi_, pi2_;

  /// gradient of the merit function
  std::vector<double> gradm_, gradms_;
  
  /// Current cost function value
  double fk_, fk_cand_;

  /// Norms and scaling
  double normc_, normcs_, normJ_, normgf_, scaleglag_, scaleg_, normc_cand_, normcs_cand_; 


  /// Current and previous linearization point and candidate
  std::vector<double> x_, x_old_, x_cand_, mu_cand_, s_, s_cand_, v_, xtmp_;
  
  /// Lagrange gradient in the next iterate
  std::vector<double> gLag_, gLag_old_, dualpen_;
  
  /// Constraint function value
  std::vector<double> gk_, QPgk_, gsk_,gk_cand_, gsk_cand_;
  
  /// Gradient of the objective function
  std::vector<double> gf_, QPgf_;

  /// BFGS update function
  enum BFGSMdoe{ BFGS_BK, BFGS_X, BFGS_X_OLD, BFGS_GLAG, BFGS_GLAG_OLD, BFGS_NUM_IN}; 
  FX bfgs_;
  
  /// Initial Hessian approximation (BFGS)
  DMatrix B_init_;
  
  /// Current Hessian approximation
  DMatrix Bk_, QPBk_;
  
  // Current Jacobian
  DMatrix Jk_, QPJk_;

  // Bounds of the QP
  std::vector<double> qp_LBA_, qp_UBA_, qp_LBX_, qp_UBX_;

  // QP solution
  std::vector<double> dx_, qp_DUAL_X_, qp_DUAL_A_, ds_, dy_, dv_;

  /// Regularization
  bool regularize_;

  // Storage for merit function
  std::deque<double> merit_mem_;

  /// Print iteration header
  void printIteration(std::ostream &stream);
  
  /// Print iteration
  void printIteration(std::ostream &stream, int iter, double obj, double pr_inf, double du_inf, 
                      double dx_norm, double reg, int ls_trials, bool ls_success);

  // Reset the Hessian or Hessian approximation
  void reset_h();

  // Evaluate the gradient of the objective
  virtual void eval_f(const std::vector<double>& x, double& f);
  
  // Evaluate the gradient of the objective
  virtual void eval_grad_f(const std::vector<double>& x, double& f, std::vector<double>& grad_f);
  
  // Evaluate the constraints
  virtual void eval_g(const std::vector<double>& x, std::vector<double>& g);

  // Evaluate the Jacobian of the constraints
  virtual void eval_jac_g(const std::vector<double>& x, std::vector<double>& g, Matrix<double>& J);

  // Evaluate the Hessian of the Lagrangian
  virtual void eval_h(const std::vector<double>& x, const std::vector<double>& lambda, double sigma, Matrix<double>& H);
  
  // Calculate the regularization parameter using Gershgorin theorem
  double getRegularization(const Matrix<double>& H);
  
  // Regularize by adding a multiple of the identity
  void regularize(Matrix<double>& H, double reg);
  
  // Solve the QP subproblem
  virtual void solve_QP(const Matrix<double>& H, const std::vector<double>& g,
                        const std::vector<double>& lbx, const std::vector<double>& ubx,
                        const Matrix<double>& A, const std::vector<double>& lbA, const std::vector<double>& ubA,
                        std::vector<double>& x_opt, std::vector<double>& lambda_x_opt, std::vector<double>& lambda_A_opt,
                        double muR, const std::vector<double> & mu, const std::vector<double> & muE);
  
  // Calculate the L1-norm of the primal infeasibility
  double primalInfeasibility(const std::vector<double>& x, const std::vector<double>& lbx, const std::vector<double>& ubx,
                             const std::vector<double>& g, const std::vector<double>& lbg, const std::vector<double>& ubg);
  
  /// Calculates inner_prod(x,mul(A,x))
  static double quad_form(const std::vector<double>& x, const DMatrix& A);
  
  /// Calculate inf norm of a vector
  double norminf(std::vector<double> v);
  double norm_2(std::vector<double> v);
  /// Calculate 1-norm of a matrix
  double norm1matrix(const DMatrix& A);
  
  /// Calculate the merit function gradient
  void meritfg();
  /// Matrix transpose and vector
  void mat_vectran(const std::vector<double>& x, const DMatrix& A, std::vector<double>& y);
  void mat_vec(const std::vector<double>& x, const DMatrix& A, std::vector<double>& y);
  
  /// Boolean to flag if QP should be stabilized manually
  bool stabilize_;

};
} // namespace CasADi

#endif //SQP_INTERNAL_HPP
