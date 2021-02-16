#!/bin/bash
#
# Adding tenant 4 to OpenStack
#
echo Adding tenant 4 to OpenStack > OSCfg/cmd-1.log
keystone tenant-create --name laas-tenant-4 --description "LaaS Tenant 4" >> OSCfg/cmd-1.log
tenantId=`keystone tenant-get laas-tenant-4 | awk '/ id /{print $4}'` >> OSCfg/cmd-1.log
nova aggregate-create laas-aggr-4 >> OSCfg/cmd-1.log
nova aggregate-set-metadata laas-aggr-4 filter_tenant_id=$tenantId >> OSCfg/cmd-1.log
nova aggregate-add-host laas-aggr-4 comp-1 >> cmd-1.log
nova aggregate-add-host laas-aggr-4 comp-2 >> cmd-1.log
nova aggregate-add-host laas-aggr-4 comp-3 >> cmd-1.log
nova aggregate-add-host laas-aggr-4 comp-4 >> cmd-1.log
nova aggregate-add-host laas-aggr-4 comp-5 >> cmd-1.log
nova aggregate-add-host laas-aggr-4 comp-6 >> cmd-1.log
nova aggregate-add-host laas-aggr-4 comp-7 >> cmd-1.log
nova aggregate-add-host laas-aggr-4 comp-8 >> cmd-1.log
nova aggregate-add-host laas-aggr-4 comp-9 >> cmd-1.log
nova aggregate-add-host laas-aggr-4 comp-10 >> cmd-1.log
