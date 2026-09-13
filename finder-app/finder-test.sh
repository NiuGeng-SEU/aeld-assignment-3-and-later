#!/bin/sh
# Tester script for assignment 1 and assignment 2

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data

# 1. 兼容板载固化路径 /etc/finder-app/conf 与宿主机本地路径
if [ -d "/etc/finder-app/conf" ]; then
	CONFDIR="/etc/finder-app/conf"
elif [ -d "conf" ]; then
	CONFDIR="conf"
else
	CONFDIR="../conf"
fi

username=$(cat "${CONFDIR}/username.txt")

if [ $# -lt 3 ]
then
	echo "Using default value ${WRITESTR} for string to write"
	if [ $# -lt 1 ]
	then
		echo "Using default value ${NUMFILES} for number of files to write"
	else
		NUMFILES=$1
	fi
else
	NUMFILES=$1
	WRITESTR=$2
	WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "${WRITEDIR}"

# 2. 读取 assignment.txt
assignment=$(cat "${CONFDIR}/assignment.txt")

if [ "$assignment" != 'assignment1' ]
then
	mkdir -p "$WRITEDIR"

	if [ -d "$WRITEDIR" ]
	then
		echo "$WRITEDIR created"
	else
		exit 1
	fi
fi

# 3. 去掉 ./，直接通过 PATH 寻找 writer
for i in $( seq 1 $NUMFILES)
do
	writer "$WRITEDIR/${username}$i.txt" "$WRITESTR"
done

# 4. 去掉 ./，直接通过 PATH 寻找 finder.sh
OUTPUTSTRING=$(finder.sh "$WRITEDIR" "$WRITESTR")

# 5. 将 finder 输出结果落盘到 /tmp/assignment4-result.txt（作业硬性要求）
echo "${OUTPUTSTRING}" > /tmp/assignment4-result.txt

# remove temporary directories
rm -rf /tmp/aeld-data

set +e
echo ${OUTPUTSTRING} | grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
	echo "success"
	exit 0
else
	echo "failed: expected  ${MATCHSTR} in ${OUTPUTSTRING} but instead found"
	exit 1
fi
