#export IDEAL_DIR="../ns2-cap-estimate-scripts/input_for_ideal-32"
export IDEAL_DIR="/home/lavanyaj/waterfilling/input_for_ideal-144"
link_file="links-100.txt"
short_flow_bytes=1000000
short_flow_prio=1
max_sim_time=2.0

FILES=$IDEAL_DIR/input*
mkdir -p input_files
for f in $FILES
do
  echo "Processing $f file..."
  filename=$(basename "$f")
  infile="input_files/sorted-${filename}"
  echo $infile
  echo "Sorting $f file..."
  cmd="sort -g -k3,3 ${f} > input_files/sorted-${filename} 2> /dev/null"
  eval $cmd
  echo "Number of lines in ${infile} ..."
  wc -l $infile
  # take action on each file. $f store current file name
  #cat $f
done

mkdir -p temp_files

FILES=input_files/sorted*
mkdir -p output_files
pids=""
for f in $FILES ; do
  echo "Processing $f file..."
  filename=$(basename "$f")
  outfile="output_files/fct-sorted-${filename}"
  tempout="temp_files/out-${filename}"
  temperr="temp_files/err-${filename}"
  echo $infile
  echo "Running ideal for $f file..."
  cmd="./wsim ${f} ${outfile} ${link_file} ${short_flow_bytes} ${short_flow_prio} ${max_sim_time} 1> $tempout 2> $temperr &"
  echo $cmd
  eval $cmd
  pids="$pids $!"
done

for pid in $pids; do
    echo "Waiting for $pid"
    wait $pid
done
 
