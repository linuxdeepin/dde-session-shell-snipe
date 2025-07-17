#!/bin/bash
lupdate ./interface ./src ./tests -ts -no-obsolete -locations none translations/dde-session-shell_en.ts
cd plugins/login-gesture
./lupdate.sh
cd ../../

#tx push -s -b m20 # Push the files that need to be translated
#tx pull -s -b m20 # Pull the translated files
