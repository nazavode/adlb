#!/usr/bin/env python

# tag = re.findall(r'.*tag (\d+).*',line)[0]

import sys, re

global ntypes, nappranks, userTypes

userTypes = {}

def handle_statinfo(timeStamp,statinfo):
    global ntypes, nappranks, userTypes
    wqinfo = []
    for i in range(ntypes):
        wqinfo.append([])
        for j in range(nappranks+1):
            wqinfo[i].append( statinfo[i*(nappranks+1)+j] )
    next = ntypes * (nappranks + 1)
    rqinfo = statinfo[ next : next+(ntypes+2) ]
    next += ntypes + 2
    putinfo = statinfo[ next : next+ntypes ]
    next += ntypes
    rrinfo = statinfo[ next : next+ntypes ]
    # process the stats
    onwq = 0
    for i in range(len(wqinfo)):
        for j in range(len(wqinfo[i])):
            onwq += wqinfo[i][j]
    onrq = rqinfo[-1]  # last val
    put_tot = 0
    for e in putinfo:
        put_tot += e
    rr_tot = 0
    for e in rrinfo:
        rr_tot += e
    print "%f:  num on rq = %d  num on wq = %d" % (timeStamp,onrq,onwq),
    print "  num puts = %d  num resolved reserves = %d" % (put_tot,rr_tot)
    if onrq > 0:
        print "    rqinfo:"
        lenrq = len(rqinfo) - 1
        for i in range(lenrq):
            if rqinfo[i] <= 0:
                continue
            if i < (lenrq-1):
                printType = userTypes[i]
            else:
                printType = -1
            print "        type = %5d  num = %6d" % (printType,rqinfo[i])
    if onwq > 0:
        print "    wqinfo:"
        for i in range(len(wqinfo)):
            lenwqi = len(wqinfo[i])
            unoSum = 0
            anySum = 0
            for j in range(lenwqi):
                if wqinfo[i][j] <= 0:
                    continue
                if j < (lenwqi-1):
                    unoSum += wqinfo[i][j]
                else:
                    anySum += wqinfo[i][j]
            if anySum > 0  or  unoSum > 0:
                print "        type = %5d  target = any  num = %5d," % (userTypes[i],anySum),
                print " target = uno  num = %5d" % (unoSum)
    if put_tot > 0:
        print "    putinfo:"
        for i in range(len(putinfo)):
            if putinfo[i] > 0:
                print "        type = %5d  num = %6d" % (userTypes[i],putinfo[i])
    if rr_tot > 0:
        print "    rrinfo:"
        for i in range(len(rrinfo)):
            if rrinfo[i] > 0:
                print "        type = %5d  num = %6d" % (userTypes[i],rrinfo[i])
            

sf = open(sys.argv[1])
print "reading ..."
lines = sf.readlines()
statinfo = ''
print "processing ..."
for line in lines:
    if 'APS:' not in line:
        continue
    if 'INFO_APS:' in line:
        sline = line.split(' ')
        for x in sline:
            if 'ntypes' in x:
                (k,v) = x.split('=')
                ntypes = int(v)
            elif 'nappranks' in x:
                (k,v) = x.split('=')
                nappranks = int(v)
            elif 'type_idx' in x:
                (k,v) = x.split('=')
                type_idx = int(v)
            elif 'type_val' in x:
                (k,v) = x.split('=')
                userTypes[type_idx] = int(v)
    elif 'STAT_APS:' in line:
        line = line[:-1]  # ONLY strip newline
        sline = line.split(': ')
        timeStamp = float(sline[3])
        lct = sline[5]
        slct = lct.split('=')
        if slct[1] == '0':
            if statinfo:
                statinfo = statinfo.rstrip()
                statinfo = statinfo.split(' ')
                statinfo = [ int(x) for x in statinfo ]
                handle_statinfo(timeStamp,statinfo)
            statinfo = sline[6]
        else:
            statinfo += sline[6]
    else:
        print '**** unrecognized aps line:', line.rstrip()

