#!/bin/bash 
set +euxo pipefail
while read file
do 
    clang-format -style=file  -i "$file"
    sed -i 's/^\}/\n\}/g' "$file"
done < <(find -name "*cpp" -o -name "*.h" |grep -v build |grep -v "3rdparty/getopt" )  
