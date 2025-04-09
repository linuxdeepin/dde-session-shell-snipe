#!/bin/bash
lupdate ./ -ts -no-obsolete -locations none translations/dde-session-shell_en.ts
#tx push -s -b m20 # Push the files that need to be translated
#tx pull -s -b m20 # Pull the translated files
