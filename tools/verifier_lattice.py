import sys

file_loc = "../example/stress_tests/"
N = int(sys.argv[1])

def int_to_str(i):
    if i < 10:
        return "0" + str(i)
    return str(i)

proposals = [[] for i in range(N+1)]

for i in range(1, N+1):
    with open(file_loc + "proc" + int_to_str(i) + ".output", "r") as f:
        lines = f.readlines()
        for line in lines:
            cur_proposal = set({})
            line = line.split(' ')
            for l in line:
                if l in cur_proposal:
                    print("DUPLICATE VALUES IN PROCESS: " + int_to_str(i))
                cur_proposal.add(l)
            proposals[i].append(cur_proposal)

for i in range(1, N+1):
    for k in range(len(proposals[i])):
        for j in range(1, N+1):
            if i == j:
                continue
            if not (proposals[i][k].issubset(proposals[j][k]) or proposals[j][k].issubset(proposals[i][k])):
                print("PROPOSAL " + int_to_str(i) + " VIOLATED SPEC WITH PROCESS " + int_to_str(j))

print("Everything is correct!")
