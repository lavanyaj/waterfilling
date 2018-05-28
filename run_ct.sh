# example ./run_ct.sh input_for_ct/ct-flows-input.tcl
INPUT=$1
./wsim $INPUT out.txt links-100.txt 100 1 1> tmp.out 2> tmp.err
grep RATE_CHANGE tmp.out 1> wf-ct-rate.txt
