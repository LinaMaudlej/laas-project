//////////////////////////////////////////////////////////////////////////////  
// 
//  SWIG Header file of Links as a Service - Allocation Service
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

%module laas
%include "std_string.i"
%include "std_vector.i"
%include "std_pair.i"
%include "std_set.i"
namespace std {
   %template(VecInt) vector<int>;
   %template(PairInt) pair<int, int>;
   %template(VecPairInt) vector<pair<int,int> >;
   %template(SetInt) set<int>;
};

%{
#define SWIG_FILE_WITH_INIT
#include "ft3.h"
#include "isol.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include "laas.h"
%}

class LaaS {
 public:
  LaaS(std::vector<int> M, std::vector<int> W, std::string logFileName);

  // if something really bad happen to the module - it will return false
  bool good();

  // Any function returning with non zero also provides the error message.
  // the return code is defined for each function separately
  std::string getLastErrorMsg();

  // set verbosity ON/OFF
  int setVerbose(bool v);

  // reload the log file
  int replayLogFile();

  // NOTE: in the below a switch port is defined as a pair< switch-idx, port-num >
  //       these are logical switch numbers and port numbers... 
  //       Switch index at L1 and L2 is from left to right assuming bipartites are close
  //       Switch index at L3 assumes L3 bipartites are close 

  // get all currently unused switch ports
  int getUnAllocated(std::vector<int> &hosts,           // OUT Hosts
                     std::vector<std::pair<int, int> > &l1UpPorts,  // OUT unallocated L1 switch up ports
                     std::vector<std::pair<int, int> > &l2UpPorts); // OUT unallocated L2 switch up ports

  // get all active tenant ids
  int getTenants(std::set<int> &tenantIds);

  // get the resources allocated to the tenant. return 1 if tenant does not exist
  int getTenantAlloc(int tenantId,
                     std::vector<int> &hosts,           // OUT allocated host indexies
                     std::vector<std::pair<int, int> > &l1UpPorts,  // OUT allocated L1 switch up ports
                     std::vector<std::pair<int, int> > &l2UpPorts); // OUT allocated L2 switch up ports

  // try to allocate the tenant
  int allocTenant(int tenantId,
                  std::vector<int> &GroupHosts); // IN number of hosts per each leaf group index
  
  // manually assign tenant resources
  int assignTenant(int tenantId,
                   std::vector<int> &hosts,                       // IN allocated host indexies
                   std::vector<std::pair<int, int> > &l1UpPorts,  // IN allocated L1 switch up ports
                   std::vector<std::pair<int, int> > &l2UpPorts); // IN allocated L2 switch up ports

  // remove a tenant
  int deallocTenant(int tenantId);

  // set the group each leaf
  int setLeafGroup(int group, std::vector<int> leafIdxs);
  
  // set the cost of hosts on each leaf
  int setLeafCost(int group, std::vector<double> leafIdxs);
  
};

