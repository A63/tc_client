#!/bin/sh
spinnerpos='+281+341
+270+369
+281+397
+309+408
+337+397
+348+369
+337+341
+309+330'
{
  for frame in `seq 1 16`; do
    echo '( ( utilities/gtk/camplaceholder.xcf -layers Merge )'
    for spinner in `seq 1 8`; do
      pos="`echo "$spinnerpos" | sed -n -e "${spinner}p"`"
      dotframe="`expr '(' "$frame" + "$spinner" '*' 2 ')' '%' 16`"
      if [ "$dotframe" -gt 11 ]; then dotframe=11; fi # TODO: use identify | wc -l to get the max frame ID?
      echo "( -geometry "$pos" "utilities/gtk/spinnerdot.xcf[${dotframe}]" ) -composite"
    done
    echo ')'
  done
  echo '-layers Optimize -delay 100 camplaceholder.gif'
} | xargs convert
