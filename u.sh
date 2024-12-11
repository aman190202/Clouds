#!/bin/sh

# Compile the program
g++ -O3 -march=native -fopenmp -std=c++17 -Wno-deprecated-declarations main.cpp camera.cpp Cloud.cpp -o cloud
echo "compiled"

# Ask the user for the range
echo "Enter the starting value (n): "
read n
echo "Enter the ending value (m): "
read m

# Validate input
if [ "$n" -eq "$n" ] 2>/dev/null && [ "$m" -eq "$m" ] 2>/dev/null && [ "$n" -le "$m" ]; then
    echo "Valid range: $n to $m"
else
    echo "Invalid range. Please provide valid integers with n <= m."
    exit 1
fi

# Loop through the range and run the program for each value
i=$n
while [ $i -le $m ]; do
    echo "Running cloud with input $i"
    ./cloud $i
    i=$((i + 1))
done

echo "All runs completed."