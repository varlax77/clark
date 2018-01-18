#!/bin/bash

#
# Prepare database for CLARK, CLAssifier based on Reduced K-mers.
# 
if [ $# -ne 4 ]; then
	echo "Usage: $0 <DB directory path> <taxondata dir> <DB name> <taxonomy rank>"
	exit
fi

#EXE=$(dirname "$0")
EXE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DBDR=$1
TAXDR=$2
DB=$3
RANK=$4

if [ -s $TAXDR/nucl_gb.accession2taxid.7z ]; then
        CAT_ACC1="7z x -so $TAXDR/nucl_gb.accession2taxid.7z"
fi
if [ -s $TAXDR/nucl_gb.accession2taxid.gz ]; then
        CAT_ACC1="gunzip -c $TAXDR/nucl_gb.accession2taxid.gz"
fi
if [ -s $TAXDR/nucl_gb.accession2taxid ]; then
        CAT_ACC1="cat $TAXDR/nucl_gb.accession2taxid"
fi
if [ -s $TAXDR/nucl_wgs.accession2taxid.7z ]; then
        CAT_ACC2="7z x -so $TAXDR/nucl_wgs.accession2taxid.7z"
fi
if [ -s $TAXDR/nucl_wgs.accession2taxid.gz ]; then
        CAT_ACC2="gunzip -c $TAXDR/nucl_wgs.accession2taxid.gz"
fi
if [ -s $TAXDR/nucl_wgs.accession2taxid ]; then
        CAT_ACC2="cat $TAXDR/nucl_wgs.accession2taxid"
fi


if [ -s $TAXDR/taxdump.tar.gz ]; then
	echo "Uncompressing taxonomy data files... "
	pushd $TAXDR 
	tar -zxf taxdump.tar.gz
	popd
fi

if [ ! -s $TAXDR/nodes.dmp ] || [ ! -s $TAXDR/merged.dmp ]; then
	echo "No taxonomy data found in $TAXDR, aborting"
	exit 1
fi

if [ ! -f $DBDR/.$DB ]; then
	echo "Failed to find list of library files: $DBDR/.$DB, aborting"
        exit 2
fi
rm -f $DBDR/.tmp $DBDR/targets.txt $DBDR/.settings $DBDR/files_excluded.txt

if [ ! -s $DBDR/.$DB.fileToAccssnTaxID ] ; then
	echo "Re-building $DB.fileToAccssnTaxID"
	($CAT_ACC1; $CAT_ACC2) | $EXE/getAccssnTaxID $DBDR/.$DB $TAXDR/merged.dmp > $DBDR/.$DB.fileToAccssnTaxID
fi
if [ ! -s $DBDR/.$DB.fileToTaxIDs ]; then
	echo "Retrieving taxonomy nodes for each sequence based on taxon ID..."
	$EXE/getfilesToTaxNodes $TAXDR/nodes.dmp $DBDR/.$DB.fileToAccssnTaxID > $DBDR/.$DB.fileToTaxIDs
fi
if [ -s $DBDR/.$DB.fileToTaxIDs ]; then 
	pushd $DBDR
	$EXE/getTargetsDef $DBDR/.$DB.fileToTaxIDs $RANK >> $DBDR/targets.txt
	popd
fi
echo "-T $DBDR/targets.txt" > $DBDR/.settings
if [ ! -d $DBDR/$DB ]; then
	echo "Creating directory to store discriminative k-mers: $DBDR/$DB"
	mkdir -m 775 $DBDR/$DB
fi
echo "-D $DBDR/$DB/" >> $DBDR/.settings
