# c = 1
# while 1:
#     print('Shrehan')
#     print(c)
#     c += 1


# c = 1
# while 1:
#     if 0:
#         if 0:
#             print(1)
#         else:
#             if 0:
#                 print(2)
#             else:
#                 print(3)
#     else:
#         print(4)

#         if 0:
#             if 0:
#                 if 0:
#                     print(5)
#                 else:
#                     if 0:
#                         if 0:
#                             print(6)
#                         else:
#                             if 0:
#                                 print(7)
#                             else:
#                                 print(8)
#                     else:
#                         print(9)
#             else:
#                 if 0:
#                     print(10)
#                 else:
#                     if 1:
#                         if 0:
#                             print(11)
#                         else:
#                             print(12)
#                     else:
#                         print(13)
#         else:
#             if 0:
#                 print(14)
#             else:
#                 if 1:
#                     if 1:
#                         if 0:
#                             print(15)
#                         else:
#                             print(16)
#                     else:
#                         print(17)
#                 else:
#                     print(18)

#     print(c)
#     c += 1

# c = 1
# while 1:
#     a = 5
#     print(c)
#     c += 1

import time


c = 0

start = time.perf_counter()
while time.perf_counter() - start < 5:
    a = 1
    b = a + 2
    cc = b * 3
    d = cc - 4
    e = d + a
    c = c + 1

print(c)
