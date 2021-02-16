//////////////////////////////////////////////////////////////////////////////  
// 
//  The Header file describing the isolation approximation algorithm
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
#ifndef _ISOL_H_
#define _ISOL_H_
#include <list>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include "portmask.h"
#include "ft3.h"

class JobSetting {
  int A,B,C,D,R,S,Q;
 public:
  JobSetting() {
    A=0; B=0; C=0; D=0; R=0; S=0; Q=0;
  };
  JobSetting(int a, int b, int c, int d, int r, int s, int q) {
    A=a; B=b; C=c; D=d; R=r; S=s; Q=q;
  };
  JobSetting (const JobSetting &obj) {
    A=obj.A; B=obj.B; C=obj.C; D=obj.D; R=obj.R; S=obj.S; Q=obj.Q;
  };
  std::string str();
};

// an isolating placement algorithm
class IsolAlgo {
  bool verbose;
  ThreeLevelFatTree *ft;
  std::ofstream log;
  
  // optional behavior:
  bool noLinkAlloc;

  std::map<int, struct JobSetting> settingByJob;

  // note we number the L3 first by BP (bipartite which is the index of the 
  // L2 connected) and then by the number in the BP. 
  // So all 0..n-1 L3's are connected to the first L2 in each of the sub-trees
  std::vector< PortMask > L1FreeDnPorts; 
  std::vector< PortMask > L1FreeUpPorts; 
  std::vector< PortMask > L2FreeUpPorts;
  std::map<int, std::map< int, std::list< int > > > L1DnPortsByJob;
  std::map<int, std::map< int, std::list< int > > > L1UpPortsByJob;
  std::map<int, std::map< int, std::list< int > > > L2UpPortsByJob;

  bool placeSubTrees(int Q, 
                     int U, 
                     int A, 
                     int B, 
                     int R,
                     std::vector<int> &l1Allocations, 
                     std::vector<int> &l2Allocations);

  bool possiblePlacement(int N,
                         std::vector<int> &l1Allocations, 
                         std::vector<int> &l2Allocations,
                         std::vector<PortMask> &jobL1UpPorts,
                         std::vector<PortMask> &jobL2UpPorts,
                         int &A, int &B, int &C, int &D, 
                         int &R, int &S, int &Q);

  bool placeL2SubTrees(int A, int B, int R,
                       std::vector<int> &l1Allocations, 
                       std::vector<PortMask> &jobL1UpPorts);

  bool placeL3SubTrees(int R, int S, int Q, 
                       std::vector<int> &l1Allocations, 
                       std::vector<int> &l2Allocations, 
                       std::vector<PortMask> &jobL1UpPorts,
                       std::vector<PortMask> &jobL2UpPorts);

  int recordAllocation(int jobId, 
                       int origNumHosts,
                       std::vector<int> &l1Allocations, 
                       std::vector<int> &l2Allocations,
                       std::vector<PortMask> &jobL1UpPorts,
                       std::vector<PortMask> &jobL2UpPorts,
                       unsigned int &numLinks);

  bool initAllocations( std::vector<int> &l1Allocations, 
                        std::vector<int> &l2Allocations,
                        std::vector<PortMask> &jobL1UpPorts,
                        std::vector<PortMask> &jobL2UpPorts);

  bool findUniqueLeaf(
                      // CONSTS
                      std::vector<SubTree> *H, // free hosts by leaf idx
                      std::vector<PortMask> *F, // free ports per leaf
                      int B,  // if <= 0 no unique leafs
                      int l0, // start leaf idx
                      int le, // last leaf idx + 1
                      // CURRENT STATE
                      std::set<int> &RL, // repeated leafs
                      PortMask &RM, // repeated ports mask
                      // OUTPUTS
                      int         &UL, // unique leaf index/ or -1
                      PortMask    &UM); // unique ports mask

  void nerrowMasks(int neededAPorts, int B, 
                   PortMask &repeatedPortMask, 
                   PortMask &uniquePortMask);

  bool findCommonSpineLeafs(
                            // CONSTS
                            std::vector<SubTree> *H, // free hosts by leaf idx
                            std::vector<PortMask> *F, // free ports per leaf
                            int A,
                            int B,  // if <= 0 no unique leafs
                            int R,
                            int AP, // needed A ports (may be < A)
                            int l0, // start leaf idx
                            int le, // last leaf idx + 1
                            // CURRENT STATE
                            int l,  // start leaf idx in [l0,ln]
                            int r,  // this leafs place in R
                            PortMask mask, // current propogated mask
                            std::set<int> rleafs, // selected leafs so far
                            // OUTPUTS
                            std::set<int> &RL, // repeated leafs
                            int      &UL, // unique leaf index/ or -1
                            PortMask &RM, // output repeated ports
                            PortMask &UM); // output unique ports

  int rePackSpinePorts();

  int getNumFreeHostWithAlloc(SubTree *st, std::vector<int> &allocations);

  int availableHosts;

  void dumpL3();
  void dumpL2();

 public:
  IsolAlgo(string o);

  // return true if placement succeeded
  bool placeJob(int jobId, int N);
  bool completeJob(int jobId, int numHosts);
  int setFatTree(ThreeLevelFatTree *ft);
  string jobSettingStr(int);

  friend class LaaS;
};

#endif // _ISOL_H_
