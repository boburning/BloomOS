#!/bin/sh
set -eu

mode=${1:-}
case "$mode" in
    build|static-analysis) ;;
    *) echo "usage: ci-scope.sh build|static-analysis" >&2; exit 2 ;;
esac

targets=""
full=0

add_target() {
    case " $targets " in
        *" $1 "*) ;;
        *) targets="${targets}${targets:+ }$1" ;;
    esac
}

while IFS= read -r path; do
    [ -n "$path" ] || continue
    case "$path" in
        .github/workflows/build.yml|.github/workflows/test.yml|.gitmodules|Makefile|\
        src/common/*|include/*|third-party/*|tools/checkout-submodules.sh|tools/ci-scope.sh)
            full=1
            ;;
        src/bloomUi/*)
            if [ "$mode" = build ]; then
                add_target bloomShell
            else
                add_target src/bloomUi
                add_target src/bloomShell
            fi
            ;;
        src/bloomShell/*)
            if [ "$mode" = build ]; then add_target bloomShell; else add_target src/bloomShell; fi
            ;;
        src/bloomRa/*)
            if [ "$mode" = build ]; then add_target bloomRa; else add_target src/bloomRa; fi
            ;;
        src/bloomLibrary/*)
            if [ "$mode" = build ]; then
                add_target bloomLibrary
                add_target bloomShell
            else
                add_target src/bloomLibrary
                add_target src/bloomShell
            fi
            ;;
        src/bloomLaunch/*|src/bloomGameId/*)
            component=$(printf '%s\n' "$path" | cut -d/ -f2)
            if [ "$mode" = build ]; then
                add_target "$component"
                add_target bloomShell
            else
                add_target "src/$component"
                add_target src/bloomShell
            fi
            ;;
        src/*)
            full=1
            ;;
        build/dependencies.lock|build/legacy-manifest.json|static/*|test/*|docs/*)
            ;;
        *)
            full=1
            ;;
    esac
done

if [ "$full" -eq 1 ]; then
    echo full
elif [ -n "$targets" ]; then
    echo "$targets"
else
    echo none
fi
