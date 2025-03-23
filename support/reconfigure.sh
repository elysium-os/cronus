for f in $(find . -name configure.ac -o -name configure.in -type f | sort); do
    echo "* autotools regen in '$(dirname $f)'..."
    ( cd "$(dirname "$f")" && autoreconf -fvi "$@" )
done