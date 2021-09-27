# Fakeroot host-side id is root
set -e
fakeroot id -u | grep -q ^0$
