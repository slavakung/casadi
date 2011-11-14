#ifdef WITH_SWIG_SPLIT
%module casadi_primitive_tools

%include "common.i"

%import "casadi_primitive.i"
#endif //WITH_SWIG_SPLIT

#ifdef SWIGPYTHON
%pythoncode%{
try:
  from numpy import sin, cos, tan, sqrt, log, exp, floor, ceil, fmin, fmax, sinh, cosh, tanh
  from numpy import arcsin as asin
  from numpy import arccos as acos
  from numpy import arctan as atan
except:
  sin = lambda x: x.sin()
  cos = lambda x: x.cos()
  tan = lambda x: x.tan()
  asin = lambda x: x.asin()
  acos = lambda x: x.acos()
  atan = lambda x: x.atan()
  sqrt = lambda x: x.sqrt()
  log = lambda x: x.log()
  exp = lambda x: x.exp()
  floor = lambda x: x.floor()
  ceil = lambda x: x.ceil()
  fmin = lambda x,y: x.fmin(y)
  fmax = lambda x,y: x.fmax(y)
  sinh = lambda x: x.asin()
  cosh = lambda x: x.acos()
  tanh = lambda x: x.atan()
%}
#endif // SWIGPYTHON

// Matrix tools
%include "casadi/matrix/matrix_tools.hpp"

// Instsantiate the functions
MATRIX_TOOLS_TEMPLATES(double)
MATRIX_TOOLS_TEMPLATES(CasADi::SX)

// Sparsity tools
%{
#include "casadi/matrix/sparsity_tools.hpp"
%}
%include "casadi/matrix/sparsity_tools.hpp"

// SX tools
%include "casadi/sx/sx_tools.hpp"

#ifdef SWIGPYTHON
%{
template<> swig_type_info** meta< std::pair< CasADi::MX, std::vector< CasADi::MX> > >::name = &SWIGTYPE_p_std__pairT_CasADi__MX_std__vectorT_CasADi__MX_std__allocatorT_CasADi__MX_t_t_t;
%}

%meta_pair(CasADi::MX, std::vector< CasADi::MX >)
%typemap(out) std::pair< CasADi::MX, std::vector< CasADi::MX >  > {
    bool ret = meta< std::pair< CasADi::MX, std::vector< CasADi::MX >  > >::toPython($1,$result);
    if (!ret) SWIG_exception_fail(SWIG_TypeError,"Could not convert to (MX,std::vector<MX>)");
}
#endif //SWIGPYTHON


%include "casadi/mx/mx_tools.hpp"
