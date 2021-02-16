//////////////////////////////////////////////////////////////////////////////  
// 
//  3 Level Fat Tree abstraction (with resource tracking)
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
#include "ft3.h"
#include <iostream>
#include <stdlib.h>
extern bool verbose;
// allocate and initialize the tree 
ThreeLevelFatTree::ThreeLevelFatTree(int m1, int m2, int m3, int w2, int w3)
{
  int i,j,k = 0;
  M1 = m1;
  M2 = m2;
  M3 = m3;
  W2 = w2;
  W3 = w3;

  L2SubTreeByIdx.resize(M3);
  L1SubTreeByIdx.resize(M2*M3);

  for (i = 0; i < M3; i++) {
    L2SubTreeByIdx[i].idxInLevel = i;
    L2SubTreeByIdx[i].idxInTree = i;
    L2SubTreeByIdx[i].numDnPorts = M2;
    L2SubTreeByIdx[i].numFreeHosts = M2*M1;
    L2SubTreeByIdx[i].level = 2; 
    L2SubTreeByIdx[i].dnPortJob.resize(M2,0); 
    L2SubTreeByIdx[i].parent = NULL;
    L2ByLessUsage.insert(&L2SubTreeByIdx[i]);

    for (j = 0; j < M2; j++) {
      L1SubTreeByIdx[k].idxInLevel = k;
      L1SubTreeByIdx[k].idxInTree = j;
      L1SubTreeByIdx[k].numDnPorts = M1;
      L1SubTreeByIdx[k].numFreeHosts = M1;      
      L1SubTreeByIdx[k].level = 1;
      L1SubTreeByIdx[k].dnPortJob.resize(M1, 0);
      L1SubTreeByIdx[k].parent = &L2SubTreeByIdx[i];
      L1ByLessUsage.insert(&L1SubTreeByIdx[k]);
      k++;
    }
  }

  l1Hosts = M1;
  l2Hosts = l1Hosts*M2;
  l3Hosts = l2Hosts*M3;
}

SubTree *
ThreeLevelFatTree::getSubTree(SubTree *st, int idx)
{
  if (st->level != 2) {
    cout << "-E- getSubTree called on non L2 sub-tree" << endl;
    exit(1);
  }

  if (idx > M2) {
    cout << "-E- getSubTree called with idx = " << idx 
         << "> M2 = " << M2 << endl;
    exit(1);
  }

  return(&L1SubTreeByIdx[idx+M2*st->idxInLevel]);
}

int 
ThreeLevelFatTree::getUnAllocatedSubTrees(SubTree *st, int num, 
                                          vector<int> &idxes)
{
  int numAlloc = 0;
  idxes.resize(num);
  for (int i = 0; (numAlloc < num) && (i < st->numDnPorts); i++) {
    if (st->dnPortJob[i] == 0) {
      idxes[numAlloc++] = i;
    }
  }
  return (numAlloc < num);
}

void
ThreeLevelFatTree::setJobNodeAllocation(int job, SubTree *st, int num)
{
  if (st->level != 1) {
    cerr << "BUG: In this implementation can only allocate at level 1" << endl;
    exit(1);
  }

  // do we have enough un-allocated?
  if (st->numFreeHosts < num) {
    cout << "-E- setJobNodeAllocation : Sub Tree: " << st->name()
         << " has " << st->numFreeHosts
         << " free children < " << num << " required" << endl;
    exit(1);
  }

  vector<int> freeIdxes;
  if (getUnAllocatedSubTrees(st, num, freeIdxes)) {
    cout << "-E- setJobNodeAllocation : Sub Tree: " << st->name()
         << " could not find " << num
         << " free children requested" << endl;
    exit(1);
  }

  // first remove the sub-tree and its parent from the appropriate sets
  L1ByLessUsage.erase(st);
  
  // mark the sub trees as used
  for (int f = 0; f < num ; f++) {
    int i = freeIdxes[f];
    st->dnPortJob[i] = job;
    st->numFreeHosts--;
    jobSubTreePorts[job][st].push_front(i);
  }
  if (st->numFreeHosts < 0) {
    cerr << "BUG: Underrun of st->numFreeHosts on:" << st->name() << endl;
    exit(1);
  }

  // update the parent L2 tree lost some of its hosts too
  if (st->parent->dnPortJob[st->idxInTree] == 0) {
    st->parent->dnPortJob[st->idxInTree] = -1;
  }
  L2ByLessUsage.erase(st->parent);
  st->parent->numFreeHosts -= num;
  if (st->parent->numFreeHosts < 0) {
    cerr << "BUG: Underrun of st->parent->numFreeHosts on:" 
         << st->parent->name() << endl;
    exit(1);
  }
  L2ByLessUsage.insert(st->parent);

  L1ByLessUsage.insert(st);
}

int
ThreeLevelFatTree::delJobNodeAllocation(int job)
{
  // we track the per job member sub-tree ports in jobSubTreePorts
  // but note that for L2's that have a sub-tree allocated at L1 we
  // do not have that tracking so need to take care of it explicitly
  map<int, map<SubTree *, list<int> > >::iterator jI;
  jI = jobSubTreePorts.find(job);
  if (jI == jobSubTreePorts.end()) {
    cout << "-E- Could not find any allocated sub-tree ports for job:"
         << job << endl;
    exit(1);
  }

  int numAlloc = 0;
  map<SubTree *, list<int> >::iterator sI;
  for (sI = (*jI).second.begin(); sI != (*jI).second.end(); sI++) {
    SubTree *st = (*sI).first;
    list<int>::iterator pI;
    if (st->level != 1) {
      cerr << "BUG: we only track leafs in this implementation" << endl; 
      exit(1);
    }

    L1ByLessUsage.erase(st);
    L2ByLessUsage.erase(st->parent);

    for (pI = (*sI).second.begin(); pI != (*sI).second.end(); pI++) {
      int i = (*pI);
      st->dnPortJob[i] = 0;
      st->numFreeHosts++;
      st->parent->numFreeHosts++;

      numAlloc++;
    }

    // On L1 if entire L1 become available may need to 
    // cleanup parent - if it has -1 in the dnPortJob[i]
    if (st->numFreeHosts == st->numDnPorts) {
      // a -1 means that there will be no reg of any job for that port!
      if (st->parent->dnPortJob[st->idxInTree] == -1) {
        st->parent->dnPortJob[st->idxInTree] = 0;
      }
    }

    L2ByLessUsage.insert(st->parent);
    L1ByLessUsage.insert(st);
  } // every leaf participating
  return numAlloc;
}
