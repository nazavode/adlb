#!/usr/bin/env python

import sys, os, commands

def guess_machine():
    # may also peek into /proc/cpuinfo
    machine = commands.getoutput('uname -m')
    if 'ppc64' in machine:
        return 'bg'
    elif 'mips64' in machine:
        return 'sicortex'
    elif '86' in machine:
        return 'cluster'
    else:
        return 'unknown'

def fix_adlb_conf_h(machine):
    os.system('cp adlb_conf.h .tempin')
    infile = open('.tempin','r')
    inlines = infile.readlines()
    infile.close()
    outfile = open('adlb_conf.h','w')
    outlines = []
    for line in inlines:
        if 'F77_NAME_LOWER ' in line:    # note blank in string
            if machine == 'bg':
                outlines.append('#define F77_NAME_LOWER 1\n')
            else:
                outlines.append(line)
        elif 'F77_NAME_LOWER_2USCORE ' in line:    # note blank in string
            if machine == 'sicortex':
                outlines.append('#define F77_NAME_LOWER_2USCORE 1\n')
            else:
                outlines.append(line)
        else:
            outlines.append(line)
    outfile.writelines(outlines)
    outfile.close()
    os.system('rm .tempin')

if __name__ == '__main__':
    machine = guess_machine()
    if machine == 'unknown':
        print '** unknown machine type'
        sys.exit(-1)
    # do backups
    x = commands.getoutput('ls Makefile.bak')
    if 'No such file' in x:
        os.system('cp Makefile Makefile.bak')
    x = commands.getoutput('ls adlb_conf.h.bak')
    if 'No such file' in x:
        os.system('cp adlb_conf.h adlb_conf.h.bak')
    print "configuring for machine:", machine
    # do Makefile
    print "creating Makefile for", machine
    if machine == 'bg':
        os.system('cp Makefile.bgp Makefile')
    elif machine == 'sicortex':
        os.system('cp Makefile.sicortex Makefile')
    elif machine == 'cluster':
        os.system('cp Makefile.cluster Makefile')
    # do adlb_conf.h
    print "creating adlb_conf.h for", machine
    fix_adlb_conf_h(machine)
