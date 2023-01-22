import sys
import numpy as np
import matplotlib.pyplot as plt
import scipy.optimize as scipy

file = open("output.txt", 'r')
times = []
periods = []
rpms = []
for line in file.readlines():
    time, period = map(int, line.split(','))
    times.append(time)
    periods.append(period)
    rpms.append(60000000.0 / period)

for i in range(1, len(times)):
    if times[i] < times[i - 1]:
        print("%d - %d" % (times[i - 1], times[i]))

def log_func(x, a, b):
    return a * np.log(x + b)

def poly_func(x, a):
    return a

def smooth_rpm_poly(times, rpms):
    n = len(times)
    result = [0] * n
    coef, steps = scipy.curve_fit(poly_func, times, rpms)
    for i in range(n):
        result[i] = poly_func(times[i], *coef)
    return result

def window_fit(times, rpms):
    n = len(times)
    result = [0] * n
    w = 32
    last = rpms[0]
    for i in range(n):
        start = max(0, i - 16)
        end = min(n, i + 16)
        # coef, steps = scipy.curve_fit(poly_func, times[start:end], rpms[start:end])
        last = (rpms[i] + 15 * last) / 16
        result[i] = last #sum(rpms[start:end]) / (end -start) # poly_func(times[i], *coef)
    return result

srpms = window_fit(times, rpms)
#plt.plot(times, periods, 'b.')
plt.plot(times, rpms, 'b.')
plt.plot(times, srpms, 'r')
plt.show()


