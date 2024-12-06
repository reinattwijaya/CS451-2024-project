import sys

file_loc = "../example/stress_tests/"
N = int(sys.argv[1])

def int_to_str(i):
    if i < 10:
        return "0" + str(i)
    return str(i)

all_map = {}

with open(file_loc + "proc01.output", "r") as f:
    lines = f.readlines()
    print("NUMBER OF SUCCESSFUL DELIVERY: ", len(lines))
    for line in lines:
        line = line.split(' ')
        if line[0] != 'd':
            print("Error in proc01"+ ".output" + ": FIRST LINE IS NOT d")
            sys.exit(1)
        
        process_id = int(line[1])
        process_message = line[2]
        if process_id not in all_map:
            all_map[process_id] = {}
            all_map[process_id][process_message] = True
        else:
            if process_message in all_map[process_id]:
                print("Error in proc01" + ".output" + ": DUPLICATE MESSAGE: " + process_message)
                sys.exit(1)
            all_map[process_id][process_message] = True

for i in range(2, N+1):
    all_messages = {}
    with open(file_loc + "proc" + int_to_str(i) + ".output", "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split(' ')
            if line[0] != 'b':
                print("Error in proc" + int_to_str(i) + ".output" + "FIRST LINE IS NOT b")
                sys.exit(1)
            all_messages[line[1]] = True
    for msg in all_map[i]:
        if msg not in all_messages:
            print("Error in proc" + int_to_str(i) + ".output" + "THE MESSAGE IS NOT BROADCASTED" + msg)
            sys.exit(1)
    print(i, "has sent", len(all_map[i]))

print("Everything is correct!")



