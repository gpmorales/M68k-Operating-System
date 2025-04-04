#!/bin/bash
#set -x

TDIR=/home/cs580000/share/hoca/support/part2/testhoca

if [ $# -ne 2 ];
then
	echo "Usage: testhocaauto  <min_seed> <max_seed>"
	exit 1;
fi

date
echo "Seeds: $1 -> $2"

for (( seed = $1; seed <= $2; seed++ ))
do
	ERRORS=0
	NUM_FILES=0
	
	# Run  emacsim
        $TDIR/expecthoca.sh $seed 1>/dev/null

	# Interpret results
	if [ -e termout0 ]; then
		diff termout0 $TDIR/termout0.ok 1>/dev/null
                TRES=`echo $?`         
                if [ $TRES = 1 ]
                then
                   echo ERROR: termout0
                   cat termout0
                   echo "---------------------------------"
                   cat $TDIR/termout0.ok
                   echo "---------------------------------"
                fi
		let "ERRORS = $ERRORS + $TRES"
		let "NUM_FILES = $NUM_FILES + 1"
	fi
	if [ -e termout1 ]; then
		diff termout1 $TDIR/termout1.ok 1>/dev/null
                TRES=`echo $?`         
                if [ $TRES = 1 ]
                then
                   echo ERROR: termout1
                   cat termout1
                   echo "---------------------------------"
                   cat $TDIR/termout1.ok
                   echo "---------------------------------"
                fi
		let "ERRORS = $ERRORS + $TRES"
		let "NUM_FILES = $NUM_FILES + 1"
	fi
	if [ -e termout2 ]; then
		diff termout2 $TDIR/termout2.ok 1>/dev/null
                TRES=`echo $?`         
                if [ $TRES = 1 ]
                then
                   echo ERROR: termout2
                   cat termout2
                   echo "---------------------------------"
                   cat $TDIR/termout2.ok
                   echo "---------------------------------"
                fi
		let "ERRORS = $ERRORS + $TRES"
		let "NUM_FILES = $NUM_FILES + 1"
	fi
	if [ -e termout3 ]; then
		diff termout3 $TDIR/termout3.ok 1>/dev/null
                TRES=`echo $?`         
                if [ $TRES = 1 ]
                then
                   echo ERROR: termout3
                   cat termout3
                   echo "---------------------------------"
                   cat $TDIR/termout3.ok
                   echo "---------------------------------"
                fi
		let "ERRORS = $ERRORS + $TRES"
		let "NUM_FILES = $NUM_FILES + 1"
	fi
	if [ -e termout4 ]; then
		diff termout4 $TDIR/termout4.ok 1>/dev/null
                TRES=`echo $?`         
                if [ $TRES = 1 ]
                then
                   echo ERROR: termout4
                   cat termout4
                   echo "---------------------------------"
                   cat $TDIR/termout4.ok
                   echo "---------------------------------"
                fi
		let "ERRORS = $ERRORS + $TRES"
		let "NUM_FILES = $NUM_FILES + 1"
	fi

	if [[ $ERRORS -eq 0 ]] && [[ $NUM_FILES -eq 5 ]]; then
		echo -n "."
	else
		echo "(errors: $ERRORS, files: $NUM_FILES/5)  seed: $seed "
                echo "==================================================== "
	fi	
	
	# Remove output files
	if [ -e "termout0" ]; then
		\rm -f termout0
	fi
	if [ -e "termout1" ]; then
	\rm -f termout1
	fi
	if [ -e "termout2" ]; then
		\rm -f termout2
	fi
	if [ -e "termout3" ]; then
		\rm -f termout3
	fi
	if [ -e "termout4" ]; then
		\rm -f termout4
	fi
	if [ -e "printer0" ]; then
		\rm -f printer0
	fi
	if [ -e "core" ]; then
		\rm -f core
	fi

	# Remove files from the /tmp/
	#\rm /tmp/emacsim* > /dev/null

#       sleep 1	
done

echo ""
date
