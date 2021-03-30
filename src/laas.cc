//////////////////////////////////////////////////////////////////////////////  
// 
//  Implementation of Links as a Service - Allocation Service
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

#include "laas.h"
using namespace std;

LaaS::LaaS(std::vector<int> M, std::vector<int> W, std::string logFileName)
{
	isBadState = false;
	// initialize the tree
	if (M.size() < 2) {
		lastErrorMsg << "Not enough M values provided";
		isBadState = true;
		return;
	}

	if (M.size() < 3) {
		M.push_back(1);
	}

	if (W.size() < 2) {
		lastErrorMsg << "Not enough W values provided";
		isBadState = true;
		return;
	}
	if (W[0] != 1) {
		lastErrorMsg << "W[0] must be zero!";
		isBadState = true;
	}

	ft  = new ThreeLevelFatTree(M[0], M[1], M[2], W[1], W[2]);
	if (!ft) {
		lastErrorMsg << "Could not create the fat tree.";
		isBadState = true;
		return;
	}

	// initialize the algorithm
	alg = new IsolAlgo("");
	if (!alg) {
		lastErrorMsg << "Could not create the Isolation Algorithm.";
		isBadState = true;
		return;
	}

	// initialize the log file
	log.open(logFileName.c_str());
	if (!log.good()) {
		lastErrorMsg << "Could not create the log file: " << logFileName;
		isBadState = true;
		return;
	}

	if (alg->setFatTree(ft)) {
		lastErrorMsg << "Could not set the fat tree by the isolation algorithm.";
		isBadState = true;
		return;
	}
}

	string
LaaS::getLastErrorMsg()
{
	string res = lastErrorMsg.str();
	lastErrorMsg.clear();
	return(res);
}

	int 
LaaS::replayLogFile()
{
	lastErrorMsg << "replayLogFile not yet implemented.";
	return 1;
	return(0);
}

	int 
LaaS::getTenantAlloc(int tenantId,
		std::vector<int> &hosts,                       // OUT allocated host indexies
		std::vector<std::pair<int, int> > &l1UpPorts,  // OUT allocated L1 switch up ports
		std::vector<std::pair<int, int> > &l2UpPorts)  // OUT allocated L2 switch up ports
{
	map<int, Job>::const_iterator I = jobByID.find(tenantId);
	if (I == jobByID.end()) {
		lastErrorMsg << "Could not find job:" << tenantId;
		return 1;
	}

	// mainly convert ports masks to indexes...
	map<int, map< int, list< int > > >::const_iterator jI;
	jI = alg->L1DnPortsByJob.find(tenantId);
	if (jI != alg->L1DnPortsByJob.end()) {
		map< int, list< int > >::const_iterator sI;
		for (sI = (*jI).second.begin(); sI != (*jI).second.end(); sI++) {
			int swIdx = (*sI).first;
			list< int >::const_iterator pI;
			for (pI = (*sI).second.begin(); pI != (*sI).second.end(); pI++) {
				int p = (*pI);
				unsigned int h = swIdx*ft->M1 + p;
				hosts.push_back(h);
			}
		}
	}

	jI = alg->L1UpPortsByJob.find(tenantId);
	if (jI != alg->L1UpPortsByJob.end()) {
		map< int, list< int > >::const_iterator sI;
		for (sI = (*jI).second.begin(); sI != (*jI).second.end(); sI++) {
			int swIdx = (*sI).first;
			list< int >::const_iterator pI;
			for (pI = (*sI).second.begin(); pI != (*sI).second.end(); pI++) {
				int p = (*pI);
				l1UpPorts.push_back(pair<int, int>(swIdx,p));
			}
		}
	}

	jI = alg->L2UpPortsByJob.find(tenantId);
	if (jI != alg->L2UpPortsByJob.end()) {
		map< int, list< int > >::const_iterator sI;
		for (sI = (*jI).second.begin(); sI != (*jI).second.end(); sI++) {
			int swIdx = (*sI).first;
			list< int >::const_iterator pI;
			for (pI = (*sI).second.begin(); pI != (*sI).second.end(); pI++) {
				int p = (*pI);
				l2UpPorts.push_back(pair<int, int>(swIdx,p));
			}
		}
	}

	return 0;
}

// try to allocate the tenant
	int
LaaS::allocTenant(int tenantId,
		std::vector<int> &GroupHosts)  // IN number of hosts per each leaf group index
{
	map<int, Job>::const_iterator jI = jobByID.find(tenantId);
	if (jI != jobByID.end()) {
		lastErrorMsg << "Cannot place an existing job:" << tenantId;
		return 1;
	}

	if (GroupHosts.size() != 1) {
		lastErrorMsg << "We currently support a single group, got: " 
			<< GroupHosts.size();
		return 1;
	}

	unsigned int N = GroupHosts[0];
	if (alg->placeJob(tenantId, N)) {
		Job job(tenantId, N);
		jobByID.insert(pair<int, Job>(tenantId, job));
		return 0;
	}
	return 1;
}

// remove a tenant
	int
LaaS::deallocTenant(int tenantId)
{
	map<int, Job>::const_iterator jI = jobByID.find(tenantId);
	if (jI == jobByID.end()) {
		lastErrorMsg << "Could not find job:" << tenantId;
		return 1;
	}
	jobByID.erase(tenantId);
	return(alg->completeJob(tenantId, (*jI).second.numHosts) == true);
}

// set the group each leaf
	int
LaaS::setLeafGroup(int group, std::vector<int> leafIdxs)
{
	lastErrorMsg << "setLeafGroup not yet implemented.";
	return 1;
	return 0;
}

// set the cost of hosts on each leaf
	int 
LaaS::setLeafCost(int group, std::vector<double> leafIdxs)
{
	lastErrorMsg << "setLeafCost not yet implemented.";
	return 1;
	return 0;
}

// get all active tenant ids
	int 
LaaS::getTenants(std::set<int> &tenantIds)
{
	tenantIds.clear();
	map<int, Job>::const_iterator jI;
	for (jI = jobByID.begin(); jI != jobByID.end(); jI++) {
		tenantIds.insert((*jI).first);
	}
	return 0;
}

// manually assign tenant resources
	int 
LaaS::assignTenant(int tenantId,
		std::vector<int> &hosts,                 // IN allocated host indexies
		std::vector<pair<int, int> > &l1UpPorts, // IN allocated L1 switch up ports
		std::vector<pair<int, int> > &l2UpPorts) // IN allocated L2 switch up ports
{
	// not very efficient but first validate entire request
	for (size_t i = 0; i < hosts.size(); i++) {
		unsigned l1Idx = hosts[i]/ft->M1;
		unsigned l1DnPn = hosts[i] % ft->M1;
		if (alg->L1FreeDnPorts.size() <= l1Idx) {
			lastErrorMsg << "Provided hosts[" << i << "] = " << hosts[i] 
				<< " maps to switch index:" << l1Idx <<  " which is > " 
				<< alg->L1FreeDnPorts.size()-1 << " thus out of range.";
			return 1;
		}
		if (!alg->L1FreeDnPorts[l1Idx].getBit(l1DnPn)) {
			lastErrorMsg << "Provided hosts[" << i << "] = " << hosts[i]
				<< " maps to switch index:" << l1Idx << " port: " << l1DnPn 
				<< " which is is not free."; 
			return 1;
		}
	}

	for (size_t i = 0; i < l1UpPorts.size(); i++) {
		unsigned l1Idx = l1UpPorts[i].first;
		unsigned l1UpPn = l1UpPorts[i].second;

		if(isolation==false){
			print "here1"
			if (alg->L1FreeUpPorts.size() <= l1Idx &&  alg->L1FreeUpPorts_v2.size() <= l1Idx) {
				lastErrorMsg << "Provided switch index of l1UpPorts[" << i << "] = switch: " 
					<< l1Idx <<  " which is > " 
					<< alg->L1FreeUpPorts.size()-1 << " thus out of range.";
				return 1;
			}
			if (!alg->L1FreeUpPorts[l1Idx].getBit(l1UpPn) && !alg->L1FreeUpPorts_v2[l1Idx].getBit(l1UpPn)  ) {
				lastErrorMsg << "Provided l1UpPorts[" << i << "] = " 
					<< " switch:" << l1Idx << " port: " << l1UpPn 
					<< " which is is not free."; 
				return 1;
			}
		}else{
			print "here2"

			if (alg->L1FreeUpPorts.size() <= l1Idx || alg->L1FreeUpPorts_v2.size() <= l1Idx) {
				lastErrorMsg << "Provided switch index of l1UpPorts[" << i << "] = switch: " 
					<< l1Idx <<  " which is > " 
					<< alg->L1FreeUpPorts.size()-1 << " thus out of range.";
				return 1;
			}
			if (!alg->L1FreeUpPorts[l1Idx].getBit(l1UpPn) || !alg->L1FreeUpPorts_v2[l1Idx].getBit(l1UpPn)) {
				lastErrorMsg << "Provided l1UpPorts[" << i << "] = " 
					<< " switch:" << l1Idx << " port: " << l1UpPn 
					<< " which is is not free."; 
				return 1;
			}
		}
	}

	for (size_t i = 0; i < l2UpPorts.size(); i++) {
		unsigned l2Idx = l2UpPorts[i].first;
		unsigned l2UpPn = l2UpPorts[i].second;
		if (alg->L2FreeUpPorts.size() <= l2Idx) {
			lastErrorMsg << "Provided switch index of l2UpPorts[" << i << "] = switch: " 
				<< l2Idx <<  " which is > " 
				<< alg->L2FreeUpPorts.size()-1 << " thus out of range.";
			return 1;
		}
		if (!alg->L2FreeUpPorts[l2Idx].getBit(l2UpPn)) {
			lastErrorMsg << "Provided l2UpPorts[" << i << "] = " 
				<< " switch:" << l2Idx << " port: " << l2UpPn 
				<< " which is is not free."; 
			return 1;
		}
	}

	// now do the job
	for (size_t i = 0; i < hosts.size(); i++) {
		unsigned l1Idx = hosts[i]/ft->M1;
		unsigned l1DnPn = hosts[i] % ft->M1;
		alg->L1FreeDnPorts[l1Idx].setBit(l1DnPn, false);
	}
	for (size_t i = 0; i < l1UpPorts.size(); i++) {
		unsigned l1Idx = l1UpPorts[i].first;
		unsigned l1UpPn = l1UpPorts[i].second;
		if (isolation==false){
			print "here3"

			if(alg->L1FreeUpPorts[l1Idx].getBit(l1UpPn)){
				alg->L1FreeUpPorts[l1Idx].setBit(l1UpPn, false);

			}else{
				alg->L1FreeUpPorts_v2[l1Idx].setBit(l1UpPn, false);
			}
		}else{
			print "here4"

			alg->L1FreeUpPorts[l1Idx].setBit(l1UpPn, false);
			alg->L1FreeUpPorts_v2[l1Idx].setBit(l1UpPn, false);

		}
	}
	for (size_t i = 0; i < l2UpPorts.size(); i++) {
		unsigned l2Idx = l2UpPorts[i].first;
		unsigned l2UpPn = l2UpPorts[i].second;
		alg->L2FreeUpPorts[l2Idx].setBit(l2UpPn, false);
	}

	return 0;
}

// get all currently unused switch ports
	int
LaaS::getUnAllocated(std::vector<int> &hosts,          // OUT Host IDs
		std::vector<pair<int, int> > &l1UpPorts, // OUT unallocated L1 switch up ports
		std::vector<pair<int, int> > &l2UpPorts) // OUT unallocated L2 switch up ports
{
	// mainly convert ports masks to indexes...
	for (size_t i = 0; i < alg->L1FreeDnPorts.size(); i++) {
		vector<unsigned int> setBits;
		size_t nSet = alg->L1FreeDnPorts[i].getBits(true, setBits);
		for (size_t j = 0; j < nSet; j++) {
			unsigned int h = i*ft->M1 + setBits[j];
			hosts.push_back(h);
		}
	}



	for (size_t i = 0; i < alg->L1FreeUpPorts.size(); i++) {
		vector<unsigned int> setBits;
		vector<unsigned int> setBits_v2;
		vector<unsigned int> setBits_result;
		setBits_result.clear();

		size_t nSet = alg->L1FreeUpPorts[i].getBits(true, setBits);
		size_t nSet_v2 = alg->L1FreeUpPorts_v2[i].getBits(true, setBits_v2);
		int flag_setBits=0;
		if(isolation==false){
			print "here5"

			for (size_t j1 = 0; j1 < setBits.size() ; j1++) {
				for(size_t j2=0 ; j2<setBits_v2.size() ;j2++){
					if(setBits[j1] == setBits_v2[j2]){
						flag_setBits=1;	
					}
					setBits_result.push_back(setBits_v2[j2]);
				}
				if(flag_setBits==0){
					setBits_result.push_back(setBits[j1]);
				}
				flag_setBits=0;
			}
		}else{
			print "here6"

			for (size_t j1 = 0; j1 < setBits.size() ; j1++) {
				for(size_t j2=0 ; j2<setBits_v2.size() ;j2++){
					if(setBits[j1] == setBits_v2[j2]){
						setBits_result.push_back(setBits_v2[j2]);
					}
				}
			}
		}
		for (size_t j = 0; j < nSet; j++) {
			l1UpPorts.push_back(pair<int, int>(i,setBits_result[j]));
		}

		for (size_t i = 0; i < alg->L2FreeUpPorts.size(); i++) {
			vector<unsigned int> setBits;
			size_t nSet = alg->L2FreeUpPorts[i].getBits(true, setBits);
			for (size_t j = 0; j < nSet; j++) {
				l2UpPorts.push_back(pair<int, int>(i,setBits[j]));
			}
		}

		return 0;
	}
}

	int
LaaS::setVerbose(bool v)
{
	verbose = v;
	alg->verbose = v;
	return 0;
}
