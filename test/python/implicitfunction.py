#
#     This file is part of CasADi.
# 
#     CasADi -- A symbolic framework for dynamic optimization.
#     Copyright (C) 2010 by Joel Andersson, Moritz Diehl, K.U.Leuven. All rights reserved.
# 
#     CasADi is free software; you can redistribute it and/or
#     modify it under the terms of the GNU Lesser General Public
#     License as published by the Free Software Foundation; either
#     version 3 of the License, or (at your option) any later version.
# 
#     CasADi is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#     Lesser General Public License for more details.
# 
#     You should have received a copy of the GNU Lesser General Public
#     License along with CasADi; if not, write to the Free Software
#     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
# 
# 
from casadi import *
import casadi as c
from numpy import *
import unittest
from types import *
from helpers import *

solvers= []
try:
  solvers.append((KinsolSolver,{"linear_solver": CSparse,"abstol":1e-10}))
except:
  pass
try:
  solvers.append((NLPImplicitSolver,{"linear_solver": CSparse,"nlp_solver": IpoptSolver}))
except:
  pass
try:
  solvers.append((NewtonImplicitSolver,{"linear_solver": CSparse}))
except:
  pass

class NLPtests(casadiTestCase):
  def test_scalar1(self):
    self.message("Scalar implicit problem, n=0")
    for Solver, options in solvers:
      self.message(Solver.__name__)
      x=SX("x")
      f=SXFunction([x],[sin(x)])
      f.init()
      solver=Solver(f)
      solver.setOption(options)
      solver.init()
      solver.setOutput(6)
      solver.evaluate()
      
      refsol = SXFunction([],[2*pi])
      refsol.init()
      self.checkfx(solver,refsol,digits=5)         
      
  def test_scalar2(self):
    self.message("Scalar implicit problem, n=1")
    for Solver, options in solvers:
      self.message(Solver.__name__)
      message = Solver.__name__
      x=SX("x")
      y=SX("y")
      n=0.2
      f=SXFunction([y,x],[x-arcsin(y)])
      f.init()
      solver=Solver(f)
      solver.setOption(options)
      solver.init()
      solver.setInput(n)
      
      refsol = SXFunction([x],[sin(x)])
      refsol.init()
      refsol.setInput(n)
      self.checkfx(solver,refsol,digits=6,sens_der=False,failmessage=message)

  def test_scalar2_indirect(self):
    for Solver, options in solvers:
      self.message(Solver.__name__)
      message = Solver.__name__
      x=SX("x")
      y=SX("y")
      n=0.2
      f=SXFunction([y,x],[x-arcsin(y)])
      f.init()
      solver=Solver(f)
      solver.setOption(options)
      solver.init()
      
      X = msym("X")
      [R] = solver.call([X])
      
      trial = MXFunction([X],[R])
      trial.init()
      trial.setInput(n)
      
      refsol = SXFunction([x],[sin(x)])
      refsol.init()
      refsol.setInput(n)
      self.checkfx(trial,refsol,digits=6,sens_der=False,failmessage=message)
      
  def test_large(self):
    for Solver, options in solvers:
      if 'Kinsol' in str(Solver): continue
      if 'Newton' in str(Solver): continue
      
      message = Solver.__name__
      N = 5
      s = sp_tril(N)
      x=ssym("x",s)

      y=ssym("y",s)
      y0 = DMatrix(sp_diag(N),0.1)

      f=SXFunction([vecNZ(y),vecNZ(x)],[vecNZ((mul((x+y0),(x+y0).T)-mul((y+y0),(y+y0).T))[s])])
      f.init()
      solver=Solver(f)
      solver.setOption(options)
      # Cholesky is only unique for positive diagonal entries
      solver.setOption("constraints",[1]*s.size())
      solver.init()
      
      X = msym("X",x.sparsity())
      [R] = solver.call([vecNZ(X)])
      
      trial = MXFunction([X],[R])
      trial.init()
      trial.setInput([abs(cos(i)) for i in range(x.size())])
      trial.evaluate()

      f.setInput(trial.output(),0)
      f.setInput(vecNZ(trial.input()),1)
      f.evaluate()

      f.setInput(vecNZ(trial.input()),0)
      f.setInput(vecNZ(trial.input()),1)
      f.evaluate()
      
      refsol = MXFunction([X],[vecNZ(X)])
      refsol.init()
      refsol.setInput(trial.input())

      self.checkfx(trial,refsol,digits=6,sens_der=False,evals=1,failmessage=message)
      
  def test_vector2(self):
    self.message("Scalar implicit problem, n=1")
    for Solver, options in solvers:
      self.message(Solver.__name__)
      message = Solver.__name__
      x=SX("x")
      y=ssym("y",2)
      y0 = DMatrix([0.1,0.4])
      yy = y + y0
      n=0.2
      f=SXFunction([y,x],[vertcat([x-arcsin(yy[0]),yy[1]**2-yy[0]])])
      f.init()
      solver=Solver(f)
      solver.setOption(options)
      solver.init()
      solver.setFwdSeed(1)
      solver.setAdjSeed(1)
      solver.setInput(n)
      solver.evaluate(1,1)
      
      refsol = SXFunction([x],[vertcat([sin(x),sqrt(sin(x))])-y0]) # ,sin(x)**2])
      refsol.init()
      refsol.setInput(n)
      self.checkfx(solver,refsol,digits=5,sens_der=False,failmessage=message)
      
  def testKINSol1c(self):
    self.message("Scalar KINSol problem, n=0, constraint")
    x=SX("x")
    f=SXFunction([x],[sin(x)])
    f.init()
    solver=KinsolSolver(f)
    solver.setOption("constraints",[-1])
    print solver.dictionary()
    solver.init()
    solver.setOutput(-6)
    solver.evaluate()
    self.assertAlmostEqual(solver.getOutput()[0],-2*pi,5)
    
  def test_constraints(self):
    for Solver, options in solvers:
      if 'Kinsol' in str(Solver): continue
      if 'Newton' in str(Solver): continue
      x=ssym("x",2)
      f=SXFunction([x],[vertcat([mul((x+3).T,(x-2)),mul((x-4).T,(x+vertcat([1,2])))])])
      f.init()
      
      solver=Solver(f)
      solver.setOption(options)
      solver.setOption("constraints",[-1,0])
      solver.init()
      solver.evaluate()
      
      self.checkarray(solver.output(),DMatrix([-3.0/50*(sqrt(1201)-1),2.0/25*(sqrt(1201)-1)]),digits=6)

      f=SXFunction([x],[vertcat([mul((x+3).T,(x-2)),mul((x-4).T,(x+vertcat([1,2])))])])
      f.init()
      
      solver=Solver(f)
      solver.setOption(options)
      solver.setOption("constraints",[1,0])
      solver.init()
      solver.evaluate()
      
      self.checkarray(solver.output(),DMatrix([3.0/50*(sqrt(1201)+1),-2.0/25*(sqrt(1201)+1)]),digits=6)

  def test_implicitbug(self):
    # Total number of variables for one finite element
    X0 = msym("X0")
    V = msym("V")

    V_eq = vertcat([V[0]-X0])

    # Root-finding function, implicitly defines V as a function of X0 and P
    vfcn = MXFunction([V,X0],[V_eq])
    vfcn.init()

    # Convert to SXFunction to decrease overhead
    vfcn_sx = SXFunction(vfcn)
    vfcn_sx.setOption("name","S")
    vfcn_sx.setOption("ad_mode","forward")

    # Create a implicit function instance to solve the system of equations
    ifcn = NewtonImplicitSolver(vfcn_sx)
    ifcn.setOption("linear_solver",CSparse)
    ifcn.init()

    #ifcn = MXFunction([X0],[vertcat([X0])])
    #ifcn.setOption("name","I")
    #ifcn.init()
    [V] = ifcn.eval([X0])

    f = 1  # fails

    F = MXFunction([X0],[f*X0+V])
    F.setOption("name","F")
    F.setOption("ad_mode","forward")
    F.init()

    # Test values
    x0_val  = 1

    G = F.gradient(0,0)
    G.init()
    G.setInput(x0_val)
    G.evaluate()
    print G.output()
    print G

    J = F.jacobian(0,0)
    J.init()
    J.setInput(x0_val)
    J.evaluate()
    print J.output()
    print J
    
    self.checkarray(G.output(),DMatrix([2]))
    self.checkarray(J.output(),DMatrix([2]))
    
if __name__ == '__main__':
    unittest.main()

