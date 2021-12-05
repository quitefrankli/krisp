#!/bin/bash

set -e 

get_new_filename () {
	MAX_NUM=0
	for file in bin/*
	do
		if [[ $file == *"shared_lib"*".dll" ]]
		then
			NEW_NUM=$(echo $file | tr -cd '[[:digit:]]')
			if [[ $NEW_NUM == "" ]]
			then
				continue
			fi
			if (( $NEW_NUM > $MAX_NUM ))
			then
				MAX_NUM=$NEW_NUM
			fi
		fi
	done

	MAX_NUM=$(($MAX_NUM + 1))
	new_filename=shared_lib$MAX_NUM.dll
}

msbuild.exe shared_lib/shared_lib.sln
get_new_filename
cp bin/shared_lib.dll bin/$new_filename
echo created $new_filename