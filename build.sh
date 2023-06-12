#!/bin/sh

npm ci
hugo --minify
./node_modules/.bin/html-beautify -r -f 'public/**/*.html'
