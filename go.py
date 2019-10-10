#!/usr/bin/env python3

# APPROXIMATELY GOOD IS ./go.py 0x6b00 0xf000

if __name__ == '__main__':
    import requests as req
    import sys
    from time import sleep
    from itertools import chain

    if len(sys.argv) != 3:
        print('go.py <start> <end>')
        exit(1)
    start = eval(sys.argv[1])
    end = eval(sys.argv[2])
    delta = 0x70 # if end >= start else -0x15

    ses = req.Session()

    for i in chain(range(start, end + delta, delta), range(end, start - delta, -delta)):
        val = f'{hex(i)[2:]:>010}'
        while True:
            try:
                text = ses.get(f'http://192.168.13.37/set_value/Blink/A/{val}').text
                break
            except Exception as _:
                pass
        print(val, '==>', text)
        # sleep(0.025)
