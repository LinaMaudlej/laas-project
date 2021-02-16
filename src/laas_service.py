##############################################################################  
# 
#  A Python(2.7) Implementation for LaaS Service
#
#  Copyright (C) 2014 TO THE AUTHORS
#  Copyright (C) 2014 TO THE AUTHORS
#
# Due to the blind review this software is available for SigComm evaluation 
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
#
# How is this working?
#======================
#
# The parameters provided on the command line should include:
# -n|--name-map csv-mapping-idx-to-name
# -m|--children m1,m2,m3 
# -w|--parents 1,w2,w3
# -o|--open-stack
#
# On any change in tenant state (add/remove) the configuration file 
# for the SDN controller OpenSM is updated under directory SDNCfg
# Similarly, if the -o/--open-stack flag is provided on the command line,
# the creation/deletion of a tenant named "tenant_<id>"
# is configured via calls to nova client (assuming the current user
# config is available in current env).
# 
# We provide the following RESTFul API:
# GET /tenants
#   Return the allocated tenant ids
#
# GET /tenant/<id>
#   Return the general info about this tenant:
#   { "allocated": <time> 
#     "N": <num hosts requested> "A": <num hosts actual>
#     "l1UpLinks": <num L1 up links> "l2UpLinks": <num L2 up links> }
# DELETE /tenant/<id>
#   Deallocate the requested tenant id
# POST /tenant/<id> param N size
#   Allocate the requested numbder of nodes. 
#   Return the new tenant id or -1 if failed
#    
# GET /tenant/<id>/hosts
#   Return the names of allocated hosts
# GET /tenant/<id>/links/l0Ports
#   Return the names of allocated L1 down links
# GET /tenant/<id>/links/l1Ports
#   Return the names of allocated L1 up links
# GET /tenant/<id>/links/l2Ports
#   Return the names of allocated L2 up links
#
##############################################################################
#
# Example usage:
#
# 1. Get all tenants:
# curl http://localhost:12345/tenant
#
# 2. Create a new tenant of id=4 and size=100
# curl http://localhost:12345/tenants -d "id=4" -d "n=100" -X POST 
#
# 3. Delete a tenant
# curl http://localhost:12345/tenants -d "id=6" -X DELETE
#
# 4. Get list of allocated host for tenant 3
# curl http://localhost:12345/tenants/3/hosts
#
# 5. Get list of allocated L1 Down Ports for tenant 3
# curl http://localhost:12345/tenants/3/l0ports
#
# 6. Get list of allocated L1 Up Ports for tenant 3
# curl http://localhost:12345/tenants/3/l1ports
#
# 7. Get list of allocated L2 Up Ports for tenant 3
# curl http://localhost:12345/tenants/3/l2ports
#
##############################################################################

import sys
import getopt
import re
import os
import laas
import heapq

from time import time
from flask import Flask
#from flask.ext.restful import reqparse, abort, Api, Resource
from flask_restful import reqparse, abort, Api, Resource


##############################################################################
#
# Global Data Structure
#

# Keep up and down ports by key = [lvl, sIdx, pIdx].__str__()
# level, switch index (in the level) and port index
upPortByLvlPort = {}
dnPortByLvlPort = {}

# Keep node name by level and switch/host index
nameByLevelIdx = {}

##############################################################################
app = Flask(__name__)
api = Api(app)

TENANTS = {}

def abort_if_tenant_doesnt_exist(tenant_id):
    if tenant_id not in TENANTS:
        abort(404, message="Tenant {} doesn't exist".format(tenant_id))

def abort_if_tenant_exist(tenant_id):
    if tenant_id in TENANTS:
        abort(404, message="Tenant {} alreadt exist".format(tenant_id))

parser = reqparse.RequestParser()
parser.add_argument('id', type=int, help='the tenant id requested')
parser.add_argument('n', type=int, help='number of hosts required')

# Tenant
#   show a single tenant item and lets you delete them
class Tenant(Resource):
    def get(self, tenant_id):
        abort_if_tenant_doesnt_exist(tenant_id)
        return TENANTS[tenant_id]

    def delete(self, tenant_id):
        global engine
        abort_if_tenant_doesnt_exist(tenant_id)
        delOpenStackTenant(tenant_id)
        if  engine.deallocTenant(tenant_id):
            del TENANTS[tenant_id]
            updateSDNConf()
            return "Deleted tenant {}".format(tenant_id), 204
        else:
            abort(405, message="Fail to delete tenant {}".format(tenant_id))

# TenantList
#   shows a list of all tenants, and lets you POST to add new tasks
class TenantList(Resource):
    def get(self):
        return TENANTS

    def post(self):
        global W
        args = parser.parse_args()
        tenant_id = args['id']
        abort_if_tenant_exist(tenant_id)
        g = laas.VecInt(1)
        g[0] = args['n']
        if not engine.allocTenant(tenant_id, g):
            hosts = laas.VecInt()
            l1UpPorts = laas.VecPairInt()
            l2UpPorts = laas.VecPairInt()
            engine.getTenantAlloc(tenant_id, hosts, l1UpPorts, l2UpPorts)
            TENANTS[tenant_id] = {'N': args['n'], 'hosts': hosts.size(), 
                                  'l1Ports': l1UpPorts.size(), 'l2Ports': W[1]*l2UpPorts.size() }
            updateSDNConf()
            addOpenStackTenent(tenant_id)
            return TENANTS[tenant_id], 201
        else:
            abort(406, message="Fail to allocate tenant {}".format(tenant_id))

class TenantHosts(Resource):
    def get(self, tenant_id):
        global engine
        global nameByLevelIdx
        abort_if_tenant_doesnt_exist(tenant_id)
        hosts = laas.VecInt()
        l1UpPorts = laas.VecPairInt()
        l2UpPorts = laas.VecPairInt()
        if not engine.getTenantAlloc(tenant_id, hosts, l1UpPorts, l2UpPorts):
            hostNames = []
            for i in range(0, hosts.size()):
                j = hosts[i]
                key = [0, j].__str__()
                name = nameByLevelIdx[key]
                if name is None:
                    hostNames.append("UNDEF")
                else:
                    hostNames.append(name)
            return hostNames
        else:
            abort(407, message="Fail to get hosts for tenant {}".format(tenant_id))

class TenantL0Ports(Resource):
    def get(self, tenant_id):
        global engine
        global dnPortByLvlPort
        global M
        abort_if_tenant_doesnt_exist(tenant_id)
        hosts = laas.VecInt()
        l1UpPorts = laas.VecPairInt()
        l2UpPorts = laas.VecPairInt()
        if not engine.getTenantAlloc(tenant_id, hosts, l1UpPorts, l2UpPorts):
            portNames = []
            for i in range(0, hosts.size()):
                j = hosts[i]
                k = j / M[0]
                p = j % M[0]
                key = [1, k, p].__str__()
                name = dnPortByLvlPort[key]
                if name is None:
                    portNames.append("UNDEF")
                else:
                    portNames.append(name)
            return portNames
        else:
            abort(408, message="Fail to get L0 ports for tenant {}".format(tenant_id))

class TenantL1Ports(Resource):
    def get(self, tenant_id):
        global engine
        global upPortByLvlPort
        abort_if_tenant_doesnt_exist(tenant_id)
        hosts = laas.VecInt()
        l1UpPorts = laas.VecPairInt()
        l2UpPorts = laas.VecPairInt()
        if not engine.getTenantAlloc(tenant_id, hosts, l1UpPorts, l2UpPorts):
            portNames = []
            for i in range(0, l1UpPorts.size()):
                iNp = l1UpPorts[i]
                key = [1, iNp[0], iNp[1]].__str__()
                name = upPortByLvlPort[key]
                if name is None:
                    portNames.append("UNDEF")
                else:
                    portNames.append(name)
            return portNames
        else:
            abort(409, message="Fail to get L1 ports for tenant {}".format(tenant_id))

class TenantL2Ports(Resource):
    def get(self, tenant_id):
        global engine
        global upPortByLvlPort
        global W
        abort_if_tenant_doesnt_exist(tenant_id)
        hosts = laas.VecInt()
        l1UpPorts = laas.VecPairInt()
        l2UpPorts = laas.VecPairInt()
        if not engine.getTenantAlloc(tenant_id, hosts, l1UpPorts, l2UpPorts):
            portNames = []
            for i in range(0, l2UpPorts.size()):
                iNp = l2UpPorts[i]
                allocSwIdx = iNp[0]
                pn = iNp[1]
                for j in range(0, W[1]):
                    swIdx = allocSwIdx*W[1] + j;
                    key = [2, swIdx, pn].__str__()
                    name = upPortByLvlPort[key]
                    if name is None:
                        portNames.append("UNDEF")
                    else:
                        portNames.append(name)
            return portNames
        else:
            abort(410, message="Fail to get L2 ports for tenant {}".format(tenant_id))

##
## Actually setup the Api resource routing here
##
api.add_resource(TenantList, '/tenants')
api.add_resource(Tenant, '/tenants/<int:tenant_id>')
api.add_resource(TenantHosts, '/tenants/<int:tenant_id>/hosts')
api.add_resource(TenantL0Ports, '/tenants/<int:tenant_id>/l0Ports')
api.add_resource(TenantL1Ports, '/tenants/<int:tenant_id>/l1Ports')
api.add_resource(TenantL2Ports, '/tenants/<int:tenant_id>/l2Ports')
##############################################################################
#
# LaaS translation to names procs
# 
def parseNameMap(fileName):
    global upPortByLvlPort 
    global dnPortByLvlPort
    global nameByLevelIdx
    rex = re.compile('^([0-3]+),([0-9]+),(\S+),(UP([0-9,]+))(DN([0-9,]+))?$')
    lineNum = 0
    with open(fileName) as f:
        nUp = 0
        nDn = 0
        for line in f:
            lineNum += 1
            if line[0] == "#":
                continue
            m = rex.match(line)
            if m is not None:
                lvl = int(m.group(1))
                sIdx = int(m.group(2))
                sName = m.group(3)
                upPorts = m.group(4)
                dnPorts = m.group(6)
                if lvl > 3 or lvl < 0:
                    print("-E- Level %d out of range" % lvl)
                    continue
                key = [lvl, sIdx].__str__()
                nameByLevelIdx[key] = sName
                if upPorts is not None:
                    sg = map(lambda s:s.strip(), upPorts.split(','))
                    if sg.pop(0) != "UP":
                        print("-E- Head of up ports != UP in line:%s" % line)
                        continue
                    pIdx = 0
                    for pNum in sg:
                        if pNum != '':
                            key = [lvl, sIdx, pIdx].__str__()
                            upPortByLvlPort[key] = {'sName': sName, 'pNum': int(pNum)}
                            pIdx = pIdx + 1
                            nUp = nUp + 1
                if dnPorts is not None:
                    sg = map(lambda s:s.strip(), dnPorts.split(','))
                    if sg.pop(0) != "DN":
                        print("-E- Head of dn ports != DN in line:%s" % line)
                        continue
                    pIdx = 0
                    for pNum in sg:
                        if pNum != '':
                            key = [lvl, sIdx, pIdx].__str__()
                            dnPortByLvlPort[key] = {'sName': sName, 'pNum': int(pNum)}
                            pIdx = pIdx + 1
                            nDn = nDn + 1
                
            else:
                print("-W- parseNameMap: ignore line: (%d) %s" % \
                      (lineNum , line))
    print("-I- Defined %d up ports and %d down port mappings\n" % (nUp, nDn))
    return 0;

##############################################################################
#
# SDN Interface
# 

def deleteDirContent(dir):
    for f in os.listdir(dir):
        fPath = os.path.join(dir, f)
        try:
            if os.path.isfile(fPath):
                os.unlink(fPath)
        except (Exception, e):
            print(e)

# Analyze the tenant hosts and extract the PQFT definition for it
def getPQFTStr(tenant_id):
    return None

# Provide SDNConf files
def updateSDNConf():
    global engine
    global upPortByLvlPort
    # make sure dest dir exists
    directory='SDNCfg'
    if not os.path.exists(directory):
        os.makedirs(directory)

    # clear pervious settings
    deleteDirContent(directory)

    # now lets populate it again
    topoFileName = os.path.join(directory,'topologies.conf')
    T = open(topoFileName, 'w')
    if T is None:
        raise LaaSFailure("Cannot write topologies file: %s" % topoFileName)

    groupsFileName = os.path.join(directory,'groups.conf')
    G = open(groupsFileName, 'w')
    if G is None:
        raise LaaSFailure("Cannot write groups file: %s" % groupsFileName)

    chainsFileName = os.path.join(directory,'chains.conf')
    C = open(chainsFileName, 'w')
    if C is None:
        raise LaaSFailure("Cannot write chains file: %s" % chainsFileName)
    
    link=1
    topo=0
    C.write("unicast-step\nid: %d\ntopology: %d\nengine: updn\npath-bit: 0\nend-unicast-step\n\n"%(link,topo))

    # now go over all tenants
    tenants = laas.SetInt()
    engine.getTenants(tenants)
    for id in tenants:
        link = link + 1
        # topologies 
        T.write("topology\n")
        T.write("id: %d\n" % id)
        T.write("sw-grp: T%d-switches\n" % id)
        T.write("hca-grp: T%d-hcas\n" % id)
        T.write("end-topology\n\n")
        
        # PQFT (?)
        pqftOpts = "T%d.opensm.conf" % id
        pqftOptsFileName = os.path.join(directory,pqftOpts)
        O = open(pqftOptsFileName, 'w')
        if C is None:
            raise LaaSFailure("Cannot write options file: %s" % pqftOptsFileName)
        pqftStr = getPQFTStr(id)
        if pqftStr is not None:
            O.write("pqft_structure %s\n" % pqftStr)
        O.close()
        
        # Chain link
        C.write("unicast-step\n")
        C.write("id: %d\n" % link)
        C.write("topology: %d\n" % id)
        if pqftStr is not None:
            C.write("engine: pqft\n")
        else:
            C.write("engine: updn\n")
        C.write("config: %s\n" % pqftOptsFileName)
        C.write("path-bit: 0\n")
        C.write("end-unicast-step\n\n")

        hosts = laas.VecInt()
        l1UpPorts = laas.VecPairInt()
        l2UpPorts = laas.VecPairInt()
        if engine.getTenantAlloc(id, hosts, l1UpPorts, l2UpPorts):
            raise LaaSFailure("Cannot get tenant %d information" % id)

        # PORT GROUPS
        G.write("port-group\n")
        G.write("name: T%d-hcas\n" % id)
        G.write("obj_list: ")
        for i in range(0, hosts.size()):
            j = hosts[i]
            key = [0, j, 0].__str__()
            nameNPort = upPortByLvlPort[key] # = {'sName': sName, 'pNum': pNum}
            if nameNPort is not None:
                G.write("\n   name=%s/U1:P%d" % (nameNPort['sName'], nameNPort['pNum']))
        G.write(";\n")
        G.write("end-port-group\n\n")

        # collapse switch used ports to masks
        SW_PORTS = {}
        for i in range(0, l1UpPorts.size()):
            iNp = l1UpPorts[i]
            key = [1, iNp[0], iNp[1]].__str__()
            nameNPort = upPortByLvlPort[key]
            if nameNPort is not None:
                name = nameNPort['sName']
                pn = nameNPort['pNum']
                mask = 1 << pn
                if name not in SW_PORTS:
                    SW_PORTS[name] = mask
                else:
                    SW_PORTS[name] = SW_PORTS[name] | mask
        for i in range(0, l2UpPorts.size()):
            iNp = l1UpPorts[i]
            key = [1, iNp[0], iNp[1]].__str__()
            nameNPort = upPortByLvlPort[key]
            if nameNPort is not None:
                name = nameNPort['sName']
                pn = nameNPort['pNum']
                mask = 1 << pn
                if name not in SW_PORTS:
                    SW_PORTS[name] = mask
                else:
                    SW_PORTS[name] = SW_PORTS[name] | mask

        # now write the group
        G.write("port-group\n")
        G.write("name: T%d-switches\n" % id)
        if len(SW_PORTS) > 0:
            G.write("obj_list: ")
            for name in SW_PORTS:
                mask = SW_PORTS[name]
                G.write("\n   name=%s/U1 pmask=0x%x" % (name, mask))
            G.write(";\n")
        G.write("end-port-group\n\n")

    T.close()
    C.close()
    G.close()
    return 0

##############################################################################
# 
# Interaction with OpenStack
# 

# Execute a generated command file or generate an exception
def safeExecute(fileName):
    os.chmod(fileName, 755);
    p = subprocess.Popen(fileName)
    p.wait()
    return 0

# We write action files into the directory OSCfg 
# They should be played in order.

# create the tenant and then populate it
def addOpenStackTenent(id):
    global engine
    global osCmdId
    
    directory = 'OSCfg'
    if not os.path.exists(directory):
        os.makedirs(directory)

    osCmdId = osCmdId + 1
    cmdName = "cmd-%d.sh" % osCmdId
    logName = "cmd-%d.log" % osCmdId
    fileName = os.path.join(directory,cmdName)
    F = open(fileName, 'w')
    if F is None:
        raise LaaSFailure("Cannot write OS cmd file: %s" % fileName)
    logFile = os.path.join(directory,logName)

    F.write("#!/bin/bash\n")
    F.write("#\n# Adding tenant %d to OpenStack\n#\n" % id)
    aggrName = "laas-aggr-%d" % id
    tenantName = "laas-tenant-%d" % id
    F.write("echo Adding tenant %d to OpenStack > %s\n" % (id, logFile))
    F.write("keystone tenant-create --name %s --description \"LaaS Tenant %d\" >> %s\n" % \
            (tenantName, id, logFile))
    F.write("tenantId=`keystone tenant-get %s | awk '/ id /{print $4}'` >> %s\n" % \
            (tenantName, logFile))
    F.write("nova aggregate-create %s >> %s\n" % (aggrName, logFile))
    # HACK for now we assume no need for key
    F.write("nova aggregate-set-metadata %s filter_tenant_id=$tenantId >> %s\n" % (aggrName, logFile))

    hosts = laas.VecInt()
    l1UpPorts = laas.VecPairInt()
    l2UpPorts = laas.VecPairInt()
    if engine.getTenantAlloc(id, hosts, l1UpPorts, l2UpPorts):
        raise LaaSFailure("Cannot get tenant %d information" % id)

    for i in range(0, hosts.size()):
        j = hosts[i]
        key = [0, j, 0].__str__()
        nameNPort = upPortByLvlPort[key] # = {'sName': sName, 'pNum': pNum}
        if nameNPort is not None:
            F.write("nova aggregate-add-host %s %s >> %s\n" % (aggrName, nameNPort['sName'], logName))
    F.close()
    if callOpenStack:
        safeExecute(fileName)
    return 0

def delOpenStackTenant(id):
    global engine
    global osCmdId
    global callOpenStack

    hosts = laas.VecInt()
    l1UpPorts = laas.VecPairInt()
    l2UpPorts = laas.VecPairInt()
    if engine.getTenantAlloc(id, hosts, l1UpPorts, l2UpPorts):
        raise LaaSFailure("Cannot get tenant %d information" % id)
    
    directory = 'OSCfg'
    if not os.path.exists(directory):
        os.makedirs(directory)

    osCmdId = osCmdId + 1
    cmdName = "cmd-%d.sh" % osCmdId
    logName = "cmd-%d.log" % osCmdId
    fileName = os.path.join(directory,cmdName)
    F = open(fileName, 'w')
    if F is None:
        raise LaaSFailure("Cannot write OS cmd file: %s" % fileName)
    logFile = os.path.join(directory,logName)

    F.write("#!/bin/bash\n")
    F.write("#\n# Removing tenant %d from OpenStack\n#\n" % id)
    aggrName = "laas-aggr-%d" % id
    tenantName = "laas-tenant-%d" % id

    for i in range(0, hosts.size()):
        j = hosts[i]
        key = [0, j, 0].__str__()
        nameNPort = upPortByLvlPort[key] # = {'sName': sName, 'pNum': pNum}
        if nameNPort is not None:
            F.write("nova aggregate-remove-host %s %s >> %s\n" % (aggrName, nameNPort['sName'], logName))
    F.write("nova aggregate-delete %s >> %s\n" % (aggrName, logFile))
    F.write("keystone tenant-delete %s >> %s\n" % (tenantName, logFile))
    F.close()
    if callOpenStack:
        safeExecute(fileName)
    return 0

##############################################################################
class LaaSFailure(Exception):
    def __init__(self, msg):
        self.msg = msg
        print("Fail to handle some Simulation condition: %s" % msg)

###############################################################################

class Usage(Exception):
    def __init__(self, msg):
        self.msg = msg

verbose = False

def main(argv=None):
    global verbose
    global engine
    global M
    global W
    global osCmdId
    global callOpenStack

    callOpenStack = False
    osCmdId = 0
    csvFileName = None
    M = laas.VecInt(3)
    W = laas.VecInt(3)

    if argv is None:
        argv = sys.argv
    else:
        try:
            opts, args = getopt.getopt(argv[1:], "hvn:m:w:o",
                                       ["help", "verbose", "names=", "children=", "parents=", "openstack"])
        except getopt.GetoptError as err:
             raise Usage(str(err))
        for opt, arg in opts:
            if opt in ('-h', "--help"):
                print('sim.py -n|--names <nameMap.csv> -m|--children m1,m2,m3' \
                    '-w|--parents 1,w2,w3')
                sys.exit(0)
            elif opt in ("-n", "--names"):
                csvFileName = arg
            elif opt in ("-v", "--verbose"):
                verbose = True
            elif opt in ("-o", "--openstack"):
                callOpenStack = True
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
        missingArgs = ""
        if csvFileName is None:
            missingArgs = "-n "
        if W is None:
            missingArgs = missingArgs + "-w "
        if M is None:
            missingArgs = missingArgs + "-m "
        if missingArgs != "":
            raise Usage("missing mandatory args %s" % missingArgs)
        # start the LaaS service with the given parameters
        engine = laas.LaaS(M, W, "logile")
        if not engine.good():
            raise LaaSFailure(engine.getLastErrorMsg())
        engine.setVerbose(verbose)
        if parseNameMap(csvFileName):
            raise LaaSFailure("Fail to parse name map file: %s", csvFileName)
        # start the server
        app.run(port=12345, debug=True)

if __name__ == "__main__":
    sys.exit(main())

