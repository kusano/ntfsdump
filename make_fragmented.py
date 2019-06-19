for i in range(1000):
  with open("stage3_a.txt", "wb" if i==0 else "ab") as f:
    print>>f, i, "a"*1024
  with open("stage3_b.txt", "wb" if i==0 else "ab") as f:
    print>>f, i, "b"*1024

for i in range(10000):
  with open("stage4_a.txt", "wb" if i==0 else "ab") as f:
    print>>f, i, "a"*1024
  with open("stage4_b.txt", "wb" if i==0 else "ab") as f:
    print>>f, i, "b"*1024
