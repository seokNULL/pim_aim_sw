#!/bin/sh

# Check if the number of trials and size are provided
if [ -z "$1" ] || [ -z "$2" ]; then
  echo "Usage: $0 <number_of_trials> <size>"
  exit 1
fi

# Number of trials and size
X=$1
SIZE=$2
LOG_FILE="log_trial_$X"
TEMP_FILE="temp_total_time.txt"
TOTAL_TIME=0

# Remove previous log file and temp file if they exist
if [ -f "$LOG_FILE" ]; then
  rm "$LOG_FILE"
fi
if [ -f "$TEMP_FILE" ]; then
  rm "$TEMP_FILE"
fi

# Execute the command X times
i=1
while [ $i -le $X ]
do
  echo "Trial $i"
  ./mem-cpy -d cdma -s $SIZE >> "$LOG_FILE"
  i=$((i + 1))
done

# Extract execution times and calculate the total
grep "CDMA Memcpy time:" "$LOG_FILE" | while read -r line; do
  TIME=$(echo $line | grep -o -E '[0-9]+')
  echo "Extracted time: $TIME"  # Debugging statement
  echo $TIME >> "$TEMP_FILE"
done

# Sum the times from the temp file
while read -r time; do
  TOTAL_TIME=$((TOTAL_TIME + time))
done < "$TEMP_FILE"

# Calculate average time
if [ $X -gt 0 ]; then
  AVERAGE_TIME=$((TOTAL_TIME / X))
else
  AVERAGE_TIME=0
fi

# Calculate transaction size in bytes
TRANSACTION_SIZE=$((SIZE * 4))

# Output the average time and transaction size
echo "Total time: $TOTAL_TIME nanoseconds"  # Debugging statement
echo "Average CDMA Memcpy time over $X trials: $AVERAGE_TIME nanoseconds"
echo "Transaction size: $SIZE * 4B = $TRANSACTION_SIZE bytes"

# Clean up temporary file
rm "$TEMP_FILE"
