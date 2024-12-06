import sys

file_loc = "../example/stress_tests/"
N = int(sys.argv[1])

def int_to_str(i):
    if i < 10:
        return "0" + str(i)
    return str(i)

max_broadcasted = {}
max_delivered = {}
all_delivered = {}

for i in range(1, N+1):
    max_broadcasted[i] = 1

for i in range(1, N+1):
    for j in range(1, N+1):
        max_delivered[(i, j)] = 1
    with open(file_loc + "proc" + int_to_str(i) + ".output", "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split(' ')
            if line[0] == 'b':
                line[1] = line[1][:-1]
                if(int(line[1]) < max_broadcasted[i]):
                    print("Error in proc" + int_to_str(i) + ".output" + ": BROADCASTED TWICE OR BROADCASTED IN WRONG ORDER")
                    sys.exit(1)
                max_broadcasted[i] = int(line[1])
            elif line[0] == 'd':
                line[2] = line[2][:-1]
                all_delivered[(int(line[1]), int(line[2]))] = True

for i in range(1, N+1):
    msg_delivered = 0
    with open(file_loc + "proc" + int_to_str(i) + ".output", "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split(' ')
            if line[0] == 'd':
                msg_delivered += 1
                if(int(line[2]) != max_delivered[(i, int(line[1]))]):
                    print("Error in proc" + int_to_str(i) + ".output" + ": WRONG DELIVERY ORDER OR DUPLICATE DELIVERY")
                    sys.exit(1)
                if(int(line[2]) > max_broadcasted[int(line[1])]):
                    print("Error in proc" + int_to_str(i) + ".output" + ": DELIVERED BEFORE BROADCASTED " + str(max_broadcasted[int(line[1])]) + " in process: " + line[1])
                    sys.exit(1)
                max_delivered[(i, int(line[1]))] += 1
    for key in all_delivered:
        if(max_delivered[(i, key[0])] < key[1]):
            print("Error in proc" + int_to_str(i) + ".output" + ": MISSING DELIVERY OF " + str(key[1]) + " FROM " + str(key[0]))

    print(i, "has broadcasted", max_broadcasted[i], "and delivered", msg_delivered)

print("Everything is correct!")



