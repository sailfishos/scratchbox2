# Fakeroot statting works
set -e
fname=test-stat
fakeroot /bin/sh -s <<EOF
touch $fname
chown 27 $fname
[ \`stat -c%u $fname\` == 27 ]
EOF
