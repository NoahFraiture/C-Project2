def sum_byte():
    l = []
    index = 0
    with open("header.tar", "rb") as file:
        while(byte := file.read(1) ):
            val = int.from_bytes(byte, "little")
            if val != 0:
                l.append(val)
    print(f"Somme des bytes du header : {sum(l)}")
    print("Somme bytes checksum : 303")

def octal_int(num:str):
    base = 0
    s = 0
    for i in range(1, len(num)+1):
        b = int(num[-i])
        a = 8**base
        s += int(b * a)
        base+=1
    return s

sum_byte()
print("octal to int : ", octal_int(input("size octal : ")))