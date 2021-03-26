##############################################################################  
# 
#  A Python Implementation for LaaS simulation
#
#  Copyright (C) 2014 TO THE AUTHORS
#  Copyright (C) 2014 TO THE AUTHORS
#
# to the blind review this software is available for SigComm evaluation 
# only. Once accepted it will be published with chioce of GPLv2 and BSD
# licenses.  
# You are not allowed to copy or publish this software in any way.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
##############################################################################

import sys
import getopt
import re
import os
import laas
import heapq

from time import time

class Job(object):
    def __init__(self, id, N, arrival, length):
        self.id = id
        self.N = N
        self.arrival = arrival
        self.length = length
        self.start = -1

    def __str__(self):
        return "job id=%d N=%d len=%d start= %d" % \
            (self.id, self.N, self.length, self.start)

    def start(self, t):
        self.start = t

# times = a heap to store the times
# jobsByTime = a map from time to list of jobs 
class RunningJobs:
    def __init__(self):
        self.times = []
        heapq.heapify(self.times)
        self.jobsByTime = {}

    def len(self):
        return len(self.times)

    def insert(self, t, job):
        job.start = t
        if self.jobsByTime.has_key(t):
            self.jobsByTime[t].append(job)
        else:
            self.jobsByTime[t] = [job]
            heapq.heappush(self.times, t)

    # return the min time and the list of jobs at that time
    def pop(self):
        if len(self.times) == 0:
            return -1,[]
        t = heapq.heappop(self.times)
        jobs = self.jobsByTime[t]
        del self.jobsByTime[t]
        return t,jobs

    def minT(self):
        if len(self.times) == 0:
            return None
        return self.times[0]
        
class SimFailure(Exception):
    def __init__(self, msg):
        self.msg = msg
        print "Fail to handle some Simulation condition: %s" % msg

def cmp_by_arrival_n_id(j1,j2):
    a = j1.arrival - j2.arrival
    d = j1.id - j2.id
    if a != 0:
        return a
    return d

# we keep pending jobs in 2 lists: pending and running
# pending is sorted by arrival and running sorted by start+length
class Sim(object):
    def __init__(self,M,W,jobsCsv):
        self.M = M
        self.W = W
        self.parseJobsCsv(jobsCsv)        
        self.pending = sorted(self.jobs, cmp=cmp_by_arrival_n_id)
        self.running = RunningJobs()
        self.engine = laas.LaaS(M, W, "logile")
        if not self.engine.good():
            raise SimFailure(self.engine.getLastErrorMsg())
        self.engine.setVerbose(verbose);
        self.lastJobPlacement = -1
        self.firstJobWaiting = -1
        self.runTime = 0
    def parseJobsCsv(self,csvFileName):
        jobRex = re.compile('^(\d+)[\s,]+(-?(\d+))[\s,]+(\d+)[\s,]+(\d+)\s*$')
        self.jobs = []
        lineNum = 0
        with open(csvFileName) as f:
            for line in f:
                lineNum += 1
                m = jobRex.match(line)
                if m is not None:
                    id = int(m.group(1))
                    N = int(m.group(2))
                    arrival = int(m.group(4))
                    length = int(m.group(5))
                    print "N is ",arrival
                    self.jobs.append(Job(id,N,arrival,length));
                else:
                    print "-W- parseJobsCsv: ignore line: (%d) %s" % \
                       (lineNum , line)
        print "-I- Obtained %d jobs" % len(self.jobs)
        return 0

    def run(self):
        # we need to loop until no pending jobs or fail to place 
        # we have two sorted containers: 
        # pending - by arrival time
        # running - by finish time (this is a heapq by time to list of jobs)
        # 
        # Whenever we advance time t we need to first clear all ended jobs
        # then we need to try placing all pending jobs arriving before t
        #
        # We advance time when either:
        # 1. Some pending job can't be placed: advance to next ending job
        # 2. No pending jobs at the time: advance to next pending job time
        #
        t0 = time()
        t = 0
        grps = laas.VecInt(1)
        while len(self.pending):
            if verbose: 
                print "-I- At t=%d" % t

            # first clean all running jobs
            endT = self.running.minT() 
            while endT != None and endT <= t:
                endT,endingJobs = self.running.pop()
                for j in endingJobs:
                    # note the inverse in semantics...
                    if not self.engine.deallocTenant(j.id):
                        raise SimFailure(self.engine.getLastErrorMsg())
                    else:
                        if verbose: 
                            print "-V- Remove %s" % j
                endT = self.running.minT() 
                
            # now try placement of all jobs that already arrived
            j = self.pending[0]
            placeFailed = False
            while not placeFailed and j.arrival <= t:
                grps[0] = j.N
                # the engine returns 0 for success!
                placeFailed = self.engine.allocTenant(j.id, grps)
                if placeFailed:
                    if not self.running.len():
                        err = "No job running but fail to place job %s" % j
                        raise SimFailure(err)
                    else:
                        if self.firstJobWaiting < 0 and t > 0:
                            self.firstJobWaiting = t
                        if verbose: 
                            print "-V- Fail   %s" % j
                else:
                    # was placed:
                    hosts = laas.VecInt()
                    l1Up = laas.VecPairInt()
                    l2Up = laas.VecPairInt()
                    alloc = self.engine.getTenantAlloc(j.id, hosts, l1Up, l2Up)
                    j.actNumHosts = hosts.size()
                    j.numL1UpLinks = l1Up.size()
                    j.numL2UpLinks = l2Up.size()*self.W[1]
                    if verbose: 
                        print "-V- Place  %s %d hosts %d L1 %d L2" % \
                            (j, j.actNumHosts, j.numL1UpLinks, j.numL2UpLinks) 
                    self.running.insert(t+j.length, j)
                    j.start = t
                    self.lastJobPlacement = t
                    self.pending.pop(0)
                    if len(self.pending):
                        j = self.pending[0]
                    else:
                        placeFailed = True

            # we can reach here on two cases either no more ready jobs or 
            # failure to place
            if placeFailed:
                # advance to next ending job time
                t = self.running.minT()
            else:
                t = self.pending[0].arrival
        t1 = time()
        self.runTime = t1 - t0
        return 0

    def analyze(self):
        if self.firstJobWaiting < 0:
            print "-E- Can't provide analysis when no job waited"
            return 1
        print "-I- first waiting job at: %d lastJobPlacementTime %d" % \
            (self.firstJobWaiting, self.lastJobPlacement)

        numHosts = self.M[0] * self.M[1] * self.M[2]
        numL1UpLinks = self.M[2] * self.M[1] * self.W[1]
        numL2UpLinks = self.M[2] * self.W[1] * self.W[2]
        totalTime = 1.0*(self.lastJobPlacement - self.firstJobWaiting)
        potentialHostTime = totalTime * numHosts
        actualHostTime = 0.0
        potentialL1UpLinksTime = totalTime * numL1UpLinks
        actualL1UpLinksUsed = 0.0
        potentialL2UpLinksTime = totalTime * numL2UpLinks
        actualL2UpLinksUsed = 0.0
        potentialTotalLinksTime = \
            potentialHostTime + potentialL1UpLinksTime + potentialL2UpLinksTime
        actualTotalLinksUsed = 0.0

        print "-I- Total potential hosts * time = %g " % potentialHostTime
        nJobs = 0
        skipLast = 0
        skipFirst = 0
        for j in self.jobs:
            runtime = j.length
            if j.start + j.length < self.firstJobWaiting:
                skipFirst = skipFirst + 1
                continue

            if j.start >= self.lastJobPlacement:
                skipLast = skipLast + 1
                continue

            #  for those starting before the first wait - take only the 
            #  time after the start
            if j.start < self.firstJobWaiting:
                runtime = j.start +runtime - self.firstJobWaiting
                
            # if crossing the stop point only take the runtime before
            if j.start + runtime > self.lastJobPlacement: 
                runtime = self.lastJobPlacement - j.start;

            nJobs = nJobs + 1

            actualHostTime += 1.0 * runtime * j.N
            actualL1UpLinksUsed += runtime * j.numL1UpLinks
            actualL2UpLinksUsed += runtime * j.numL2UpLinks;
            actualTotalLinksUsed += runtime * (j.actNumHosts + j.numL1UpLinks +  j.numL2UpLinks)
        # end of for
        print "-I- Total considered jobs: %d skip first: %d last: %d" % \
            (nJobs, skipFirst, skipLast)
        print "-I- Total actual hosts * time = %g" % actualHostTime
        hl =  (100.0*actualHostTime)/potentialHostTime
        print "-I- Host Utilization = %3.2f %%" % hl
        l1u = (100.0*actualL1UpLinksUsed)/potentialL1UpLinksTime
        print "-I- L1 Up Links Utilization  = %3.2f %%" % l1u
        l2u = (100.0*actualL2UpLinksUsed)/potentialL2UpLinksTime
        print "-I- L2 Up Links Utilization  = %3.2f %%" % l2u
        ltu = (100.0*actualTotalLinksUsed)/potentialTotalLinksTime
        print "-I- Total Links Utilization  = %3.2f %%" % ltu
        print "-I- Run Time = %3.3g sec" % self.runTime
        return 0

###############################################################################

class Usage(Exception):
    def __init__(self, msg):
        self.msg = msg

verbose = False

def main(argv=None):
    global verbose
    csvFileName = None
    M = laas.VecInt(3)
    W = laas.VecInt(3)

    if argv is None:
        argv = sys.argv
    try:
        try:
            opts, args = getopt.getopt(argv[1:], 
                                       "hvc:m:w:",
                                       ["help", "verbose", "csv=",
                                        "children=", "parents="])
        except getopt.error, msg:
             raise Usage(msg)
        for opt, arg in opts:
            if opt in ('-h', "--help"):
                print 'sim.py -c|--csv <csvFile> -m|--children m1,m2,m3' \
                    '-w|--parents 1,w2,w3'
                sys.exit()
            elif opt in ("-c", "--csv"):
                csvFileName = arg
            elif opt in ("-v", "--verbose"):
                verbose = True
            elif opt in ("-m", "--children"):
                i = 0
                for m in arg.split(','):
                    M[i] = int(m)
                    i += 1
            elif opt in ("-w", "--parents"):
                i = 0
                for w in arg.split(','):
                    W[i] = int(w)
                    i += 1
        if csvFileName is None or W is None or M is None:
            raise Usage("missing mandatory args")
        sim = Sim(M,W,csvFileName)
        sim.run()
        sim.analyze()

    except Usage, err:
        print >>sys.stderr, err.msg
        print >>sys.stderr, "for help use --help"
        return 2

    except SimFailure, err:
        print >>sys.stderr, "-F- " + err.msg
        return 3

if __name__ == "__main__":
    sys.exit(main())
