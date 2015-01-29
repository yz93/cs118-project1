#!/bin/bash

# 20-grading.sh

rm -f ./text.txt
rm -f ./tools/text.txt

cp ./tools/text.txt.bak ./tools/text.txt

echo "Run 5 test cases..."

./tools/sbt-tracker 60207 ./tools/test-2.torrent &

sleep 1

./tools/sbt-peer 11111 ./tools/test-2.torrent ./tools/ SIMPLEBT.TEST.111111 2>/dev/null &

sleep 1

./build/simple-bt 60207 ./tools/test-2.torrent > /dev/null 2>&1 &

sleep 50

killall sbt-tracker > /dev/null 2>&1

sleep 1

cmp ./text.txt ./tools/text.txt

if [ $? -eq 0 ] ; then
    echo "[Test case 5] Passed"
else
    echo "[Test case 5] Failed (In consistent file)"
fi

rm -f ./text.txt
rm -f ./tools/text.txt
