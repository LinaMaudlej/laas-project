//////////////////////////////////////////////////////////////////////////////  
// 
//  Isolation Approximation Algorithm
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
#include "isol.h"
#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <iomanip>
#include <bitset>
std::vector< PortMask > L1FreeUpPorts_result;

bool isolation=true;

string
JobSetting::str() 
{
  stringstream s;
  s << "A=" << A << " B=" << B << " C=" << C << " D=" << D
    << " R=" << R << " S=" << S << " Q=" << Q;
  return s.str();
}

IsolAlgo::IsolAlgo(string o) 
{
  noLinkAlloc = false;

  // break the given string by comma
  stringstream ss(o);
  string item;
  while (getline(ss, item, ',')) {
    if (item == "noLinkAlloc") {
      cout << "-I- Using Isolation with Placement Only" << endl;
      noLinkAlloc = true;
    } else {
      cout << "-F- Unknown IsolAlgo option:" << item << endl;
      exit(1);
    }
  }
}

int 
IsolAlgo::setFatTree(ThreeLevelFatTree *f)
{
  ft = f;
  log.open("isol.log");
  if (!log.good()) {
    cerr << "-F- Could not open the log file: isol.log" << endl;
    exit(1);
  }

  // We manage the link allocation DB on the IsolAlgo as it only 
  // interest for this algo
  PortMask fullL1DnMask(ft->M1,0);
  fullL1DnMask.setAll();
  L1FreeDnPorts.resize(ft->M3 * ft->M2, fullL1DnMask);

  PortMask fullL1Mask(ft->W2,0);
  fullL1Mask.setAll();
  L1FreeUpPorts.resize(ft->M3 * ft->M2, fullL1Mask);
  L1FreeUpPorts_v2.resize(ft->M3 * ft->M2, fullL1Mask);

  L1FreeUpPorts_result.resize(ft->M3 * ft->M2, fullL1Mask);


  // NOTE: we only need a single Bipartite! So we only have 
  // M3 such leafs on a single bipartite.
  // this is true as long as we allocate A=C=18 hosts!
  PortMask fullL2Mask(ft->W3,0);
  fullL2Mask.setAll();
  L2FreeUpPorts.resize(ft->M3, fullL2Mask);

  availableHosts = ft->l3Hosts;

  return 0;
}

// return true if succeeded
bool
IsolAlgo::placeJob(int jobId, int numHosts) {
  int A, B, C, D, R, S, Q;

  if (numHosts <= 0) 
    return true;

  if (verbose) {
    log << "----------------------------------------------------" << endl;
    log << "VERB handling job:" << jobId << " size:" 
        << numHosts << endl;
  }

  if (availableHosts < numHosts) {
    if (verbose)
      log << "VERB abort job: " << jobId << " since request:" 
          << numHosts << " > " << availableHosts << " available" << endl;
    return false;
  }

  // the allocation vectors are sized by the next calls
  // may stay 0 size!
  vector<int> l1Allocations, l2Allocations;
  vector<PortMask> jobL2Ports, jobL3Ports; 

  if (!possiblePlacement(numHosts, l1Allocations, l2Allocations, 
                         jobL2Ports, jobL3Ports,
                         A, B, C, D, R, S, Q)) {
    if (verbose)
      cout << "-V- No possible placement for job: " << jobId
           << " num-hosts: " << numHosts << endl;

    if (verbose) {
      dumpL3();
      dumpL2();
    }

    return false;
  }
  
  if (verbose)
    log << "VERB Job: " << jobId << " placed as: "
        << Q*(R*A+B)+S*C+D << " = " << Q << "(" << R << "*" << A 
        << " + " << B << ") + " << S << "*" << C << " + " << D
        << " A: " << A << " B: " << B << " C: " << C << " D: " << D
        << " R: " << R << " S: " << S << " Q: " << Q << endl;
    
  JobSetting st(A,B,C,D,R,S,Q);
  settingByJob[jobId] = st;
  unsigned int numLinks = 0;
  recordAllocation(jobId, numHosts, l1Allocations, l2Allocations, jobL2Ports, jobL3Ports,
                   numLinks);
  if (verbose) {
    dumpL3();
    dumpL2();
  }

  return true;
}

bool
IsolAlgo::initAllocations( vector<int> &l1Allocations, 
                           vector<int> &l2Allocations,
                           vector<PortMask> &jobL1UpPorts,
                           vector<PortMask> &jobL2UpPorts)
{
  l1Allocations.clear();
  l1Allocations.resize(ft->M3*ft->M2, 0);
  l2Allocations.clear(); 
  l2Allocations.resize(ft->W2*ft->M3, 0);
  
  PortMask emptyL1Mask(ft->W2,0);
  jobL1UpPorts.clear();
  jobL1UpPorts.resize(L1FreeUpPorts.size(), emptyL1Mask);


  PortMask emptyL2Mask(ft->W3,0);
  jobL2UpPorts.clear();
  jobL2UpPorts.resize(L2FreeUpPorts.size(), emptyL2Mask);

  return true;
}

// the main function to try all allocations
bool 
IsolAlgo::findCommonSpineLeafs(
                               // CONSTS
                               vector<SubTree> *H, // free hosts by leaf idx
                               vector<PortMask> *F, // free ports per leaf
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
                               set<int> rleafs, // selected leafs so far
                               // OUTPUTS
                               set<int> &RL, // repeated leafs
                               int      &UL, // unique leaf index/ or -1
                               PortMask &RM, // output repeated ports
                               PortMask &UM) // output unique ports
{
  // we are searching for next leaf with matching mask of A bits
  // TODO: maybe we want another order?
  for (int i = l; i < le; i++) {
    
    // must have enough hosts
    if (&ft->L1SubTreeByIdx == H) {
      if (((*H)[i].numFreeHosts < A))
        continue;
    } else {
      // for L2 subtrees we want complete suntrees so count them
      int numFreeL1s = 0;
      SubTree *l2st = &ft->L2SubTreeByIdx[i];
      for (int p = 0; (numFreeL1s < A) && (p < ft->M2); p++) {
        SubTree *l1st = ft->getSubTree(l2st, p);
        if (l1st->numFreeHosts >= ft->M1) 
          numFreeL1s++;
      }
      if (numFreeL1s < A) 
        continue;
    }
    
    PortMask newMask;
    if (noLinkAlloc) {
      newMask = mask;
    } else {
      newMask = mask & (*F)[i];
    }
    if ((int)newMask.nBits() < AP) 
      continue;
    
    set<int> newRL = rleafs;
    newRL.insert(i);

    // are we done?
    if (r == R) {
      if (findUniqueLeaf(H, F, B, l0, le, newRL, newMask, UL, UM)) {
        RM = newMask;
        RL = newRL;
        return true;
      } 
      return false;
    }

    // need more repeated leafs
    if (findCommonSpineLeafs(H, F, A, B, R, AP, l0, le, // consts
                             i+1, r+1, newMask, newRL,  // cur
                             RL, UL, RM, UM))           // out
      return true;
  }
  return false;
}

bool 
IsolAlgo::findUniqueLeaf(
                         // CONSTS
                         vector<SubTree> *H, // free hosts by leaf idx
                         vector<PortMask> *F, // free ports per leaf
                         int B,  // if <= 0 no unique leafs
                         int l0, // start leaf idx
                         int le, // last leaf idx + 1
                         // CURRENT STATE
                         set<int> &RL, // repeated leafs
                         PortMask &RM, // repeated ports mask
                         // OUTPUTS
                         int         &UL, // unique leaf index/ or -1
                         PortMask    &UM) // unique ports mask
{
  if (B <= 0) 
    return true;

  // TODO: maybe we want another order?
  for (int l = l0; l < le; l++) {
    // should not be already used as repeated
    if (RL.find(l) != RL.end())
      continue;

    // should have enough free hosts
    // must have enough hosts
    if (&ft->L1SubTreeByIdx == H) {
      if (((*H)[l].numFreeHosts < B))
        continue;
    } else {
      // for L2 subtrees we want complete suntrees so count them
      int numFreeL1s = 0;
      SubTree *l2st = &ft->L2SubTreeByIdx[l];
      for (int p = 0; (numFreeL1s < B) && (p < ft->M2); p++) {
        SubTree *l1st = ft->getSubTree(l2st, p);
        if (l1st->numFreeHosts >= ft->M1) 
          numFreeL1s++;
      }
      if (numFreeL1s < B) 
        continue;
    }
    
    PortMask mask;
    if (noLinkAlloc) {
      mask = RM;
    } else {
      mask = (*F)[l] & RM;
    }

    if ((int)mask.nBits() >= B) {
      UL = l;
      UM = mask;
      return true;
    }
  }
  return false;
}
 
void
IsolAlgo::nerrowMasks(int neededAPorts, int B, 
                      PortMask &repeatedPortMask, 
                      PortMask &uniquePortMask)
{
  if (neededAPorts == 0) {
    repeatedPortMask.clrAll();
    uniquePortMask.clrAll();
    return;
  }

  // scan through the B mask first remove extra bits
  if (B == 0) {
    uniquePortMask.clrAll();
  } else {
    int extraB = uniquePortMask.nBits() - B;
    for (int p = (int)uniquePortMask.len() -1; (extraB > 0) && (p >= 0); p--) {
      if (uniquePortMask.getBit(p)) {
        if (extraB > 0) {
          uniquePortMask.setBit(p, 0);
          extraB--;
        }
      }
    }
  }

  // now scan through A bits to find extra bits but they might be used by B
  // so we can't simply remove by order!
  int extraA = repeatedPortMask.nBits() - neededAPorts;
  for (int p = (int)repeatedPortMask.len() -1; (extraA > 0) && (p >= 0); p--) {
    if (!uniquePortMask.getBit(p)) {
      repeatedPortMask.setBit(p, 0);
      extraA--;
    }
  }
}

string v2str(const vector<int> &v)
{
  ostringstream out;
  for (unsigned c = 0; c < v.size(); c++) {
    if (c) out << ",";
    out << v[c];
  }
  return out.str();
}

string s2str(const set<int> &v)
{
  ostringstream out;
  set<int>::const_iterator I;
  for (I = v.begin(); I != v.end(); I++) {
    if (I != v.begin()) out << ",";
    out << (*I);
  }
  return out.str();
}


// given that we search for single L2 sub-tree placement find it
bool
IsolAlgo::placeL2SubTrees(int A, int B, int R,
                          vector<int> &l1Allocations, 
                          vector<PortMask> &jobL1UpPorts)
{
  // try on each of the L2 sub trees to find a port allocation 
  // by this A B R
  int N = R*A+B;

  // go over all L2 sub trees looking for candidates
  set<int> repeatedLeafs;
  PortMask uniquePortMask(ft->W2,0);
  PortMask repeatedPortMask(ft->W2, 0);
  int uniqueLeaf = -1;
  const setSubTreeByLessUsage &L2ByFreeSpace = ft->getSubTreeByLessUsageSet(2);

  // we may have a special case of smaller than A needed rep leaf ports
  int neededAPorts = A;
  if (R == 1) {
    if (B <= 0) {
      neededAPorts = 0;
    } else {
      neededAPorts = B;
    }
  }
  
  bool found = false;
  setSubTreeByLessUsage::const_iterator l2I;
  for (l2I = L2ByFreeSpace.begin(); l2I != L2ByFreeSpace.end(); l2I++) {
    SubTree *st2 = *l2I;
    if (st2->numFreeHosts < N) 
      continue;

    int l0 = ft->getSubTree(st2, 0)->idxInLevel;
    int le = l0 + ft->M2;

    // can we do it?
    repeatedLeafs.clear();
    PortMask portMask(ft->W2, 0);
    portMask.setAll();
    std::vector< PortMask >* L1FreeUpPorts_tmp;
    L1FreeUpPorts_tmp=&L1FreeUpPorts; //TODO: individual isolation is required (and between them).
    if(isolation==false){
	   	for (size_t i = 0; i < L1FreeUpPorts.size(); i++) {
		(L1FreeUpPorts_result)[i]= L1FreeUpPorts[i] | L1FreeUpPorts_v2[i];
	}
		L1FreeUpPorts_tmp=&L1FreeUpPorts_result;
    }
    if (findCommonSpineLeafs(
                             // CONSTS 
                             &ft->L1SubTreeByIdx, // free hosts per L1 leaf
                             L1FreeUpPorts_tmp, // free ports per L1 leaf
			     A,
                             B,  // if <= 0 no unique leafs
                             R,
                             neededAPorts, // needed A ports (may be < A)
                             l0, // start leaf idx
                             le, // last leaf idx + 1
                             // CURRENT STATE
                             l0,  // start leaf idx in [l0,ln]
                             1,   // this leafs index in R
                             portMask,      // full mask to start with
                             repeatedLeafs, // previously selected leafs so far
                             // OUTPUTS
                             repeatedLeafs, // repeated leafs
                             uniqueLeaf,    // unique leaf index/ or -1
                             repeatedPortMask, // output repeated ports
                             uniquePortMask)) {  // output unique ports
      found = true;
      break;
    }
  } // for all L2 sub trees
  
  if (!found)
    return false;
  
  // need to adjust the port masks first - they are not cut to size yet
  // select the first B and then additional
  nerrowMasks(neededAPorts, B, repeatedPortMask, uniquePortMask);

  if (verbose) 
    log << "VERB L2 alloc repeated: " << s2str(repeatedLeafs)
        << " mask: " << repeatedPortMask 
        << " unique: " << uniqueLeaf << " mask: " << uniquePortMask << endl;

  // fill in the allocations correctly!
  // and assign the repeated leafs by their port mask
  set<int>::const_iterator I;
  for (I = repeatedLeafs.begin(); I != repeatedLeafs.end(); I++) {
    int leaf = *I;
    l1Allocations[leaf] = A;
    jobL1UpPorts[leaf] = repeatedPortMask;
  }

  if (B > 0) {
    l1Allocations[uniqueLeaf] = B;
    jobL1UpPorts[uniqueLeaf] = uniquePortMask;
  }
  
  return true;
}

// given that we search for A=C=M1 and multiple L2 sub-tree placement
bool
IsolAlgo::placeL3SubTrees(int R, int S, int Q, 
                          vector<int> &l1Allocations, 
                          vector<int> &l2Allocations, 
                          vector<PortMask> &jobL1UpPorts,
                          vector<PortMask> &jobL2UpPorts)
{
  // go over all L2 sub trees looking for candidates
  set<int> repeatedLeafs;
  PortMask uniquePortMask(ft->W3,0);
  PortMask repeatedPortMask(ft->W3, 0);
  int uniqueLeaf = -1;

  // we may have a special case of smaller than A needed rep leaf ports
  int neededRPorts = R;
  if (Q == 1) {
    if (S <= 0) {
      neededRPorts = 0;
    } else {
      neededRPorts = S;
    }
  }
  
  int l0 = 0;
  int le = ft->M3;

  repeatedLeafs.clear();
  PortMask portMask(ft->W3, 0);
  portMask.setAll();

  bool found = 
    findCommonSpineLeafs(
                         // CONSTS 
                         &ft->L2SubTreeByIdx, // free hosts per L2 leaf
                         &L2FreeUpPorts, // free ports per L2 leaf
                         R,
                         S,  // if <= 0 no unique leafs
                         Q,
                         neededRPorts, // needed R ports (may be < R)
                         l0, // start leaf idx
                         le, // last leaf idx + 1
                         // CURRENT STATE
                         l0,  // start leaf idx in [l0,ln]
                         1,   // this leafs index in Q
                         portMask,      // full mask to start with
                         repeatedLeafs, // previously selected leafs so far
                         // OUTPUTS
                         repeatedLeafs, // repeated leafs
                         uniqueLeaf,    // unique leaf index/ or -1
                         repeatedPortMask, // output repeated ports
                         uniquePortMask);  // output unique ports
  
  if (!found)
    return false;
  
  // need to adjust the port masks first - they are not cut to size yet
  // select the first B and then additional
  nerrowMasks(neededRPorts, S, repeatedPortMask, uniquePortMask);

  if (verbose) 
    log << "VERB L3 alloc repeated: " << s2str(repeatedLeafs)
        << " mask: " << repeatedPortMask 
        << " unique: " << uniqueLeaf << " mask: " << uniquePortMask << endl;

  // fill in the allocations correctly!
  // and assign the repeated leafs by their port mask
  set<int>::const_iterator I;
  for (I = repeatedLeafs.begin(); I != repeatedLeafs.end(); I++) {
    int l2 = *I;
    l2Allocations[l2] = R*ft->M1;
    jobL2UpPorts[l2] = repeatedPortMask;

    // but we also need to allocate the hosts at the L1 level:
    // take the first R sub-trees
    int neededL1s = R;
    SubTree *l2st = &ft->L2SubTreeByIdx[l2];
    for (int i = 0; neededL1s && (i < ft->M2); i++) {
      SubTree *l1st = ft->getSubTree(l2st, i);
      if (l1st->numFreeHosts >= ft->M1) {
        l1Allocations[l1st->idxInLevel] = ft->M1;
        jobL1UpPorts[l1st->idxInLevel].setAll();
        neededL1s--;
      }
    }
    if (neededL1s) {
      cerr << "BUG: how come we could not allocate R " << neededL1s
           << " L1s under L2:" << l2 << endl;
      dumpL3();
      dumpL2();
      exit(1);
    }
  }

  if (S > 0) {
    l2Allocations[uniqueLeaf] = S*ft->M1;
    jobL2UpPorts[uniqueLeaf] = uniquePortMask;
    SubTree *l2st = &ft->L2SubTreeByIdx[uniqueLeaf];
    int neededL1s = S;
    for (int i = 0; neededL1s && (i < ft->M2); i++) {
      SubTree *l1st = ft->getSubTree(l2st, i);
      if (l1st->numFreeHosts >= ft->M1) {
        l1Allocations[l1st->idxInLevel] = ft->M1;
        jobL1UpPorts[l1st->idxInLevel].setAll();
        neededL1s--;
      }
    }    
    if (neededL1s) {
      cerr << "BUG: how come we did could allocate S " << neededL1s
           << " L1s under L2:" << uniqueLeaf << endl;
      dumpL3();
      dumpL2();
      exit(1);
    }
  }
  
  return true;
}

// Terminology:
// Nj = Qj (RjAj + Bj) + SjCj+Dj 
// But we limit the scope to either N=RA+B or N=K(QR+S)
bool
IsolAlgo::possiblePlacement(int N,
                            vector<int> &l1Allocations, 
                            vector<int> &l2Allocations,
                            vector<PortMask> &jobL1UpPorts,
                            vector<PortMask> &jobL2UpPorts,
                            int &A, int &B, int &C, int &D, 
                            int &R, int &S, int &Q)
{
  // We simply try to allocate in as less sub-trees as we can:

  // first we try on single L2 tree: N=RA+B
  if (N < ft->M1*ft->M2) {
    Q=1; C=D=S=0;

    int maxA = N;
    if (maxA > ft->M1) 
      maxA = ft->M1;
    for (A = maxA; A >= 1; A--) {
      R = N / A;
      B = N - R * A;

      initAllocations(l1Allocations, l2Allocations, jobL1UpPorts, jobL2UpPorts);
      if (placeL2SubTrees(A, B, R, l1Allocations, jobL1UpPorts)) {
        return true;
      }
    }
  }

  // as it does not fit a single tree we need to round it up
  // The general case not handled: N = Q (R A + B) + S C + D = Q U + V
  // instead we do handle : N = M1*(QR+S)
  int U, maxR;

  U = ((N+ft->M1-1)/ft->M1); // U = ceil(N/M1)
  A=C=ft->M1;
  B=D=0;
  // R can't be more than a complete L2 sub-tree
  maxR = U;
  if (maxR > ft->M2) 
    maxR = ft->M2;

  // now need to try all values of U and V
  for (R = maxR; R >= 1; R--) {
    Q = U / R;
    S = U - Q * R;

    // can't have more than M3 sub L2 trees
    if (Q > ft->M3) {
      if (verbose)
        log << "VERB due to Q > M3 skip R=" << R << " Q=" << Q 
            << " > M3=" << ft->M3
            << " N=" << N << " U=" << U << endl;
      continue;
    }
    
    //if (verbose) 
    //  cout << "-V- trying Q=" << Q << " R=" << R << " S=" << S << endl;

    initAllocations(l1Allocations, l2Allocations, jobL1UpPorts, jobL2UpPorts);
    if (placeL3SubTrees(R, S, Q, l1Allocations, l2Allocations, 
                        jobL1UpPorts, jobL2UpPorts)) {
      return true;
    }
  }

  if (verbose)
    cout << "-V- possiblePlacement fail to place at L3 job N: " << N 
         << " tree has now:" << availableHosts << " vailable hosts" << endl;

  return false;
}

void 
IsolAlgo::dumpL3()
{
  log << "-----------------------------------------------------" << endl;
  // we need to convert the by project links DB into this matrix
  vector<vector<int> > L3LeafPortJob;
  L3LeafPortJob.resize(ft->W3);
  for (size_t i = 0; i < L3LeafPortJob.size(); i++)
    L3LeafPortJob[i].resize(ft->M3,0);

  map<int, map< int, list< int > > >::iterator jI;
  for (jI = L2UpPortsByJob.begin(); jI != L2UpPortsByJob.end(); jI++) {
    int jobId = (*jI).first;
    map< int, list< int > >::iterator lI;
    for (lI = (*jI).second.begin(); lI !=  (*jI).second.end(); lI++) {
      int l2 =  (*lI).first;
      int l3p = l2;
      list< int >::iterator pI;
      for (pI = (*lI).second.begin(); pI != (*lI).second.end(); pI++) {
        int l2p = (*pI);
        L3LeafPortJob[l2p][l3p] = jobId;
      }
    }
  }

  // we go over each bi-partite and pack its jobs from larger to smaller
  log << " L3 Bipartites:" << endl;
  log << "      L2:";
  for (int l = 0; l < ft->M3; l++)
    log << setw(4) << l << " ";
  log << endl;
  log << "      FH:";
  for (int l = 0; l < ft->M3; l++)
    log << setw(4) << ft->L2SubTreeByIdx[l].numFreeHosts << " ";
  log << endl;
  for (int s = 0; s < ft->W3; s++) {
    log << "L3: " << setw(4) << s << "| ";
    for (int l = 0; l < ft->M3; l++) {
      log << setw(4) << L3LeafPortJob[s][l] << " ";
    }
    log << endl;
  }
  log << endl;
}

void 
IsolAlgo::dumpL2()
{
  log << "-----------------------------------------------------" << endl;
  // we need to convert the by project links DB into this matrix
  vector<vector<int> > L2LeafPortJob;
  L2LeafPortJob.resize(ft->W2*ft->M3);
  for (size_t i = 0; i < L2LeafPortJob.size(); i++)
    L2LeafPortJob[i].resize(ft->M2,0);

  map<int, map< int, list< int > > >::iterator jI;
  for (jI = L1UpPortsByJob.begin(); jI != L1UpPortsByJob.end(); jI++) {
    int jobId = (*jI).first;
    map< int, list< int > >::iterator lI;
    for (lI = (*jI).second.begin(); lI !=  (*jI).second.end(); lI++) {
      int l1 =  (*lI).first;
      int l1p = l1 % ft->M2;
      list< int >::iterator pI;
      for (pI = (*lI).second.begin(); pI != (*lI).second.end(); pI++) {
        int p = (*pI);
        int l2 = (l1 / ft->M2)*ft->W2+p;
        L2LeafPortJob[l2][l1p] = jobId;
      }
    }
  }

  // we go over each bi-partite and pack its jobs from larger to smaller
  log << " L2 Bipartites:" << endl;
  for (int bp = 0; bp < ft->M3; bp++) {
    log << "      L1:";
    for (int l = 0; l < ft->M2; l++)
      log << setw(4) << l+bp*ft->W2 << " ";
    log << endl;
    log << "      FH:";
    for (int l = 0; l < ft->M2; l++)
      log << setw(4) << ft->L1SubTreeByIdx[l+bp*ft->W2].numFreeHosts << " ";
    log << endl;
    for (int l = 0; l < ft->W2; l++) {
      int l2 = l + bp*ft->W2;
      log << "L2: " << setw(4) << l2 << "| ";
      for (int l1 = 0; l1 < ft->M2; l1++) {
        log << setw(4) << L2LeafPortJob[l2][l1] << " ";
      }
      log << endl;
    }
    log << endl;
  }
  log << endl;
}

// re-assign ports to all active jobs!
int 
IsolAlgo::rePackSpinePorts()
{
  // Input DB:

  // NOTE: for now as we only allocate links for cases where
  // A=C=18 and B=D=0 we KNOW ALL BPs are the SAME !!!!
  dumpL3();
  dumpL2();

  return 0;
}

// apply an allocation to our internal DB
int
IsolAlgo::recordAllocation(int jobId, 
                           int origHosts,
                           vector<int> &l1Allocations, 
                           vector<int> &l2Allocations,
                           vector<PortMask> &jobL1UpPorts,
                           vector<PortMask> &jobL2UpPorts,
                           unsigned int &numLinks)
{
  // register the allocated hosts. note that registering at 
  // L1 changes L2 free hosts
  int numL1s = 0;
  unsigned int numHosts = 0;
  for (size_t i = 0; i < l1Allocations.size(); i++) {
    unsigned int n = l1Allocations[i];
    if (n > 0) {
      SubTree *st = &ft->L1SubTreeByIdx[i];
      ft->setJobNodeAllocation(jobId, st, n);
      availableHosts -= n;
      numL1s++;
      numHosts += n;

      // actual allocation of hosts by playing with Dn port mask
      if (L1FreeDnPorts[i].nBits() < n) {
        cerr << "-F- BUG as L1FreeDnPorts[" << i << "] = " << L1FreeDnPorts[i]
             << " < required hosts: " << n << endl;
        exit(1);
      }
      // take the first available n bits...
      PortMask mask(L1FreeDnPorts[i].len(), 0);
      for (unsigned int j = 0; (n > 0) && (j < L1FreeDnPorts[i].len()); j++)  {
        if (L1FreeDnPorts[i].getBit(j)) {
          mask.setBit(j, true);
          L1FreeDnPorts[i].setBit(j, false);
          L1DnPortsByJob[jobId][i].push_back(j);
          n--;
        }
      } 
    }
  }

  numLinks = numHosts;
  if (!noLinkAlloc) {
    for (size_t l = 0; l < jobL1UpPorts.size(); l++) {
      unsigned long usedPorts = jobL1UpPorts[l].get();

      if(isolation==false){
        //update v2 
        L1FreeUpPorts_v2[l] &= ~(usedPorts & ~(L1FreeUpPorts[l].get()));
      }else{
        L1FreeUpPorts_v2[l] &= ~usedPorts;
      }
      L1FreeUpPorts[l] &= ~usedPorts;
      for (int p = 0; p < ft->W2; p++) {
        if (jobL1UpPorts[l].getBit(p)) {
          L1UpPortsByJob[jobId][l].push_back(p);
          numLinks++;
        }
      }
    }
    unsigned int numL2Links = 0;
    for (size_t l2 = 0; l2 < jobL2UpPorts.size(); l2++) {
      unsigned long usedPorts = jobL2UpPorts[l2].get();
      L2FreeUpPorts[l2] &= ~usedPorts;
      for (int p = 0; p < ft->W3; p++) {
        if (jobL2UpPorts[l2].getBit(p)) {
          L2UpPortsByJob[jobId][l2].push_back(p);
          numL2Links++;
        }
      }
    }
    numLinks += numL2Links*ft->W3;
  }

  log << "ADD Job:" << jobId << " hosts:" << origHosts << " links:" << numLinks
      << " " << jobSettingStr(jobId) << endl;

  return 0;
}

// cleanup the job allocations
bool 
IsolAlgo::completeJob(int jobId, int numHosts)
{
  unsigned int numL2Links = 0;
  unsigned int numLinks = 0;

  if (verbose) 
    log << "----------------------------------------------------" << endl;
  
  if (numHosts <= 0) 
    return true;
  
  availableHosts += ft->delJobNodeAllocation(jobId);

  map<int, map< int, list< int > > >::iterator jI;
  jI = L1DnPortsByJob.find(jobId);
  if (jI != L1DnPortsByJob.end()) {
    map< int, list< int > >::iterator lI;
    for (lI = (*jI).second.begin(); lI !=  (*jI).second.end(); lI++) {
      int l =  (*lI).first;
      list< int >::iterator pI;
      for (pI = (*lI).second.begin(); pI != (*lI).second.end(); pI++) {
        int p = (*pI);
        L1FreeDnPorts[l].setBit(p,1);
        numLinks++;
      }
    }
  }
  L1DnPortsByJob.erase(jobId);

  jI = L1UpPortsByJob.find(jobId);
  if (jI != L1UpPortsByJob.end()) {
    map< int, list< int > >::iterator lI;
    for (lI = (*jI).second.begin(); lI !=  (*jI).second.end(); lI++) {
      int l =  (*lI).first;
      list< int >::iterator pI;
      for (pI = (*lI).second.begin(); pI != (*lI).second.end(); pI++) {
        int p = (*pI);
	if(isolation==false){
	  if(L1FreeUpPorts[l].getBit(p)){
		 L1FreeUpPorts_v2[l].setBit(p,1);
	   }else{
		 L1FreeUpPorts[l].setBit(p,1);
	   }  
	}else{
	  L1FreeUpPorts_v2[l].setBit(p,1);
	  L1FreeUpPorts[l].setBit(p,1);
	}
           numLinks++;
      }
    }
  }
  L1UpPortsByJob.erase(jobId);

  jI = L2UpPortsByJob.find(jobId);
  if (jI != L2UpPortsByJob.end()) {
    map< int, list< int > >::iterator lI;
    for (lI = (*jI).second.begin(); lI !=  (*jI).second.end(); lI++) {
      int l =  (*lI).first;
      list< int >::iterator pI;
      for (pI = (*lI).second.begin(); pI != (*lI).second.end(); pI++) {
        int p = (*pI);
        L2FreeUpPorts[l].setBit(p,1);
        numL2Links++;
      }
    }
  }
  L2UpPortsByJob.erase(jobId);
  numLinks += numL2Links*ft->W3;

  log << "REM Job:" << jobId << " hosts:" << numHosts //<< " links:" << numLinks
      << " " << jobSettingStr(jobId) << endl;

  return true;
}

string
IsolAlgo::jobSettingStr(int job)
{
  map<int, int> l1Ports;
  stringstream s;

  map<int, JobSetting>::iterator stI = settingByJob.find(job);
  if (stI == settingByJob.end()) {
    cout << "-E- Could not find JobSetting for job:"
         << job << endl;
    exit(1);
  }
  s << (*stI).second.str() << " ";

  // collect all leafs of this job and provide a short vectorized log
  map<int, map<SubTree *, list<int> > >::iterator jI;
  jI = ft->jobSubTreePorts.find(job);
  if (jI == ft->jobSubTreePorts.end()) {
    cout << "-E- Could not find any allocated sub-tree ports for job:"
         << job << endl;
    exit(1);
  }

  map<SubTree *, list<int> >::iterator sI;
  for (sI = (*jI).second.begin(); sI != (*jI).second.end(); sI++) {
    SubTree *st = (*sI).first;
    list<int>::iterator pI;
    if (st->level == 2)
      continue;
    l1Ports[st->idxInLevel] = (*sI).second.size();
  }

  s << "LEAFS";
  for (map<int, int>::iterator I = l1Ports.begin(); I != l1Ports.end(); I++) {
    s << " " << (*I).first << ":" << (*I).second;
  }

  // now include the port allocations:
  map<int, map< int, list< int > > >::iterator jpI;
  jpI = L1UpPortsByJob.find(job);
  if (jpI != L1UpPortsByJob.end()) {
    s << " L1PORTS";
    map< int, list< int > >::iterator lI;
    for (lI = (*jpI).second.begin(); lI !=  (*jpI).second.end(); lI++) {
      int l =  (*lI).first;
      list< int >::iterator pI;
      for (pI = (*lI).second.begin(); pI != (*lI).second.end(); pI++) {
        s << " " << l << ":" << (*pI);
      }
    }
  }

  jpI = L2UpPortsByJob.find(job);
  if (jpI != L2UpPortsByJob.end()) {
    s << " L2PORTS";
    map< int, list< int > >::iterator lI;
    for (lI = (*jpI).second.begin(); lI !=  (*jpI).second.end(); lI++) {
      int l =  (*lI).first;
      list< int >::iterator pI;
      for (pI = (*lI).second.begin(); pI != (*lI).second.end(); pI++) {
        s << " " << l << ":" << (*pI);
      }
    }
  }

  return s.str();
}
