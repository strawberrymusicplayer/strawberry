#!/bin/bash 
set +euxo pipefail
while read file
do 
    clang-format -style=file  -i $file
done < <(find -name "*cpp" -o -name "*.h" |grep -v build )  
