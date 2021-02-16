//////////////////////////////////////////////////////////////////////////////  
// 
//  Header file of PortMask object provising bit mask abstraction
//
//  Copyright (C) 2014 TO THE AUTHORS
//  Copyright (C) 2014 TO THE AUTHORS
//
// Due to the blind review this software is available for SigComm evaluation 
// only. Once accepted it will be published with chioce of GPLv2 and BSD
// licenses.  
// You are not allowed to copy or publish this software in any way.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
// BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//////////////////////////////////////////////////////////////////////////////
#ifndef _PORTMASK_H_
#define _PORTMASK_H_

#include <iostream>
#include <stdlib.h>
#include <vector>

int __builtin_popcountl (unsigned long);

class PortMask {
  unsigned long mask;
  unsigned long value;  
  int numBits;
public:
  PortMask();
  PortMask(unsigned len, unsigned long val);
  PortMask(const PortMask &obj);
  inline unsigned long get() const {return value;};
  inline unsigned long len() const {return numBits;};
  inline unsigned int nBits() const {return __builtin_popcountl(value);};
  inline void setAll() {value = mask;};
  inline void clrAll() {value = 0;};
  unsigned long setBit(unsigned int idx, bool val);
  bool getBit(unsigned int idx) const;
  size_t getBits(bool v, std::vector<unsigned int> &bitIdxs) const;
  PortMask &operator&=(const PortMask &rhs); // assign and
  PortMask &operator&=(const unsigned long val); // assign and
  PortMask operator&(const PortMask &rhs); // Bitwise and
};
std::ostream& operator<<(std::ostream& out, const PortMask &pm);

#endif // _PORTMASK_H_
