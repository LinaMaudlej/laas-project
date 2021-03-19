//////////////////////////////////////////////////////////////////////////////  
// 
//  PortMask object provising bit mask abstraction
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
#include "portmask.h"
using namespace std;

PortMask::PortMask() {
  PortMask(64, 0);
}

PortMask::PortMask(unsigned len, unsigned long val)
{
  if (len > 64) {
    cerr << "BUG: can't use PortMaks with > 64 bits. Required:" << len << endl;
    exit(1);
  }

  numBits = len;
  mask = (unsigned long)(((1ULL)<<len) - 1);
  value = val & mask;
}

PortMask::PortMask(const PortMask &ref)
{
  numBits = ref.numBits;
  mask = ref.mask;
  value = ref.value;
}

unsigned long
PortMask::setBit(unsigned int idx, bool val)
{
  if (val) {
    value |= (1UL)<<idx;
  } else {
    value &= ~((1UL)<<idx);
  }
  return value;
}

bool 
PortMask::getBit(unsigned int idx) const
{
  unsigned long r = value & ( (1UL)<<idx);
  return (r != 0);
}

size_t
PortMask::getBits(bool v, std::vector<unsigned int> &bitIdxs) const
{
  bitIdxs.clear();
  for (unsigned int i = 0; i < numBits; i++) {
    if (getBit(i) == v) {
      bitIdxs.push_back(i);
    }
  }
  return bitIdxs.size();
}

PortMask PortMask::operator &(const PortMask &rhs)
{
  if (numBits != rhs.numBits) {
    cerr << "BUG: can't bitwaise and PortMask of different length:" 
         << numBits << " vs. " << rhs.numBits << endl;
    exit(1);
  }
  return PortMask(numBits, value & rhs.value);
}

PortMask PortMask::operator |(const PortMask &rhs)
{
  if (numBits != rhs.numBits) {
    cerr << "BUG: can't bitwaise and PortMask of different length:" 
         << numBits << " vs. " << rhs.numBits << endl;
    exit(1);
  }
  return PortMask(numBits, value | rhs.value);
}


// assign and
PortMask &PortMask::operator&=(const PortMask &rhs)
{
  if (numBits != rhs.numBits) {
    cerr << "BUG: can't bitwaise and PortMask of different length:" 
         << numBits << " vs. " << rhs.numBits << endl;
    exit(1);
  }
  value &= rhs.value;
  return *this;
}

PortMask &PortMask::operator&=(const unsigned long val)
{
  value &= val;
  return *this;
}

std::ostream& operator<<(std::ostream& out, const PortMask &pm) {
  for (int c = pm.len()-1; c >= 0; c--) {
    if (pm.getBit(c)) {
      out << "1";
    } else {
      out << "0";
    }
  }
  return out << " #1=" << pm.nBits();
}

