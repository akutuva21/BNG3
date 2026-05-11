def factorial(x):
    temp = x
    acc = 1
    while temp > 0:
        acc *= temp
        temp -= 1
    return acc


def comb(x, y, exact=True):
    return factorial(x) / (factorial(y) * factorial(x - y))
