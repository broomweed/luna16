import sys
from PIL import Image

if len(sys.argv) < 2:
    print("please supply a file name")
    sys.exit(1)

if '-t' in sys.argv:
    # '-t' for text mode
    binmode = False
    sys.argv = [x for x in sys.argv if x != '-b']
else:
    binmode = True

if len(sys.argv) < 3:
    outfile = sys.argv[1] + '.out'
else:
    outfile = sys.argv[2]

img = Image.open(sys.argv[1])

print("Opened", sys.argv[1])

if img.size != (128, 128):
    print("Wrong image size; should be 128x128")
    sys.exit(1)


if not binmode:
    out = open(outfile, 'w')

    for iy in range(16):
        for ix in range(16):
            print("sprite"+("%1X"%iy)+("%1X"%ix)+":", file=out)
            for y in range(8):
                print("    data ", end='', file=out)
                for x in range(8):
                    print("%1X" % img.getpixel((ix * 8 + x, iy * 8 + y)), end='', file=out)
                print('', file=out)

else:
    out = open(outfile, 'wb')
    arr = bytearray()

    for iy in range(16):
        for ix in range(16):
            print("***", ix, iy)
            for y in range(8):
                for x in range(4):
                    px1 = img.getpixel((ix * 8 + x * 2,     iy * 8 + y)) % 8
                    px2 = img.getpixel((ix * 8 + x * 2 + 1, iy * 8 + y)) % 8

                    #print("Pixel data at", ix * 8 + x * 4, ",", iy * 8 + y, ":", px1)

                    dat = (px1 << 4) | (px2)
                    arr += bytes([dat])

                    s = "{0:02x}".format(dat)
                    s = s.replace('0', ' ')
                    s = s.replace('1', '#')
                    s = s.replace('2', '.')
                    s = s.replace('3', '*')
                    s = s[0] + s[0] + s[1] + s[1]
                    print(s, end='')
                print()
    out.write(arr)
