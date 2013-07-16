## Taken from http://johnstachurski.net/lectures/more_numpy.html
## Author: John Stachurski

## (c) John Stachurski, All Rights Reserved
## This file is NOT to be distributed with QoF, and not subject to the 
## QoF licensing terms.

import numpy as np
import matplotlib.pyplot as plt

class ecdf:

    def __init__(self, observations):
        "Parameters: observations is a NumPy array."
        self.observations = observations

    def __call__(self, x): 
        try:
            return (self.observations <= x).mean()
        except AttributeError:
            # self.observations probably needs to be converted to
            # a NumPy array
            self.observations = np.array(self.observations)
            return (self.observations <= x).mean()

    def plot(self, a=None, b=None): 
        if a == None:
            # Set up a reasonable default
            a = self.observations.min() - self.observations.std()
        if b == None:
            # Set up a reasonable default
            b = self.observations.max() + self.observations.std()
        X = np.linspace(a, b, num=100)
        f = np.vectorize(self.__call__)
        plt.plot(X, f(X))
        plt.show()