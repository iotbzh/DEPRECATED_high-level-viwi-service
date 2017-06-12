#!/bin/bash

cat cover.svg | sed -e 's/{title}/Low Level CAN binding/' \
    -e 's/font-size:87.5px/font-size:50px/g' \
    -e 's/{subtitle}//g' \
    -e 's/{version}/Version 1.0/g' \
    -e 's/{date}/March 2017/g' \
    > /tmp/cover.svg

# use  imagemagick convert tool  (cover size must be 1800x2360)
convert -resize "1600x2160!" -border 100 -bordercolor white -background white \
    -flatten -quality 100 /tmp/cover.svg ../cover.jpg

convert -resize "200x262!" ../cover.jpg ../cover_small.jpg
