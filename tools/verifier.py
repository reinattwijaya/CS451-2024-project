import sys

file_loc = "../example/stress_tests/"
N = int(sys.argv[1])

def int_to_str(i):
    if i < 10 and N < 100:
        return "0" + str(i)
    return str(i)

all_map = {}

with open(file_loc + "proc01.output", "r") as f:
    lines = f.readlines()
    print("NUMBER OF SUCCESSFUL DELIVERY: ", len(lines))
    for line in lines:
        line = line.split(' ')
        if line[0] != 'd':
            print("Error in proc" + int_to_str(i) + ".output" + ": FIRST LINE IS NOT d")
            sys.exit(1)
        
        process_id = int(line[1])
        process_message = line[2]
        all_map[process_id] = process_message

for i in range(2, N+1):
    all_messages = []
    with open(file_loc + "proc" + int_to_str(i) + ".output", "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split(' ')
            if line[0] != 'b':
                print("Error in proc" + int_to_str(i) + ".output" + "FIRST LINE IS NOT b")
                sys.exit(1)
            
            all_messages.append(line[1])
    for msg in all_map[i]:
        if msg not in all_messages:
            print("Error in proc" + int_to_str(i) + ".output" + "THE MESSAGE IS NOT BROADCASTED")
            sys.exit(1)

print("Everything is correct!")


