#!/bin/sh
# Every toy Spyro's Adventure accepts, one isolated run each (docs/runtime/host/figures.md, "Every SSA figure type").
# The harness itself is tools/regress/figures.sh, which runs the same per-toy run for all three games; this wrapper keeps
# the old command line and its results.tsv.
#
#   tools/regress/ssa_figures.sh <SSA disc .iso> <work dir> [--variants] [--seconds N] [<type>[:<variant hex>] ...]
#
# The toys are the catalogue's `ssa` accept list (resources/collection/catalogue.json; 54 types); --variants runs instead
# every variant the catalogue lists for them that has a tag of its own. With no <type> arguments it runs them all (~80 s
# each); arguments run just those. Results: <work dir>/results.tsv (label, verdict, detail). Never touches the player's
# files; one game at a time, `nice`.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
exec "$root/tools/regress/figures.sh" ssa "$@"
