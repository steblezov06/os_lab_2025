#!/bin/bash

echo "всего агрументов: $#"
sum=0
count=0
for arg in "$@"; do
    ((sum += arg))
    ((count++))
done
average=$((sum / count));
echo "среднее арифметическое: $average"
