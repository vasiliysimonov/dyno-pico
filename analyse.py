import math
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


temps = [18.3, 25.0, 25.0, 30.0, 30.0, 35.0, 35.0, 40.0, 40.0, 45.0, 45.0, 50.0, 55.0, 60.0, 65.0, 70.0]
volts = [1.784, 1.446, 1.467, 1.218, 1.229, 1.013, 1.03, 0.845, 0.89, 0.723, 0.764, 0.621, 0.485, 0.444, 0.381, 0.343]

def tempF(volt, r, a, b, c):
    try:
        #r0 = 10000.
        #t0 = 25. + 273.15
        #rt = 10000 * volt / (3.3 - volt)
        #invT = 1. / t0 + math.log(rt / r0) / b
        #return 1. / invT - 273.15
        rl = math.log(r * volt / (3.3 - volt))
        return a + b*rl + c*rl**3
        #return 1 / y - 273.15
    except:
        print("%s,%s,%s,%s,%s" % (volt, r, a, b, c))
        pass

def jacobF(volt, r, a, b, c):
    rlog = math.log(r * volt / (3.3 - volt))
    dr = (b + 3*c*rlog**2)/r
    da = 1
    db = rlog
    dc = rlog**3
    return [dr, da, db, dc]

def tempFV(volt, r, a, b, c):
    return [tempF(v, r, a, b, c) for v in volt]

def jacobFV(volt, r, a, b, c):
    return [jacobF(v, r, a, b, c) for v in volt]


invTemps = [1 / (t + 273.15) for t in temps]
coef, steps = scipy.curve_fit(tempFV, volts, invTemps,
                              p0=[10000., 0.01, 0.001, 0.0000001],
                              jac=jacobFV,
                              bounds=([9800,0,0,-1e-6],[10200,1,1,1e-6]))
#coef = [9900, 0.9257834309e-3, 3.066721614e-4, -4.482710162e-7]
plt.plot(volts, temps, 'b.')
minT = min(volts)
maxT = max(volts)
funcVolts = [minT + i * (maxT - minT) / 1000 for i in range(0, 1000)]
funcTemps = [1 / tempF(v, *coef) - 273.15 for v in funcVolts]
plt.plot(funcVolts, funcTemps, 'r')

print(str(coef))

srpms = window_fit(times, rpms)
#plt.plot(times, periods, 'b.')
#plt.plot(times, rpms, 'b.')
#plt.plot(times, srpms, 'r')
plt.show()


