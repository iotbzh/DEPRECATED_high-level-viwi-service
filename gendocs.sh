#!/bin/bash

SCRIPT=$(basename $BASH_SOURCE)

function usage() {
	cat <<EOF >&2
Usage: $SCRIPT [options] [pdf|serve|doxygen]

Options:
   --debug
      enable debug when generating pdf or html documentation
   -d|--dry
      dry run
   -h|--help
      get this help

Example:
	$SCRIPT pdf

EOF
	exit 1
}

function info() {
	echo "$@" >&2
}

#default values
DEBUG_FLAG=""
DRY=""
DO_ACTION=""
OUT_DIR=./build

[[ $? != 0 ]] && usage
while [ $# -gt 0 ]; do
	case "$1" in
		--debug) DEBUG_FLAG="--log=debug --debug";;
		-d|--dry) DRY=echo;;
		-h|--help) usage;;
        pdf | serve | doxygen) DO_ACTION=$1;;
		--) break;;
	esac
    shift
done

cd $(dirname $0)
ROOTDIR=`pwd -P`

# Create out dir if needed
[ -d $OUT_DIR ] || mkdir -p $OUT_DIR

if [ "$DO_ACTION" = "pdf" -o "$DO_ACTION" = "serve" ]; then
    GITBOOK=`which gitbook`
    [ "$?" = "1" ] && { echo "You must install gitbook first, using: sudo npm install -g gitbook-cli"; exit 1; }

    EBCONV=`which ebook-convert`
    [ "$?" = "1" ] && { echo "You must install calibre first, using: 'sudo apt install calibre' or refer to https://calibre-ebook.com/download"; exit 1; }

    if [ "$DO_ACTION" = "pdf" ]; then
	    OUTFILE=$OUT_DIR/LowLevelCanBinding_Guide.pdf
        $DRY $GITBOOK pdf $ROOTDIR $OUTFILE $DEBUG_FLAG
        [ "$?" = "0" ] && echo "PDF has been successfully generated in $OUTFILE"
    else
        $DRY $GITBOOK serve $DEBUG_FLAG
    fi

elif [ "$DO_ACTION" = "doxygen" ]; then
    $DRY cd $OUT_DIR && cmake .. && make doxygen $ROOTDIR/Doxyfile

else
    echo "Unknown action !"
    usage
fi
