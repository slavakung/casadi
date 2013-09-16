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

#ifndef IO_SCHEME_VECTOR_HPP
#define IO_SCHEME_VECTOR_HPP

#include "schemes_metadata.hpp"
#include "io_scheme.hpp"
#include "../casadi_exception.hpp"
#include "../printable_object.hpp"
namespace CasADi{
  
template<typename T>
class IOSchemeVector : public PrintableObject {
    public:
    // Constructor
    IOSchemeVector(const std::vector<T>& t, CasADi::IOScheme io_scheme =  CasADi::IOScheme()) : t_(t), io_scheme_(io_scheme){} 
    
    #ifndef SWIG
    //@{
    // Automatic type conversion
    operator std::vector<T>&(){ return t_;}
    operator const std::vector<T>&() const{ return t_;}
    //@}
    T operator[](int i) {
      casadi_assert_message(i>=0 && i<t_.size(),"Index error for " << io_scheme_.name() << ": supplied integer must be >=0 and <= " << t_.size() << " but got " << i << ".");
      return t_.at(i);
    }
    T operator[](const std::string& name) { return (*this)[io_scheme_.index(name)]; }
    #endif // SWIG
    T __getitem__(int i) { if (i<0) i+= t_.size(); return (*this)[i]; }
    T __getitem__(const std::string& name) { return (*this)[name]; }
    int __len__() { return t_.size(); }
    
    //@{
    /// Get access to the data
    std::vector<T> & data() { return t_; }
    const std::vector<T> & data() const { return t_; }
    //@}
    
    /// Access the IOScheme
    CasADi::IOScheme io_scheme() const { return io_scheme_; }
    
    #ifndef SWIG
    /// Print a destription of the object
    virtual void print(std::ostream &stream=std::cout) const { 
      stream << "IOSchemeVector(" ;
      for (int i=0;i<t_.size();++i) {
        stream << io_scheme_.entry(i) << "=" << t_[i];
        if (i<t_.size()-1) stream << ",";
      }
      
      stream << ";" << io_scheme_.name() <<  ")";
    }

    /// Print a representation of the object
    virtual void repr(std::ostream &stream=std::cout) const { print(stream); }

    #endif // SWIG  

    private:
      /// Vector of data
      std::vector<T> t_;
      /// Scheme
      IOScheme io_scheme_; 
    
};


} // namespace CasADi


#endif // IO_SCHEME_VECTOR_HPP
