#!/usr/bin/env python

import sys, time, os, commands

x = commands.getoutput('svn update')
x = x.split('\n')[-1]   # split into lines; keep last line with revision number
if 'At revision' not in x:    #  e.g.: At revison 99.
    print 'cannot obtain old revision number'
    sys.exit(-1)
x = x.replace('.','')  # get rid of the trailing .
x = x.split()          # ['At', 'revision', 'NN']  so x[2] is the old number
newVersionNumber = int(x[2]) + 1

adlbch  = open('adlb.h')
tmpfile = open('tempch','w')
for line in adlbch:
    line = line.rstrip()
    if 'ADLB_VERSION_NUMBER' in line:
        newLine = '#define  ADLB_VERSION_NUMBER      %03d' % (newVersionNumber)
        print >>tmpfile, newLine
    elif 'ADLB_VERSION_DATE' in line:
        newLine = '#define  ADLB_VERSION_DATE        %s' % (time.strftime("%d-%b-%Y"))
        print >>tmpfile, newLine
    else:
        print >>tmpfile, line
tmpfile.close()
os.system("cp tempch adlb.h")
