#!/bin/bash

function usage(){

    echo "Usage: createNewTrigger.sh: <inputDir> <replace string> <with string>"

}

if [ "$1" == "" ] || [ "$2" == "" ] || [ "$3" == "" ];
then
    usage
    exit 1
fi

IN_DIR=$1
OLD_STRING=$2
OLD_STRING_LOWER=`echo $OLD_STRING | tr '[:upper:]' '[:lower:]'`
NEW_STRING=$3
NEW_STRING_LOWER=`echo $NEW_STRING | tr '[:upper:]' '[:lower:]'`

echo "Replacing \"$OLD_STRING\" with \"$NEW_STRING\", and \"$OLD_STRING_LOWER\" with \"$NEW_STRING_LOWER\""

if [ ! -d $IN_DIR ];then
    echo "IN_DIR $IN_DIR does not exist"; 
    exit 1;
fi

temp_file="/tmp/createNewTrigger.tmp.$$"



for file in ${IN_DIR}/*${OLD_STRING}*;
do
#    echo file $file;
    new_file=`echo $file | sed "s/$OLD_STRING/$NEW_STRING/g"`
#    echo file $file new_file $new_file
    cp $file $new_file
    sed "s/$OLD_STRING/$NEW_STRING/g" "$new_file" > $temp_file && mv $temp_file "$new_file"
    sed "s/$OLD_STRING_LOWER/$NEW_STRING_LOWER/g" "$new_file" > $temp_file && mv $temp_file "$new_file"


    
done


exit 0