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

#include "sqp_internal.hpp"
#include "symbolic/stl_vector_tools.hpp"
#include "symbolic/matrix/sparsity_tools.hpp"
#include "symbolic/matrix/matrix_tools.hpp"
#include "symbolic/fx/sx_function.hpp"
#include "symbolic/sx/sx_tools.hpp"
#include "symbolic/casadi_calculus.hpp"
#include <ctime>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <cfloat>

using namespace std;
namespace CasADi{

  SQPInternal::SQPInternal(const FX& nlp) : NLPSolverInternal(nlp){
    casadi_warning("The SQP method is under development");
    addOption("qp_solver",         OT_QPSOLVER,   GenericType(),    "The QP solver to be used by the SQP method");
    addOption("qp_solver_options", OT_DICTIONARY, GenericType(),    "Options to be passed to the QP solver");
    addOption("hessian_approximation", OT_STRING, "exact",          "limited-memory|exact");
    addOption("max_iter",           OT_INTEGER,      20,            "Maximum number of SQP iterations");
    addOption("max_iter_ls",        OT_INTEGER,       10,            "Maximum number of linesearch iterations");
    addOption("tol_pr",            OT_REAL,       1e-6,             "Stopping criterion for primal infeasibility");
    addOption("tol_du",            OT_REAL,       1e-6,             "Stopping criterion for dual infeasability");
    addOption("c1",                OT_REAL,       0.7,              "Armijo condition, coefficient of decrease in merit");
    addOption("beta",              OT_REAL,       0.8,              "Line-search parameter, restoration factor of stepsize");
    addOption("merit_memory",      OT_INTEGER,      4,              "Size of memory to store history of merit function values");
    addOption("lbfgs_memory",      OT_INTEGER,     10,              "Size of L-BFGS memory.");
    addOption("regularize",        OT_BOOLEAN,  false,              "Automatic regularization of Lagrange Hessian.");
    addOption("print_header",      OT_BOOLEAN,   true,              "Print the header with problem statistics");
    
    // Monitors
    addOption("monitor",      OT_STRINGVECTOR, GenericType(),  "", "eval_f|eval_g|eval_jac_g|eval_grad_f|eval_h|qp|dx", true);

    // New
    addOption("eps_active",        OT_REAL,      1e-6,              "Threshold for the epsilon-active set.");
    addOption("nu",                OT_REAL,      1,                 "Parameter for primal-dual augmented Lagrangian.");
    addOption("phiWeight",         OT_REAL,      1e-5,              "Weight used in pseudo-filter.");
    addOption("dvMax0",            OT_REAL,      100,               "Parameter used to defined the max step length.");
    addOption("tau0",              OT_REAL,      1e-2,              "Initial parameter for the merit function optimality threshold.");
    addOption("yEinitial",         OT_STRING,    "simple",          "Initial multiplier. Simple (all zero) or least (LSQ).");
    addOption("alphaMin",          OT_REAL,      1e-3,              "Used to check whether to increase rho.");
    addOption("sigmaMax",            OT_REAL,      1e+14,             "Maximum rho allowed.");
    addOption("muR0",              OT_REAL,      1e-4,              "Initial choice of regularization parameter");
  }


  SQPInternal::~SQPInternal(){
  }

  void SQPInternal::init(){
    // Call the init method of the base class
    NLPSolverInternal::init();
    
    // Read options
    max_iter_ = getOption("max_iter");
    max_iter_ls_ = getOption("max_iter_ls");
    c1_ = getOption("c1");
    beta_ = getOption("beta");
    merit_memsize_ = getOption("merit_memory");
    lbfgs_memory_ = getOption("lbfgs_memory");
    tol_pr_ = getOption("tol_pr");
    tol_du_ = getOption("tol_du");
    regularize_ = getOption("regularize");
    exact_hessian_ = getOption("hessian_approximation")=="exact";
    
    eps_active_ = getOption("eps_active");
    nu_ = getOption("nu");
    phiWeight_ = getOption("phiWeight");
    dvMax_ = getOption("dvMax0");
    tau_ = getOption("tau0");
    alphaMin_ = getOption("alphaMin");
    sigmaMax_ = getOption("sigmaMax");    
    muR_ = getOption("muR0");

 
    // Get/generate required functions
    gradF();
    jacG();
    if(exact_hessian_){
      hessLag();
    }
    
    // TODO: make some Check here
    stabilize_ = true;

    // Allocate a QP solver
    CRSSparsity H_sparsity = exact_hessian_ ? hessLag().output().sparsity() : sp_dense(nx_,nx_);
    H_sparsity = H_sparsity + DMatrix::eye(nx_).sparsity();
    CRSSparsity A_sparsity = jacG().isNull() ? CRSSparsity(0,nx_,false) : jacG().output().sparsity();
    
    CRSSparsity H_sparsity_qp = H_sparsity;
    CRSSparsity A_sparsity_qp = A_sparsity;
    
    if (stabilize_) { // Stabilized QP sparsity
      int nI = A_sparsity.size1();
      H_sparsity_qp = blkdiag(H_sparsity,sp_diag(nI));
      A_sparsity_qp = horzcat(A_sparsity,sp_diag(nI));
    }
    
    QPSolverCreator qp_solver_creator = getOption("qp_solver");
    qp_solver_ = qp_solver_creator(qpStruct("h",H_sparsity_qp,"a",A_sparsity_qp));

    // Set options if provided
    if(hasSetOption("qp_solver_options")){
      Dictionary qp_solver_options = getOption("qp_solver_options");
      qp_solver_.setOption(qp_solver_options);
    }
    qp_solver_.init();
  
    // Lagrange multipliers of the NLP
    mu_.resize(ng_);
    mu_cand_.resize(ng_);
    mu_x_.resize(nx_);
    mu_e_.resize(ng_);
    pi_.resize(ng_);
    pi2_.resize(ng_);
  
    // Lagrange gradient in the next iterate
    gLag_.resize(nx_);
    gLag_old_.resize(nx_);

    // Current linearization point
    x_.resize(nx_);
    x_cand_.resize(nx_);
    x_old_.resize(nx_);
    xtmp_.resize(nx_);

    // Constraint function value
    gk_.resize(ng_);
    gk_cand_.resize(ng_);
    QPgk_.resize(ng_);
    gsk_.resize(ng_);
    gsk_cand_.resize(ng_);
    s_.resize(ng_);
    s_cand_.resize(ng_);
    // Hessian approximation
    Bk_ = DMatrix(H_sparsity);
  
    //QPBk_ = DMatrix(Bk_);
    // Jacobian
    Jk_ = DMatrix(A_sparsity);

    //QPJk_ = DMatrix(QPJk_);

    // Bounds of the QP
    qp_LBA_.resize(ng_);
    qp_UBA_.resize(ng_);
    qp_LBX_.resize(nx_+ng_);
    qp_UBX_.resize(nx_+ng_);
    for (int i=0;i<ng_;++i) {
      qp_LBX_[i+nx_] = -numeric_limits<double>::infinity();
      qp_UBX_[i+nx_] = numeric_limits<double>::infinity();
    }
    // QP solution
    dx_.resize(nx_);
    qp_DUAL_X_.resize(nx_+ng_);
    qp_DUAL_A_.resize(ng_);

    ds_.resize(ng_);
    dy_.resize(ng_);
    dv_.resize(ng_+nx_);

    // Merit function vectors
    dualpen_.resize(ng_);
    gradm_.resize(ng_+nx_);
    gradms_.resize(ng_);

    // Gradient of the objective
    gf_.resize(nx_);
    QPgf_.resize(nx_+ng_);

    // Primal-dual variables
    v_.resize(nx_+ng_);
    

    // Create Hessian update function
    if(!exact_hessian_){
      // Create expressions corresponding to Bk, x, x_old, gLag and gLag_old
      SXMatrix Bk = ssym("Bk",H_sparsity);
      SXMatrix x = ssym("x",input(NLP_SOLVER_X0).sparsity());
      SXMatrix x_old = ssym("x",x.sparsity());
      SXMatrix gLag = ssym("gLag",x.sparsity());
      SXMatrix gLag_old = ssym("gLag_old",x.sparsity());
    
      SXMatrix sk = x - x_old;
      SXMatrix yk = gLag - gLag_old;
      SXMatrix qk = mul(Bk, sk);
    
      // Calculating theta
      SXMatrix skBksk = inner_prod(sk, qk);
      SXMatrix omega = if_else(inner_prod(yk, sk) < 0.2 * inner_prod(sk, qk),
                               0.8 * skBksk / (skBksk - inner_prod(sk, yk)),
                               1);
      yk = omega * yk + (1 - omega) * qk;
      SXMatrix theta = 1. / inner_prod(sk, yk);
      SXMatrix phi = 1. / inner_prod(qk, sk);
      SXMatrix Bk_new = Bk + theta * mul(yk, trans(yk)) - phi * mul(qk, trans(qk));
    
      // Inputs of the BFGS update function
      vector<SXMatrix> bfgs_in(BFGS_NUM_IN);
      bfgs_in[BFGS_BK] = Bk;
      bfgs_in[BFGS_X] = x;
      bfgs_in[BFGS_X_OLD] = x_old;
      bfgs_in[BFGS_GLAG] = gLag;
      bfgs_in[BFGS_GLAG_OLD] = gLag_old;
      bfgs_ = SXFunction(bfgs_in,Bk_new);
      bfgs_.setOption("number_of_fwd_dir",0);
      bfgs_.setOption("number_of_adj_dir",0);
      bfgs_.init();
    
      // Initial Hessian approximation
      B_init_ = DMatrix::eye(nx_);
    }
  
    // Header
    if(bool(getOption("print_header"))){
      cout << "-------------------------------------------" << endl;
      cout << "This is CasADi::SQPMethod." << endl;
      if(exact_hessian_){
        cout << "Using exact Hessian" << endl;
      } else {
        cout << "Using limited memory BFGS Hessian approximation" << endl;
      }
      cout << endl;
      cout << "Number of variables:                       " << setw(9) << nx_ << endl;
      cout << "Number of constraints:                     " << setw(9) << ng_ << endl;
      cout << "Number of nonzeros in constraint Jacobian: " << setw(9) << A_sparsity.size() << endl;
      cout << "Number of nonzeros in Lagrangian Hessian:  " << setw(9) << H_sparsity.size() << endl;
      cout << endl;
    }
  }

  void SQPInternal::evaluate(int nfdir, int nadir){
    casadi_assert(nfdir==0 && nadir==0);
  
    checkInitialBounds();
  
    // Get problem data
    const vector<double>& x_init = input(NLP_SOLVER_X0).data();
    const vector<double>& lbx = input(NLP_SOLVER_LBX).data();
    const vector<double>& ubx = input(NLP_SOLVER_UBX).data();
    const vector<double>& lbg = input(NLP_SOLVER_LBG).data();
    const vector<double>& ubg = input(NLP_SOLVER_UBG).data();
    //vector<double>& lbv, ubv;

    const vector<double>& lam_g0 = input(NLP_SOLVER_LAM_G0).data();
    
    vector<int> indicesn(nx_);
    vector<int> indicesm(ng_);    
    vector<int> emptyvec(0);
    for (int i=0;i<nx_;++i)
       indicesn[i] = i+nx_;
    for (int i=0;i<ng_;++i)
       indicesm[i] = i+ng_;

    //set primal-dual bounds
    //v.resize(nx_+ng_);
    //v.resize(nx_+ng_);
    //copy(lbx.begin(),lbx.end(),lbv.begin());
    

    // Set linearization point to initial guess
    copy(x_init.begin(),x_init.end(),x_.begin());
    for (int i=0;i<nx_;++i) {
      x_[i] = std::max(x_[i],lbx[i]);
      x_[i] = std::min(x_[i],ubx[i]);
    }


    // Initialize Lagrange multipliers of the NLP
    copy(input(NLP_SOLVER_LAM_G0).begin(),input(NLP_SOLVER_LAM_G0).end(),mu_.begin());
    copy(input(NLP_SOLVER_LAM_G0).begin(),input(NLP_SOLVER_LAM_G0).end(),mu_e_.begin());
    copy(input(NLP_SOLVER_LAM_X0).begin(),input(NLP_SOLVER_LAM_X0).end(),mu_x_.begin());


    // Initial constraint Jacobian
    eval_jac_g(x_,gk_,Jk_);


    // Initial objective gradient
    eval_grad_f(x_,fk_,gf_);
  
    normgf_ = norm_2(gf_);



    // Initialize or reset the Hessian or Hessian approximation
    reg_ = 0;
    if(exact_hessian_){
      eval_h(x_,mu_,1.0,Bk_);
    } else {
      reset_h();
    }


    // Evaluate the initial gradient of the Lagrangian
    copy(gf_.begin(),gf_.end(),gLag_.begin());
    if(ng_>0) DMatrix::mul_no_alloc_tn(Jk_,mu_,gLag_);
    // gLag += mu_x_;
    transform(gLag_.begin(),gLag_.end(),mu_x_.begin(),gLag_.begin(),plus<double>());

    // Number of SQP iterations
    int iter = 0;

    // Number of line-search iterations
    int ls_iter = 0;
  
    // Last linesearch successfull
    bool ls_success = true;
  
    // Reset
    merit_mem_.clear();
    sigma_ = 1.;







    // MAIN OPTIMIZATION LOOP
    while(true){
    
      // Primal infeasability
      double pr_inf = primalInfeasibility(x_, lbx, ubx, gk_, lbg, ubg);
    
      // 1-norm of lagrange gradient
      double gLag_norm1 = norm_1(gLag_);
    
      // 1-norm of step
      double dx_norm1 = norm_1(dx_);
    
      // Print header occasionally
      if(iter % 10 == 0) printIteration(cout);
    
      // Printing information about the actual iterate
      printIteration(cout,iter,fk_,pr_inf,gLag_norm1,dx_norm1,reg_,ls_iter,ls_success);
    
      // Call callback function if present
      if (!callback_.isNull()) {
        if (!callback_.input(NLP_SOLVER_F).empty()) callback_.input(NLP_SOLVER_F).set(fk_);
        if (!callback_.input(NLP_SOLVER_X).empty()) callback_.input(NLP_SOLVER_X).set(x_);
        if (!callback_.input(NLP_SOLVER_LAM_G).empty()) callback_.input(NLP_SOLVER_LAM_G).set(mu_);
        if (!callback_.input(NLP_SOLVER_LAM_X).empty()) callback_.input(NLP_SOLVER_LAM_X).set(mu_x_);
        if (!callback_.input(NLP_SOLVER_G).empty()) callback_.input(NLP_SOLVER_G).set(gk_);
        callback_.evaluate();
      
        if (callback_.output(0).at(0)) {
          cout << endl;
          cout << "CasADi::SQPMethod: aborted by callback..." << endl;
          break;
        }
      }
      normJ_ = norm1matrix(Jk_);
     // Default stepsize
      double t = 0;

      for (int i=0;i<ng_;++i)
        s_[i] = std::min(gk_[i]+mu_e_[i]*muR_,ubg[i]);
      for (int i=0;i<ng_;++i)
        s_[i] = std::max(s_[i],lbg[i]);
      for (int i=0;i<ng_;++i)
        gsk_[i] = gk_[i]-s_[i];    
  

      normc_ = norm_2(gk_);
      normcs_ = norm_2(gsk_);

      scaleg_ = 1+normc_*normJ_;
      scaleglag_ = std::max(1., std::max(normgf_,(std::max(1.,norm_2(mu_)) * normJ_)));

      

      // Checking convergence criteria
      if (pr_inf/scaleg_ < tol_pr_ && gLag_norm1/scaleglag_ < tol_du_){
        cout << endl;
        cout << "CasADi::SQPMethod: Convergence achieved after " << iter << " iterations." << endl;
        break;
      }

      if (iter==0) {
	phiMaxO_ = std::max(gLag_norm1+pr_inf+10.,1000.);
        phiMaxV_ = phiMaxO_;
        transform(mu_.begin(),mu_.end(),mu_e_.begin(),dualpen_.begin(),minus<double>());
        for (int i=0;i<ng_;++i)
	  dualpen_[i] = dualpen_[i]*(1/sigma_);
        transform(gsk_.begin(),gsk_.end(),dualpen_.begin(),dualpen_.begin(),plus<double>());
        Merit_ = fk_+inner_prod(mu_e_,gk_)+.5*sigma_*(normcs_*normcs_+nu_*norm_2(dualpen_));

      }

      if (iter >= max_iter_){
        cout << endl;
        cout << "CasADi::SQPMethod: Maximum number of iterations reached." << endl;
        break;
      }
    
      // Start a new iteration
      iter++;
    

      // Formulate the QP
      transform(lbx.begin(),lbx.end(),x_.begin(),qp_LBX_.begin(),minus<double>());
      transform(ubx.begin(),ubx.end(),x_.begin(),qp_UBX_.begin(),minus<double>());
      transform(lbg.begin(),lbg.end(),gk_.begin(),qp_LBA_.begin(),minus<double>());
      transform(ubg.begin(),ubg.end(),gk_.begin(),qp_UBA_.begin(),minus<double>());

      // Solve the QP
      solve_QP(Bk_,gf_,qp_LBX_,qp_UBX_,Jk_,qp_LBA_,qp_UBA_,dx_,qp_DUAL_X_,qp_DUAL_A_,muR_,mu_,mu_e_);

      /**
      for (int i=0;i<nx_;++i)
         dx_[i] = dv_[i];
      for (int i=0;i<ng_;++i) 
         dy_[i] = dv_[i+nx_];
         
      **/

      log("QP solved");



      

      // Detecting indefiniteness
      double gain = quad_form(dx_,Bk_);
      if (gain < 0){
        casadi_warning("Indefinite Hessian detected...");
      }

      mat_vec(dx_,Jk_,ds_);
      transform(ds_.begin(),ds_.end(),gsk_.begin(),ds_.begin(),plus<double>());      


      double muhat;
      //make sure, if nu=0 (so using classical augLag) that muR is small enough
      if (nu_==0) {
        mat_vectran(mu_e_,Jk_,xtmp_);
        transform(xtmp_.begin(),xtmp_.end(),gk_.begin(),xtmp_.begin(),plus<double>());
        muhat = inner_prod(xtmp_,dx_)-inner_prod(mu_e_,ds_)+.5*gain;
        muhat = inner_prod(gsk_,gsk_)/abs(muhat);
        transform(qp_DUAL_A_.begin(),qp_DUAL_A_.end(),mu_e_.begin(),pi2_.begin(),minus<double>());
        muhat = std::min(muhat,norm_2(gsk_)/(2*norm_2(pi2_)));
        if (muR_>muhat) 
	  muR_ = muhat;
      }     


      // Calculate line search values
      meritfg();         

 
      //EDIT: now calculate grads and inner product with both
      double rhsmerit = 0;
      for (int i=0;i<nx_;++i)
           rhsmerit = rhsmerit+ dx_[i]*gradm_[i];
      
      for (int i=0;i<ng_;++i)
	rhsmerit = rhsmerit+(dy_[i])*gradm_[i+ng_]+ds_[i]*gradms_[i];
      
      if (nu_= 0 && rhsmerit > 0) {
	for (int i=0;i<ng_;++i) 
	  rhsmerit = rhsmerit+gsk_[i]*(dy_[i]);
      }


      // Calculate L1-merit function in the actual iterate
      //      double l1_infeas = primalInfeasibility(x_, lbx, ubx, gk_, lbg, ubg);

      // Right-hand side of Armijo condition
      //double F_sens = inner_prod(dx_, gf_);    
      //double L1dir = F_sens - sigma_ * l1_infeas;
      //double L1merit = fk_ + sigma_ * l1_infeas;

      // Storing the actual merit function value in a list
      //merit_mem_.push_back(L1merit);
      //if (merit_mem_.size() > merit_memsize_){
      //  merit_mem_.pop_front();
      //}
      // Stepsize
      t = 1.0;
      //double fk_cand;
      // Merit function value in candidate
      double merit_cand = 0;

      // Reset line-search counter, success marker
      ls_iter = 0;
      ls_success = true;

      // Line-search
      log("Starting line-search");

      if(max_iter_ls_>0){ // max_iter_ls_== 0 disables line-search
      
        // Line-search loop
        while (true){
          for(int i=0; i<nx_; ++i) {
            x_cand_[i] = x_[i] + t * dx_[i];
            mu_cand_[i] = mu_[i]+t*(dy_[i]);
            s_cand_[i] = s_[i]+t*ds_[i];
	  }
          // Evaluating objective and constraints
          eval_f(x_cand_,fk_cand_);
          eval_g(x_cand_,gk_cand_);
         
          for(int i=0;i<ng_;++i) {
	    gsk_cand_[i] = gk_cand_[i]-s_cand_[i];
	  }
         

          ls_iter++;

          // Calculating merit-function in candidate

          normc_cand_ = norm_2(gk_cand_);
          normcs_cand_ = norm_2(gsk_cand_);

          transform(mu_cand_.begin(),mu_cand_.end(),mu_e_.begin(),dualpen_.begin(),minus<double>());
          for (int i=0;i<ng_;++i) 
       	    dualpen_[i] = dualpen_[i]*(1/sigma_);
          transform(gsk_cand_.begin(),gsk_cand_.end(),dualpen_.begin(),dualpen_.begin(),plus<double>());
          Merit_cand_ = fk_cand_+inner_prod(mu_e_,gk_cand_)+.5*sigma_*(normcs_cand_*normcs_cand_+nu_*norm_2(dualpen_));


        
          // Calculating maximal merit function value so far          
          if (Merit_cand_ <= Merit_ + c1_*t * rhsmerit){
            // Accepting candidate
            log("Line-search completed, candidate accepted");
            break;
          }

     
          // Line-search not successful, but we accept it.
          //do mu merit comparison as per flexible penalty
          if(ls_iter == 1) {
            transform(mu_.begin(),mu_.end(),mu_e_.begin(),dualpen_.begin(),minus<double>());
            for (int i=0;i<ng_;++i) 
       	      dualpen_[i] = dualpen_[i]*(muR_);
            transform(gsk_.begin(),gsk_.end(),dualpen_.begin(),dualpen_.begin(),plus<double>());
            Merit_mu_ = fk_+inner_prod(mu_e_,gk_)+.5*(1/muR_)*(normcs_*normcs_+nu_*norm_2(dualpen_));
	  }
          transform(mu_cand_.begin(),mu_cand_.end(),mu_e_.begin(),dualpen_.begin(),minus<double>());
          for (int i=0;i<ng_;++i) 
       	    dualpen_[i] = dualpen_[i]*(muR_);
          transform(gsk_cand_.begin(),gsk_cand_.end(),dualpen_.begin(),dualpen_.begin(),plus<double>());
          Merit_mu_cand_ = fk_cand_+inner_prod(mu_e_,gk_cand_)+.5*(1/muR_)*(normcs_cand_*normcs_cand_+nu_*norm_2(dualpen_));
          if (Merit_mu_cand_ <= Merit_mu_+c1_*t*rhsmerit) {
	    sigma_ = std::min(std::min(2*sigma_,1/muR_),sigmaMax_);
            break;
	  }
          
          if(ls_iter == max_iter_ls_){
            ls_success = false;
            log("Line-search completed, maximum number of iterations");
            break;
          }
      
          // Backtracking
          t = beta_ * t;
        }
      }

      // Candidate accepted, update dual variables
      for(int i=0; i<ng_; ++i) mu_[i] = t * dy_[i] + mu_[i];
      for(int i=0; i<nx_; ++i) mu_x_[i] = t * qp_DUAL_X_[i] + (1 - t) * mu_x_[i];
    
      if(!exact_hessian_){
        // Evaluate the gradient of the Lagrangian with the old x but new mu (for BFGS)
        copy(gf_.begin(),gf_.end(),gLag_old_.begin());
        if(ng_>0) DMatrix::mul_no_alloc_tn(Jk_,mu_,gLag_old_);
        // gLag_old += mu_x_;
        transform(gLag_old_.begin(),gLag_old_.end(),mu_x_.begin(),gLag_old_.begin(),plus<double>());
      }
    
      // Candidate accepted, update the primal variable
      copy(x_.begin(),x_.end(),x_old_.begin());
      copy(x_cand_.begin(),x_cand_.end(),x_.begin());
      Merit_ = Merit_cand_;
      
      // Change this later
      copy(mu_.begin(),mu_.end(),mu_e_.begin());

      // Evaluate the constraint Jacobian
      log("Evaluating jac_g");
      eval_jac_g(x_,gk_,Jk_);
    
      // Evaluate the gradient of the objective function
      log("Evaluating grad_f");
      eval_grad_f(x_,fk_,gf_);
    
      // Evaluate the gradient of the Lagrangian with the new x and new mu
      copy(gf_.begin(),gf_.end(),gLag_.begin());
      if(ng_>0) DMatrix::mul_no_alloc_tn(Jk_,mu_,gLag_);
      // gLag += mu_x_;
      transform(gLag_.begin(),gLag_.end(),mu_x_.begin(),gLag_.begin(),plus<double>());

      // Updating Lagrange Hessian
      if( !exact_hessian_){
        log("Updating Hessian (BFGS)");
        // BFGS with careful updates and restarts
        if (iter % lbfgs_memory_ == 0){
          // Reset Hessian approximation by dropping all off-diagonal entries
          const vector<int>& rowind = Bk_.rowind();      // Access sparsity (row offset)
          const vector<int>& col = Bk_.col();            // Access sparsity (column)
          vector<double>& data = Bk_.data();             // Access nonzero elements
          for(int i=0; i<rowind.size()-1; ++i){          // Loop over the rows of the Hessian
            for(int el=rowind[i]; el<rowind[i+1]; ++el){ // Loop over the nonzero elements of the row
              if(i!=col[el]) data[el] = 0;               // Remove if off-diagonal entries
            }
          }
        }
      
        // Pass to BFGS update function
        bfgs_.setInput(Bk_,BFGS_BK);
        bfgs_.setInput(x_,BFGS_X);
        bfgs_.setInput(x_old_,BFGS_X_OLD);
        bfgs_.setInput(gLag_,BFGS_GLAG);
        bfgs_.setInput(gLag_old_,BFGS_GLAG_OLD);
      
        // Update the Hessian approximation
        bfgs_.evaluate();
      
        // Get the updated Hessian
        bfgs_.getOutput(Bk_);
      } else {
        // Exact Hessian
        log("Evaluating hessian");
        eval_h(x_,mu_,1.0,Bk_);
      }

   for (int i=0;i<ng_;++i)
      gsk_[i] = gk_[i]-s_[i];    


    normc_ = norm_2(gk_);
    normcs_ = norm_2(gsk_);
     
    }
  
    // Save results to outputs
    output(NLP_SOLVER_F).set(fk_);
    output(NLP_SOLVER_X).set(x_);
    output(NLP_SOLVER_LAM_G).set(mu_);
    output(NLP_SOLVER_LAM_X).set(mu_x_);
    output(NLP_SOLVER_G).set(gk_);
  
    // Save statistics
    stats_["iter_count"] = iter;
  }
  
  void SQPInternal::printIteration(std::ostream &stream){
    stream << setw(4)  << "iter";
    stream << setw(14) << "objective";
    stream << setw(9) << "inf_pr";
    stream << setw(9) << "inf_du";
    stream << setw(9) << "||d||";
    stream << setw(7) << "lg(rg)";
    stream << setw(3) << "ls";
    stream << ' ';
    stream << endl;
  }
  
  void SQPInternal::printIteration(std::ostream &stream, int iter, double obj, double pr_inf, double du_inf, 
                                   double dx_norm, double rg, int ls_trials, bool ls_success){
    stream << setw(4) << iter;
    stream << scientific;
    stream << setw(14) << setprecision(6) << obj;
    stream << setw(9) << setprecision(2) << pr_inf;
    stream << setw(9) << setprecision(2) << du_inf;
    stream << setw(9) << setprecision(2) << dx_norm;
    stream << fixed;
    if(rg>0){
      stream << setw(7) << setprecision(2) << log10(rg);
    } else {
      stream << setw(7) << "-";
    }
    stream << setw(3) << ls_trials;
    stream << (ls_success ? ' ' : 'F');
    stream << endl;
  }

  double SQPInternal::quad_form(const std::vector<double>& x, const DMatrix& A){
    // Assert dimensions
    casadi_assert(x.size()==A.size1() && x.size()==A.size2());
  
    // Access the internal data of A
    const std::vector<int> &A_rowind = A.rowind();
    const std::vector<int> &A_col = A.col();
    const std::vector<double> &A_data = A.data();
  
    // Return value
    double ret=0;

    // Loop over the rows of A
    for(int i=0; i<x.size(); ++i){
      // Loop over the nonzeros of A
      for(int el=A_rowind[i]; el<A_rowind[i+1]; ++el){
        // Get column
        int j = A_col[el];
      
        // Add contribution
        ret += x[i]*A_data[el]*x[j];
      }
    }
  
    return ret;
  }

  void SQPInternal::reset_h(){
    // Initial Hessian approximation of BFGS
    if ( !exact_hessian_){
      Bk_.set(B_init_);
    }

    if (monitored("eval_h")) {
      cout << "x = " << x_ << endl;
      cout << "H = " << endl;
      Bk_.printSparse();
    }
  }

  double SQPInternal::getRegularization(const Matrix<double>& H){
    const vector<int>& rowind = H.rowind();
    const vector<int>& col = H.col();
    const vector<double>& data = H.data();
    double reg_param = 0;
    for(int i=0; i<rowind.size()-1; ++i){
      double mineig = 0;
      for(int el=rowind[i]; el<rowind[i+1]; ++el){
        int j = col[el];
        if(i == j){
          mineig += data[el];
        } else {
          mineig -= fabs(data[el]);
        }
      }
      reg_param = fmin(reg_param,mineig);
    }
    return -reg_param;
  }
  
  void SQPInternal::regularize(Matrix<double>& H, double reg){
    const vector<int>& rowind = H.rowind();
    const vector<int>& col = H.col();
    vector<double>& data = H.data();
    
    for(int i=0; i<rowind.size()-1; ++i){
      for(int el=rowind[i]; el<rowind[i+1]; ++el){
        int j = col[el];
        if(i==j){
          data[el] += reg;
        }
      }
    }
  }

  
  void SQPInternal::eval_h(const std::vector<double>& x, const std::vector<double>& lambda, double sigma, Matrix<double>& H){
    try{
      // Get function
      FX& hessLag = this->hessLag();

      // Pass the argument to the function
      hessLag.setInput(x,HESSLAG_X);
      hessLag.setInput(input(NLP_SOLVER_P),HESSLAG_P);
      hessLag.setInput(sigma,HESSLAG_LAM_F);
      hessLag.setInput(lambda,HESSLAG_LAM_G);
      
      // Evaluate
      hessLag.evaluate();
      
      // Get results
      hessLag.getOutput(H);
      
      if (monitored("eval_h")) {
        cout << "x = " << x << endl;
        cout << "H = " << endl;
        H.printSparse();
      }

      // Determing regularization parameter with Gershgorin theorem
      if(regularize_){
        reg_ = getRegularization(H);
        if(reg_ > 0){
          regularize(H,reg_);
        }
      }

    } catch (exception& ex){
      cerr << "eval_h failed: " << ex.what() << endl;
      throw;
    }
  }

  void SQPInternal::eval_g(const std::vector<double>& x, std::vector<double>& g){
    try {
      
      // Quick return if no constraints
      if(ng_==0) return;
      
      // Pass the argument to the function
      nlp_.setInput(x,NL_X);
      nlp_.setInput(input(NLP_SOLVER_P),NL_P);
      
      // Evaluate the function and tape
      nlp_.evaluate();
      
      // Ge the result
      nlp_.output(NL_G).get(g,DENSE);
      
      // Printing
      if(monitored("eval_g")){
        cout << "x = " << nlp_.input(NL_X) << endl;
        cout << "g = " << nlp_.output(NL_G) << endl;
      }
    } catch (exception& ex){
      cerr << "eval_g failed: " << ex.what() << endl;
      throw;
    }
  }

  void SQPInternal::eval_jac_g(const std::vector<double>& x, std::vector<double>& g, Matrix<double>& J){
    try{
      // Quich finish if no constraints
      if(ng_==0) return;
    
      // Get function
      FX& jacG = this->jacG();

      // Pass the argument to the function
      jacG.setInput(x,NL_X);
      jacG.setInput(input(NLP_SOLVER_P),NL_P);
      
      // Evaluate the function
      jacG.evaluate();
      
      // Get the output
      jacG.output(1+NL_G).get(g,DENSE);
      jacG.output().get(J);

      if (monitored("eval_jac_g")) {
        cout << "x = " << x << endl;
        cout << "g = " << g << endl;
        cout << "J = " << endl;
        J.printSparse();
      }
    } catch (exception& ex){
      cerr << "eval_jac_g failed: " << ex.what() << endl;
      throw;
    }
  }

  void SQPInternal::eval_grad_f(const std::vector<double>& x, double& f, std::vector<double>& grad_f){
    try {
      // Get function
      FX& gradF = this->gradF();

      // Pass the argument to the function
      gradF.setInput(x,NL_X);
      gradF.setInput(input(NLP_SOLVER_P),NL_P);
      
      // Evaluate, adjoint mode
      gradF.evaluate();
      
      // Get the result
      gradF.output().get(grad_f,DENSE);
      gradF.output(1+NL_X).get(f);
      
      // Printing
      if (monitored("eval_f")){
        cout << "x = " << x << endl;
        cout << "f = " << f << endl;
      }
      
      if (monitored("eval_grad_f")) {
        cout << "x      = " << x << endl;
        cout << "grad_f = " << grad_f << endl;
      }
    } catch (exception& ex){
      cerr << "eval_grad_f failed: " << ex.what() << endl;
      throw;
    }
  }
  
  void SQPInternal::eval_f(const std::vector<double>& x, double& f){
    try {
      // Pass the argument to the function
      nlp_.setInput(x,NL_X);
      nlp_.setInput(input(NLP_SOLVER_P),NL_P);
      
      // Evaluate the function
      nlp_.evaluate();

      // Get the result
      nlp_.getOutput(f,NL_F);

      // Printing
      if(monitored("eval_f")){
        cout << "x = " << nlp_.input(NL_X) << endl;
        cout << "f = " << f << endl;
      }
    } catch (exception& ex){
      cerr << "eval_f failed: " << ex.what() << endl;
      throw;
    }
  }
  
  void SQPInternal::solve_QP(const Matrix<double>& H, const std::vector<double>& g,
                             const std::vector<double>& lbx, const std::vector<double>& ubx,
                             const Matrix<double>& A, const std::vector<double>& lbA, const std::vector<double>& ubA,
                             std::vector<double>& x_opt, std::vector<double>& lambda_x_opt, std::vector<double>& lambda_A_opt,
                             double muR, const std::vector<double> & mu, const std::vector<double> & muE){

      cout << "H = " << endl;
      H.printDense();
      cout << "A = " << endl;
      A.printDense();
      cout << "g = " << g << endl;
      cout << "lbx = " << lbx << endl;
      cout << "ubx = " << ubx << endl;
      cout << "lbA = " << lbA << endl;
      cout << "ubA = " << ubA << endl;
    //}
    
    
    // Hessian
    if (stabilize_) {
      // Construct stabilized H
      DMatrix & H_qp = qp_solver_.input(QP_SOLVER_H);
      std::copy(H.begin(),H.end(),H_qp.begin());
      std::fill(H_qp.begin()+H.size(),H_qp.end(),muR);
    } else {
      qp_solver_.setInput(H, QP_SOLVER_H);
    }
    std::cout << "orig H";
    H.printDense();
    std::cout << std::endl;
    
    std::cout << "new :";
    qp_solver_.input(QP_SOLVER_H).printDense();
    std::cout << std::endl;

    // Pass linear bounds
    if (ng_>0) {
      qp_solver_.setInput(lbA,QP_SOLVER_LBA);
      qp_solver_.setInput(ubA,QP_SOLVER_UBA);
      
      if (stabilize_) {
        DMatrix & A_qp = qp_solver_.input(QP_SOLVER_A);
        const std::vector <int> &A_rowind = A.rowind();
        for (int i=0;i<A_qp.size1();++i) { // Loop over rows
          int row_start = A_rowind[i];
          int row_end = A_rowind[i+1];
          // Copy row contents
          std::copy(A.begin()+row_start,A.begin()+row_end,A_qp.begin()+row_start+i);
          A_qp[row_end+i] = muR;
        }
        // Add constant to linear inequality 
        for (int i=0;i<mu.size();++i) {
          double extra = muR*(mu[i]-muE[i]);
          qp_solver_.input(QP_SOLVER_LBA).at(i)+= extra;
          qp_solver_.input(QP_SOLVER_UBA).at(i)+= extra;
        }
      } else {
        qp_solver_.setInput(A, QP_SOLVER_A);
      }

    }
    
    std::cout << "orig A";
    A.printDense();
    std::cout << std::endl;
    
    std::cout << "new :";
    qp_solver_.input(QP_SOLVER_A).printDense();
    std::cout << std::endl;
    
    // g
    if (stabilize_) {
      std::copy(g.begin(),g.end(),qp_solver_.input(QP_SOLVER_G).begin());
      for (int i=0;i<mu.size();++i) {
        qp_solver_.input(QP_SOLVER_G).at(g.size()+i) = muR * mu[i];
      }
    } else {
      qp_solver_.setInput(g,QP_SOLVER_G);
    }

    std::cout << "orig g" << g <<  std::endl;
    std::cout << "new :" << qp_solver_.input(QP_SOLVER_G) << std::endl;

    std::cout << "orig lbA" << lbA <<  std::endl;
    std::cout << "new :" << qp_solver_.input(QP_SOLVER_LBA) << std::endl;
    
    std::cout << "orig ubA" << ubA <<  std::endl;
    std::cout << "new :" << qp_solver_.input(QP_SOLVER_UBA) << std::endl;

    // Hot-starting if possible
    std::copy(x_opt.begin(),x_opt.end(),qp_solver_.input(QP_SOLVER_X0).begin());
  
    if (monitored("qp")) {
      cout << "H = " << endl;
      H.printDense();
      cout << "A = " << endl;
      A.printDense();
      cout << "g = " << g << endl;
      cout << "lbx = " << lbx << endl;
      cout << "ubx = " << ubx << endl;
      cout << "lbA = " << lbA << endl;
      cout << "ubA = " << ubA << endl;
    }

    // Solve the QP
    qp_solver_.evaluate();
  
    // Get the optimal solution
    std::copy(qp_solver_.output(QP_SOLVER_X).begin(),qp_solver_.output(QP_SOLVER_X).begin()+nx_,x_opt.begin());
    std::copy(qp_solver_.output(QP_SOLVER_LAM_X).begin(),qp_solver_.output(QP_SOLVER_LAM_X).begin()+nx_,lambda_x_opt.begin());
    std::copy(qp_solver_.output(QP_SOLVER_LAM_A).begin(),qp_solver_.output(QP_SOLVER_LAM_A).begin()+ng_,lambda_A_opt.begin());
    
    if (monitored("dx")){
      cout << "dx = " << x_opt << endl;
    }
  }
  
  double SQPInternal::primalInfeasibility(const std::vector<double>& x, const std::vector<double>& lbx, const std::vector<double>& ubx,
                                          const std::vector<double>& g, const std::vector<double>& lbg, const std::vector<double>& ubg){
    // L1-norm of the primal infeasibility
    double pr_inf = 0;
  
    // Bound constraints
    for(int j=0; j<x.size(); ++j){
      pr_inf += max(0., lbx[j] - x[j]);
      pr_inf += max(0., x[j] - ubx[j]);
    }
  
    // Nonlinear constraints
    for(int j=0; j<g.size(); ++j){
      pr_inf += max(0., lbg[j] - g[j]);
      pr_inf += max(0., g[j] - ubg[j]);
    }
  
    return pr_inf;
  }  

  double SQPInternal::norm1matrix(const DMatrix& A) {
    // Access the arrays
    const std::vector<double>& v = A.data();
    const std::vector<int>& col = A.col();
    std::vector<double> sums(A.size2(),0);
    for (int i=0;i<A.size();i++)
      sums[col[i]] += abs(v[i]);
    
    return SQPInternal::norminf(sums);        
    
  }

  double SQPInternal::norminf(std::vector<double> v) {
    double max = 0;

    for (int i=0;i<v.size();i++) {
      if (v[i]> max)
	max = v[i];
    }
    return max;

  }
  double SQPInternal::norm_2(std::vector<double> v) {
    double val=0;
    for (int i=0;i<v.size();i++) {
      val+= v[i]*v[i];
    }
    val = sqrt(val);
    return val;

  }


  void SQPInternal::mat_vectran(const std::vector<double>& x, const DMatrix& A, std::vector<double>& y){
    // Assert dimensions
    casadi_assert(x.size()==A.size1() && y.size()==A.size2());
  
    // Access the internal data of A
    const std::vector<int> &A_rowind = A.rowind();
    const std::vector<int> &A_col = A.col();
    const std::vector<double> &A_data = A.data();
    
    
    for (int i=0;i<y.size();++i)
      y[i] = 0;
    // Loop over the rows of A
    for(int i=0; i<A.size1(); ++i){
      // Loop over the nonzeros of A
      for(int el=A_rowind[i]; el<A_rowind[i+1]; ++el){
        // Get column
        int j = A_col[el];
      
        // Add contribution
        y[j] += A_data[el]*x[i];
      }
    }
  }

  void SQPInternal::mat_vec(const std::vector<double>& x, const DMatrix& A, std::vector<double>& y){
    // Assert dimensions
    casadi_assert(x.size()==A.size2() && y.size()==A.size1());
  
    // Access the internal data of A
    const std::vector<int> &A_rowind = A.rowind();
    const std::vector<int> &A_col = A.col();
    const std::vector<double> &A_data = A.data();
    
    
    for (int i=0;i<y.size();++i)
      y[i] = 0;
    // Loop over the rows of A
    for(int i=0; i<A.size1(); ++i){
      // Loop over the nonzeros of A
      for(int el=A_rowind[i]; el<A_rowind[i+1]; ++el){
        // Get column
        int j = A_col[el];
      
        // Add contribution
        y[i] += A_data[el]*x[j];
      }
    }
  }

  void SQPInternal::meritfg() {
    for (int i=0;i<ng_;++i)
      pi_[i] = 1/muR_*gsk_[i];
    transform(mu_e_.begin(),mu_e_.end(),pi_.begin(),pi_.begin(),plus<double>());
    transform(pi_.begin(),pi_.end(),mu_.begin(),pi_.begin(),minus<double>());
    for (int i=0;i<ng_;++i) { 
      pi_[i] = nu_*pi_[i];
      pi2_[i] = gsk_[i]/muR_;
    }
    transform(pi_.begin(),pi_.end(),mu_e_.begin(),pi_.begin(),plus<double>());
    transform(pi_.begin(),pi_.end(),pi2_.begin(),pi_.begin(),plus<double>());
    copy(gf_.begin(),gf_.end(),gradm_.begin());
    mat_vectran(pi_,Jk_,xtmp_);
    transform(xtmp_.begin(),xtmp_.end(),gradm_.begin(),gradm_.begin(),plus<double>());
    for (int i=0;i<ng_;++i) { 
      gradm_[nx_+i] = nu_*(gsk_[i]+muR_*(mu_[i]-mu_e_[i]));
      gradms_[i] = -(mu_e_[i]+(1+nu_)*gsk_[i]/muR_+nu_*(mu_[i]-mu_e_[i]));

    }

    
  }
} // namespace CasADi
