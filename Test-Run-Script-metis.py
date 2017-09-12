from subprocess import call
from submitFileWrapper import *

import os
import math
import random

iterations = 7
dimension = 2

dirString = os.path.expanduser("/gpfs/work/pr87si/di36sop")

for i in range(iterations):
        graphNumber = i
        formatString = '%02d' % graphNumber
        p = 16*2**graphNumber
        filename = os.path.join(dirString, "refinedtrace-"+'000'+formatString+".graph")

        commandString = "metisWrapper" + " --graphFile " + str(filename) + " --dimensions 2"

        if not os.path.exists(filename):
                print(filename + " does not exist.")
        else:
            submitfile = createLLSubmitFile("llsub-"+str(p)+"-"+str(i)+"-metis.cmd", commandString, "00:10:00", p, "4000mb")
            call(["llsubmit", submitfile])

                

