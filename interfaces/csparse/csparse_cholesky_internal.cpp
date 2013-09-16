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

#include "csparse_cholesky_internal.hpp"
#include "symbolic/matrix/matrix_tools.hpp"

using namespace std;
namespace CasADi{

  CSparseCholeskyInternal::CSparseCholeskyInternal(const CRSSparsity& sparsity, int nrhs)  : LinearSolverInternal(sparsity,nrhs){
    L_ = 0;
    S_ = 0;

    casadi_assert_message(sparsity==trans(sparsity),"CSparseCholeskyInternal: supplied sparsity must be symmetric, got " << sparsity.dimString() << ".");
  }

  CSparseCholeskyInternal::CSparseCholeskyInternal(const CSparseCholeskyInternal& linsol) : LinearSolverInternal(linsol){
    L_ = 0;
    S_ = 0;
  }

  CSparseCholeskyInternal::~CSparseCholeskyInternal(){
    if(S_) cs_sfree(S_);
    if(L_) cs_nfree(L_);
  }

  void CSparseCholeskyInternal::init(){
    // Call the init method of the base class
    LinearSolverInternal::init();

    AT_.nzmax = input().size();  // maximum number of entries 
    AT_.m = input().size2(); // number of rows
    AT_.n = input().size1(); // number of columns
    AT_.p = const_cast<int*>(&input().rowind().front()); // column pointers (size n+1) or col indices (size nzmax)
    AT_.i = const_cast<int*>(&input().col().front()); // row indices, size nzmax
    AT_.x = &input().front(); // row indices, size nzmax
    AT_.nz = -1; // of entries in triplet matrix, -1 for compressed-col 

    // Temporary
    temp_.resize(AT_.n);
  
    if(verbose()){
      cout << "CSparseCholeskyInternal::prepare: symbolic factorization" << endl;
    }

    // ordering and symbolic analysis 
    int order = 0; // ordering?
    if(S_) cs_sfree(S_);
    S_ = cs_schol (order, &AT_) ;
  }


  CRSSparsity CSparseCholeskyInternal::getFactorizationSparsity(bool transpose) const {
    casadi_assert(S_);
    int n = AT_.n;
    int nzmax = S_->cp[n];
    std::vector< int > col(n+1);
    std::copy(S_->cp,S_->cp+n+1,col.begin());
    std::vector< int > rowind(nzmax);
    int *Li = &rowind.front();
    int *Lp = &col.front();
    const cs* C;
    C = S_->pinv ? cs_symperm (&AT_, S_->pinv, 1) : &AT_;
    std::vector< int > temp(2*n);
    int *c = & temp.front();
    int *s = c+n;
    for (int k = 0 ; k < n ; k++) c [k] = S_->cp [k] ;
    for (int k = 0 ; k < n ; k++) {       /* compute L(k,:) for L*L' = C */
      int top = cs_ereach (C, k, S_->parent, s, c) ;
      for ( ; top < n ; top++)    /* solve L(0:k-1,0:k-1) * x = C(:,k) */
        {
          int i = s [top] ;               /* s [top..n-1] is pattern of L(k,:) */
          int p = c [i]++ ;
          Li [p] = k ;                /* store L(k,i) in column i */
        }
      int p = c [k]++ ;
      Li [p] = k ;    
    }
    Lp [n] = S_->cp [n] ; 
    CRSSparsity ret(n, n, rowind, col);

    return transpose? ret : trans(ret);
  
  }
  
  DMatrix CSparseCholeskyInternal::getFactorization(bool transpose) const {
    casadi_assert(L_);
    cs *L = L_->L;
    int nz = L->nzmax;
    int m = L->m; // number of rows
    int n = L->n; // number of columns
    std::vector< int > rowind(m+1);
    std::copy(L->p,L->p+m+1,rowind.begin());
    std::vector< int > col(nz);
    std::copy(L->i,L->i+nz,col.begin());
    std::vector< double > data(nz);
    std::copy(L->x,L->x+nz,data.begin());
    DMatrix ret(CRSSparsity(m, n, col, rowind),data); 
    
    return transpose? ret : trans(ret);
  }
  
  void CSparseCholeskyInternal::prepare(){

    prepared_ = false;
  
    // Get a reference to the nonzeros of the linear system
    const vector<double>& linsys_nz = input().data();
  
    // Make sure that all entries of the linear system are valid
    for(int k=0; k<linsys_nz.size(); ++k){
      casadi_assert_message(!isnan(linsys_nz[k]),"Nonzero " << k << " is not-a-number");
      casadi_assert_message(!isinf(linsys_nz[k]),"Nonzero " << k << " is infinite");
    }
  
    if(verbose()){
      cout << "CSparseCholeskyInternal::prepare: numeric factorization" << endl;
      cout << "linear system to be factorized = " << endl;
      input(0).printSparse();
    }
  
    if(L_) cs_nfree(L_);
    L_ = cs_chol(&AT_, S_) ;                 // numeric Cholesky factorization 
    if(L_==0){
      DMatrix temp = input();
      makeSparse(temp);
      if (isSingular(temp.sparsity())) {
        stringstream ss;
        ss << "CSparseCholeskyInternal::prepare: factorization failed due to matrix being singular. Matrix contains numerical zeros which are structurally non-zero. Promoting these zeros to be structural zeros, the matrix was found to be structurally rank deficient. sprank: " << rank(temp.sparsity()) << " <-> " << temp.size1() << endl;
        if(verbose()){
          ss << "Sparsity of the linear system: " << endl;
          input(LINSOL_A).sparsity().print(ss); // print detailed
        }
        throw CasadiException(ss.str());
      } else {
        stringstream ss;
        ss << "CSparseCholeskyInternal::prepare: factorization failed, check if Jacobian is singular" << endl;
        if(verbose()){
          ss << "Sparsity of the linear system: " << endl;
          input(LINSOL_A).sparsity().print(ss); // print detailed
        }
        throw CasadiException(ss.str());
      }
    }
    casadi_assert(L_!=0);

    prepared_ = true;
  }
  
  void CSparseCholeskyInternal::solve(double* x, int nrhs, bool transpose){
    casadi_assert(prepared_);
    casadi_assert(L_!=0);
    casadi_assert(transpose);
  
    double *t = &temp_.front();
  
    for(int k=0; k<nrhs; ++k){
      cs_ipvec (L_->pinv, x, t, AT_.n) ;   // t = P1\b
      cs_lsolve (L_->L, t) ;               // t = L\t 
      cs_ltsolve (L_->L, t) ;              // t = U\t 
      cs_ipvec (S_->q, t, x, AT_.n) ;      // x = P2\t 
      x += nrow();
    }
  }

  void CSparseCholeskyInternal::solveL(double* x, int nrhs, bool transpose){
    casadi_assert(prepared_);
    casadi_assert(L_!=0);
  
    double *t = getPtr(temp_);
  
    for(int k=0; k<nrhs; ++k){
      cs_ipvec (L_->pinv, x, t, AT_.n) ;   // t = P1\b
      if (transpose) cs_lsolve (L_->L, t) ; // t = L\t 
      if (!transpose) cs_ltsolve (L_->L, t) ; // t = U\t 
      cs_ipvec (S_->q, t, x, AT_.n) ;      // x = P2\t 
      x += nrow();
    }
  }

  CSparseCholeskyInternal* CSparseCholeskyInternal::clone() const{
    return new CSparseCholeskyInternal(input(LINSOL_A).sparsity(),input(LINSOL_B).size1());
  }

} // namespace CasADi
