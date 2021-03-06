// Copyright (c) 2016 Tim Churches
// Utilising some code from https://github.com/xaocdevices/batumi/blob/alternate/lfo.cc
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef UTIL_IRRATIONALS_H_
#define UTIL_IRRATIONALS_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "../OC_strings.h"

//     int bitcount =  __builtin_popcountll(1625255263);

namespace util {

class IrrationalSequence {
public:



  void Init(int16_t i, int16_t l) {
    n_ = 0; // index of irrational series
    i_ = i; // start of loop
    l_ = l; // length of loop
    j_ = i_ + l_; // end of loop
    k_ = i_; // current index
    x_ = 3; // first digit of pi
    loop_ = true;
    pass_go_ = true;
    up_ = true;
  }

  uint16_t Clock() {
  	if (loop_ || up_) {
  		k_ += 1;
  	} else {
  		k_ -= 1;
  	}
  	if (k_ > j_) {
  		if (loop_) {
  			k_ = i_;
  		} else {
  			k_ -= 2;
  			up_ = false;
  		}
  	}
  	if (k_ < i_) {
  		k_ += 2;
  		up_ = true;
  	}  	

  	if (k_ == i_) {
  		pass_go_ = true;
  	} else {
  		pass_go_ = false;
  	}

  	  	
  	switch (n_) {
      case 0:
      	x_ = OC::Strings::pi_digits[k_];
      	break;
      case 1:
      	x_ = OC::Strings::phi_digits[k_];
      	break;
      case 2:
      	x_ = OC::Strings::tau_digits[k_];
      	break;
      case 3:
      	x_ = OC::Strings::eul_digits[k_];
      	break;
      case 4:
      	x_ = OC::Strings::rt2_digits[k_];
      	break;
      case 5:
      	x_ = OC::Strings::dress_digits[k_];
      	break;
      case 6:
      	x_ = OC::Strings::dress31_digits[k_];
      	break;
      case 7:
      	x_ = OC::Strings::dress63_digits[k_];
      	break;
      default:
        break;
    }
    return static_cast<uint16_t>(x_ << 8);
  }

  uint16_t get_register() const {
    return x_ << 8;
  }

  void set_loop_start(int16_t i) {
    if (i < 0) i = 0;
    if (i > 254) i = 254;
    i_ = i; // loop start point
    j_ = i_ + l_;
    if (j_ < 1) j_ = 1;
    if (j_ > 255) j_ = 255;
    if (k_ < i_) k_ = i_;
    if (k_ > j_) k_ = j_;
  }
  
  void set_loop_length(int16_t l) {
    if (l < 1) l = 1;
    if (l > 255) l = 255;
    l_ = l; // loop length
    j_ = i_ + l_;
    if (j_ < 1) j_ = 1;
    if (j_ > 255) j_ = 255;
    if (k_ < i_) k_ = i_;
    if (k_ > j_) k_ = j_;
  }

  void set_loop_direction(bool p) {
    loop_ = p; // loop direction, false = swing (pendulum), true = loop
  }

  void set_irr_seq(int16_t n) {
    n_ = n; 
  }

  void reset_loop() {
    k_ = i_;
  }

  int16_t get_k() const {
    return k_;
  }

  int16_t get_l() const {
    return l_;
  }

  int16_t get_i() const {
    return i_;
  }

  int16_t get_j() const {
    return j_;
  }

  int16_t get_n() const {
    return n_;
  }

  bool get_pass_go() const {
    return pass_go_;
  }

private:
  int16_t n_;
  int16_t k_;
  int16_t i_;
  int16_t j_;
  int16_t l_;
  int16_t x_;
  bool loop_;
  bool pass_go_;
  bool up_ ;
};

}; // namespace util

#endif // UTIL_IRRATIONALS_H_
