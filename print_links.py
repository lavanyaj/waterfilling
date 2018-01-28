end=144
link_rate=100
num_tors = int(round(end/16))
num_aggs= 4
for i in range(0, end):
    tor = int(i/16) + 144
    print "%d %d %f" % (i, tor, link_rate)
    print "%d %d %f" % (tor, i, link_rate)
    pass

for tor in range(144, (144+num_tors)):
    for agg in range(153, 153+num_aggs):
        print "%d %d %f" % (tor, agg, 4*link_rate)
        print "%d %d %f" % (agg, tor, 4*link_rate)
        pass
    pass

