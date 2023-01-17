import sys
import numpy as np
import matplotlib.pyplot as plt

file = open("output.txt", 'r')
times = []
periods = []
rpms = []
for line in file.readlines():
    time, period = map(int, line.split(','))
    times.append(time / 1000000.0)
    periods.append(period)
    rpms.append(60000000.0 / period)
    #print("%i,%i" % (time, period))

plt.plot(times, periods, '.')
plt.show()


