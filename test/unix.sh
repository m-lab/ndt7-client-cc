#!/bin/sh
set -e

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <cmake-binary-dir>" 1>&2
  exit 1
fi

ndt_server=$(curl -s https://mlab-ns.appspot.com/ndt \
  | python -c 'import sys, json; print json.load(sys.stdin)["fqdn"]')

neubot_server=$(curl -s https://mlab-ns.appspot.com/neubot \
  | python -c 'import sys, json; print json.load(sys.stdin)["fqdn"]')

set +x
$1/client --upload $ndt_server
$1/client --download-ext --json $neubot_server
$1/client --download $ndt_server