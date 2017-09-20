#!/bin/bash
DOCS_DIR=$(cd $(dirname $0)/.. && pwd)
BOOKFILE=$DOCS_DIR/../book.json

TITLE=$(grep  '"title":' $BOOKFILE | cut -d'"' -f 4)
SUBTITLE=$(grep  '"subtitle":' $BOOKFILE | cut -d'"' -f 4)
VERSION="Version $(grep  '"version":' $BOOKFILE | cut -d'"' -f 4)"
DATE=$(grep  '"published":' $BOOKFILE | cut -d'"' -f 4)

[ -z "$TITLE" ] && { echo "Error TITLE not set!" ; exit 1; }
[ -z "$VERSION" ] && { echo "Error VERSION not set!" ; exit 1; }
[ -z "$DATE" ] && { echo "Error DATE not set!" ; exit 1; }


cat $(dirname $0)/cover.svg | sed -e "s/{title}/$TITLE/g" \
    -e "s/font-size:87.5px/font-size:54px/g" \
    -e "s/{subtitle}/$SUBTITLE/g" \
    -e "s/font-size:62.5px/font-size:40px/g" \
    -e "s/{version}/$VERSION/g" \
    -e "s/{date}/$DATE/g" \
    > /tmp/cover.svg

# use  imagemagick convert tool  (cover size must be 1800x2360)
convert -resize "1600x2160!" -border 100 -bordercolor white -background white \
    -flatten -quality 100 /tmp/cover.svg $DOCS_DIR/cover.jpg

convert -resize "200x262!" $DOCS_DIR/cover.jpg $DOCS_DIR/cover_small.jpg
