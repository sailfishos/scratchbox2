# Primitive floating-point operations

set -e
CODE=floattest
cat > $CODE.c <<EOF
int main(void) {
  float a, b;
  a = 0.1f;
  b = 0.8f;
  return a<b?0:1;
}
EOF
gcc $CODE.c -o $CODE
./$CODE
