#!/usr/bin/env bash

run()
{
  cat $1 |

  while read line; do

  # se a linha é branca ignore
  [ "$line" ] || continue

  # se a linha começa com # ignore
  expr "$line" : '#' > /dev/null
  if [ $? -eq 0 ]; then
    continue
  fi

  echo $line | sed 's/|//' |
  {
    read file linkname

    # if exist link `$linkname' then remove.
    _l=/usr/local/bin/$linkname
    test -L $_l && echo sudo rm $_l

  } | sh

  done
}

run ./programs.txt
