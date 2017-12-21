#!/bin/bash
# shellcheck disable=SC2086

OUTFILENAME="High_Level_ViWi_Service"

SCRIPT=$(basename "${BASH_SOURCE[@]}")

VERSION=$(grep '"version":' "$(dirname "${BASH_SOURCE[@]}")/book.json" | cut -d'"' -f 4)
[ "$VERSION" != "" ] && OUTFILENAME="${OUTFILENAME}_v${VERSION}"


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
ARGS=""

[ $# = 0 ] && usage
while [ $# -gt 0 ]; do
	case "$1" in
		--debug)                DEBUG_FLAG="--log=debug --debug";;
		-d|--dry)               DRY="echo";;
		-h|--help)              usage;;
        pdf | serve | doxygen)  DO_ACTION=$1;;
		--)                     shift; ARGS="$ARGS $*"; break;;
        *)                      ARGS="$ARGS $1";;
	esac
    shift
done

cd "$(dirname "$0")" || exit 1
ROOTDIR=$(pwd -P)

# Create out dir if needed
[ -d $OUT_DIR ] || mkdir -p $OUT_DIR

if [ "$DO_ACTION" = "pdf" ] || [ "$DO_ACTION" = "serve" ]; then

    GITBOOK=$(which gitbook)
    [ "$?" = "1" ] && { echo "You must install gitbook first, using: sudo npm install -g gitbook-cli"; exit 1; }

    which ebook-convert > /dev/null 2>&1
    [ "$?" = "1" ] && { echo "You must install calibre first, using: 'sudo apt install calibre' or refer to https://calibre-ebook.com/download"; exit 1; }

    [ ! -d "$ROOTDIR/node_modules" ] && $GITBOOK install

    if [ "$DO_ACTION" = "pdf" ]; then

        # Update cover when book.json has been changed
        [[ $ROOTDIR/book.json -nt $ROOTDIR/docs/cover.jpg ]] && { echo "Update cover files"; $ROOTDIR/docs/resources/make_cover.sh || exit 1; }

	    OUTFILE=$OUT_DIR/$OUTFILENAME.pdf
        if $DRY $GITBOOK pdf $ROOTDIR $OUTFILE $DEBUG_FLAG $ARGS; then
            echo "PDF has been successfully generated in $OUTFILE"
        fi
    else
        $DRY $GITBOOK serve $DEBUG_FLAG $ARGS
    fi

elif [ "$DO_ACTION" = "doxygen" ]; then
    $DRY cd $OUT_DIR && cmake .. && make doxygen $ROOTDIR/Doxyfile

else
    echo "Unknown action !"
    usage
fi
