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
# -*- coding: utf-8 -*-
from casadi import *
import numpy as N
import matplotlib.pyplot as plt

"""
Demonstration on how to construct a fixed-step implicit Runge-Kutta integrator
@author: Joel Andersson, K.U. Leuven 2013
"""

# End time
tf = 10.0  

# Dimensions
nx = 3
np = 1

# Declare variables
x  = ssym("x",nx)  # state
p  = ssym("u",np)  # control

# ODE right hand side function
ode = vertcat([(1 - x[1]*x[1])*x[0] - x[1] + p, \
               x[0], \
               x[0]*x[0] + x[1]*x[1] + p*p])
f = SXFunction(daeIn(x=x,p=p),daeOut(ode=ode))
f.init()

# Number of finite elements
n = 100     

# Size of the finite elements
h = tf/n

# Choose collocation points
tau_root = collocationPoints(4,"legendre")

# Degree of interpolating polynomial
d = len(tau_root)-1

# Coefficients of the collocation equation
C = N.zeros((d+1,d+1))

# Coefficients of the continuity equation
D = N.zeros(d+1)

# Dimensionless time inside one control interval
tau = ssym("tau")
  
# For all collocation points
for j in range(d+1):
  # Construct Lagrange polynomials to get the polynomial basis at the collocation point
  L = 1
  for r in range(d+1):
    if r != j:
      L *= (tau-tau_root[r])/(tau_root[j]-tau_root[r])
  lfcn = SXFunction([tau],[L])
  lfcn.init()
  
  # Evaluate the polynomial at the final time to get the coefficients of the continuity equation
  lfcn.setInput(1.0)
  lfcn.evaluate()
  D[j] = lfcn.getOutput()

  # Evaluate the time derivative of the polynomial at all collocation points to get the coefficients of the continuity equation
  for r in range(d+1):
    lfcn.setInput(tau_root[r])
    lfcn.setFwdSeed(1.0)
    lfcn.evaluate(1,0)
    C[j,r] = lfcn.getFwdSens()

# Total number of variables for one finite element
X0 = msym("X0",nx)
P  = msym("P",np)
V = msym("V",d*nx)

# Get the state at each collocation point
X = [X0] + list(vertsplit(V,[r*nx for r in range(d+1)]))

# Get the collocation quations (that define V)
V_eq = []
for j in range(1,d+1):
  # Expression for the state derivative at the collocation point
  xp_j = 0
  for r in range (d+1):
    xp_j += C[r,j]*X[r]
      
  # Append collocation equations
  [f_j] = daeOut(f.call(daeIn(x=X[j],p=P)),"ode")
  V_eq.append(h*f_j - xp_j)

# Concatenate constraints
V_eq = vertcat(V_eq)

# Root-finding function, implicitly defines V as a function of X0 and P
vfcn = MXFunction([V,X0,P],[V_eq])
vfcn.init()
  
# Convert to SXFunction to decrease overhead
vfcn_sx = SXFunction(vfcn)

# Create a implicit function instance to solve the system of equations
ifcn = NewtonImplicitSolver(vfcn_sx)
ifcn.setOption("linear_solver",CSparse)
ifcn.init()
[V] = ifcn.call([X0,P])
X = [X0 if r==0 else V[(r-1)*nx:r*nx] for r in range(d+1)]

# Get an expression for the state at the end of the finie element
XF = 0
for r in range(d+1):
  XF += D[r]*X[r]
  
# Get the discrete time dynamics
F = MXFunction([X0,P],[XF])
F.init()

# Do this iteratively for all finite elements
X = X0
for i in range(n):
  [X] = F.call([X,P])

# Fixed-step integrator
irk_integrator = MXFunction(integratorIn(x0=X0,p=P),integratorOut(xf=X))
irk_integrator.setOption("name","irk_integrator")
irk_integrator.setOption("number_of_fwd_dir",2)
irk_integrator.init()

# Create a convensional integrator for reference
ref_integrator = CVodesIntegrator(f)
ref_integrator.setOption("name","ref_integrator")
ref_integrator.setOption("number_of_fwd_dir",2)
ref_integrator.setOption("tf",tf)
ref_integrator.init()

# Test values
x0_val  = N.array([0,1,0])
p_val = 0.2

# Make sure that both integrators give consistent results
for integrator in (irk_integrator,ref_integrator):
  print "-------"
  print "Testing ", integrator.getOption("name")
  print "-------"

  # Pass arguments
  integrator.setInput(x0_val,"x0")
  integrator.setInput(p_val,"p")
  
  # Forward sensitivity analysis, first direction: seed p
  integrator.setFwdSeed(0.0,"x0",0)
  integrator.setFwdSeed(1.0,"p",0)
  
  # Forward sensitivity analysis, second direction: seed x0[0]
  integrator.setFwdSeed([1,0,0],"x0",1)
  integrator.setFwdSeed(0.0,"p",1)
  
  # Adjoint sensitivity analysis, seed xf[2]
  integrator.setAdjSeed([0,0,1],"xf")

  # Integrate
  integrator.evaluate(2,1)

  # Get the nondifferentiated results
  print "%15s = " % "xf", integrator.getOutput("xf")

  # Get the forward sensitivities
  print "%15s = " % "d(xf)/d(p)", integrator.getFwdSens("xf",0)
  print "%15s = " % "d(xf)/d(x0[0])", integrator.getFwdSens("xf",1)

  # Get the adjoint sensitivities
  print "%15s = " % "d(xf[2])/d(x0)", integrator.getAdjSens("x0")
  print "%15s = " % "d(xf[2])/d(p)", integrator.getAdjSens("p")

