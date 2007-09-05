#!/bin/sh

export SBOX_DIR=/scratchbox
export SBOX_LOGIN=$SBOX_DIR/login

tdir=`mktemp -d test.XXXXXX` || exit 1
status=0

if [ x$1 = x ]; then
	tests="test-*.sh"
else
	tests=$1
fi

echo "*******************************"
echo "testing basic sb2 functionality"
echo "*******************************"
echo

sb2 sh ./init_tests.sh || exit 1

echo
echo OK
echo
echo "*******************************"
echo "running regression tests.."
echo "*******************************"
echo

for test in $tests
do
	name=`head -n1 $test | grep '^#' | cut -c3-`
	if [ x"$name" = x ]
	then
		name=`echo $test | cut -d- -f2- | cut -d. -f1 | tr - ' ' | tr _ ' '`
	fi

	echo -n "Running \"$name\" ... " >&2
	cp $test $tdir
	( cd ${tdir} ; sb2 sh ./$test ) > $tdir/out 2> $tdir/err
	retval=$?

	if [ $retval -eq 0 ]
	then
		echo "OK" >&2
	elif [ $retval -eq 66 ]
	then
		echo "DISABLED" >&2
	else
		echo "FAIL" >&2
		status=1
	fi

	if [ `stat -c %s $tdir/out` -ne 0 ]
	then
		echo -n 'Output:'
		sed 's/^/\t/' $tdir/out
	fi

	if [ `stat -c %s $tdir/err` -ne 0 ]
	then
		echo -n 'Errors:'
		sed 's/^/\t/' $tdir/err
	fi
done

rm -r $tdir
exit $status
